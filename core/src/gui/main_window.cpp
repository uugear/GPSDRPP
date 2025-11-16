#include <gui/main_window.h>
#include <gui/gui.h>
#include "imgui.h"
#include <stdio.h>
#include <thread>
#include <complex>
#include <gui/widgets/waterfall.h>
#include <gui/widgets/frequency_select.h>
#include <signal_path/iq_frontend.h>
#include <gui/icons.h>
#include <gui/widgets/bandplan.h>
#include <gui/style.h>
#include <config.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/menus/source.h>
#include <gui/menus/display.h>
#include <gui/menus/bandplan.h>
#include <gui/menus/sink.h>
#include <gui/menus/vfo_color.h>
#include <gui/menus/module_manager.h>
#include <gui/menus/theme.h>
#include <gui/dialogs/credits.h>
#include <filesystem>
#include <signal_path/source.h>
#include <gui/dialogs/loading_screen.h>
#include <gui/colormaps.h>
#include <gui/widgets/snr_meter.h>
#include <gui/tuner.h>

#define TOOLBAR_BUTTON_SIZE		    50
#define SIDEBAR_WIDTH			    190

void MainWindow::init() {
    LoadingScreen::show("Initializing UI");
    gui::waterfall.init();
    gui::waterfall.setRawFFTSize(fftSize);

    credits::init();

    core::configManager.acquire();
    json menuElements = core::configManager.conf["menuElements"];
    std::string modulesDir = core::configManager.conf["modulesDirectory"];
    std::string resourcesDir = core::configManager.conf["resourcesDirectory"];
    core::configManager.release();

    // Assert that directories are absolute
    modulesDir = std::filesystem::absolute(modulesDir).string();
    resourcesDir = std::filesystem::absolute(resourcesDir).string();

    // Load menu elements
    gui::menu.order.clear();
    for (auto& elem : menuElements) {
        if (!elem.contains("name")) {
            flog::error("Menu element is missing name key");
            continue;
        }
        if (!elem["name"].is_string()) {
            flog::error("Menu element name isn't a string");
            continue;
        }
        if (!elem.contains("open")) {
            flog::error("Menu element is missing open key");
            continue;
        }
        if (!elem["open"].is_boolean()) {
            flog::error("Menu element name isn't a string");
            continue;
        }
        Menu::MenuOption_t opt;
        opt.name = elem["name"];
        opt.open = elem["open"];
        gui::menu.order.push_back(opt);
    }

    gui::menu.registerEntry("Source", sourcemenu::draw, NULL);
    gui::menu.registerEntry("Sinks", sinkmenu::draw, NULL);
    gui::menu.registerEntry("Band Plan", bandplanmenu::draw, NULL);
    gui::menu.registerEntry("Display", displaymenu::draw, NULL);
    gui::menu.registerEntry("Theme", thememenu::draw, NULL);
    gui::menu.registerEntry("VFO Color", vfo_color_menu::draw, NULL);
    gui::menu.registerEntry("Module Manager", module_manager_menu::draw, NULL);

    gui::freqSelect.init();

    // Set default values for waterfall in case no source init's it
    gui::waterfall.setBandwidth(8000000);
    gui::waterfall.setViewBandwidth(8000000);

    fft_in = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * fftSize);
    fft_out = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * fftSize);
    fftwPlan = fftwf_plan_dft_1d(fftSize, fft_in, fft_out, FFTW_FORWARD, FFTW_ESTIMATE);

    sigpath::iqFrontEnd.init(&dummyStream, 8000000, true, 1, false, 1024, 20.0, IQFrontEnd::FFTWindow::NUTTALL, acquireFFTBuffer, releaseFFTBuffer, this);
    sigpath::iqFrontEnd.start();

    vfoCreatedHandler.handler = vfoAddedHandler;
    vfoCreatedHandler.ctx = this;
    sigpath::vfoManager.onVfoCreated.bindHandler(&vfoCreatedHandler);

    flog::info("Loading modules");

    // Load modules from /module directory
    if (std::filesystem::is_directory(modulesDir)) {
        for (const auto& file : std::filesystem::directory_iterator(modulesDir)) {
            std::string path = file.path().generic_string();
            if (file.path().extension().generic_string() != SDRPP_MOD_EXTENTSION) {
                continue;
            }
            if (!file.is_regular_file()) { continue; }
            flog::info("Loading {0}", path);
            LoadingScreen::show("Loading " + file.path().filename().string());
            core::moduleManager.loadModule(path);
        }
    }
    else {
        flog::warn("Module directory {0} does not exist, not loading modules from directory", modulesDir);
    }

    // Read module config
    core::configManager.acquire();
    std::vector<std::string> modules = core::configManager.conf["modules"];
    auto modList = core::configManager.conf["moduleInstances"].items();
    core::configManager.release();

    // Load additional modules specified through config
    for (auto const& path : modules) {
        std::string apath = std::filesystem::absolute(path).string();
        flog::info("Loading {0}", apath);
        LoadingScreen::show("Loading " + std::filesystem::path(path).filename().string());
        core::moduleManager.loadModule(apath);
    }

    // Create module instances
    for (auto const& [name, _module] : modList) {
        std::string mod = _module["module"];
        bool enabled = _module["enabled"];
    	if (enabled) {
            flog::info("Initializing {0} ({1})", name, mod);
            LoadingScreen::show("Initializing " + name + " (" + mod + ")");
            core::moduleManager.createInstance(name, mod);
    	}
        /*if (!enabled) { 
			core::moduleManager.disableInstance(name); 
		}*/
    }

    // Load color maps
    LoadingScreen::show("Loading color maps");
    flog::info("Loading color maps");
    if (std::filesystem::is_directory(resourcesDir + "/colormaps")) {
        for (const auto& file : std::filesystem::directory_iterator(resourcesDir + "/colormaps")) {
            std::string path = file.path().generic_string();
            LoadingScreen::show("Loading " + file.path().filename().string());
            flog::info("Loading {0}", path);
            if (file.path().extension().generic_string() != ".json") {
                continue;
            }
            if (!file.is_regular_file()) { continue; }
            colormaps::loadMap(path);
        }
    }
    else {
        flog::warn("Color map directory {0} does not exist, not loading modules from directory", modulesDir);
    }

    gui::waterfall.updatePalletteFromArray(colormaps::maps["Turbo"].map, colormaps::maps["Turbo"].entryCount);

    sourcemenu::init();
    sinkmenu::init();
    bandplanmenu::init();
    displaymenu::init();
    vfo_color_menu::init();
    module_manager_menu::init();
	
    // Update UI settings
    LoadingScreen::show("Loading configuration");
    core::configManager.acquire();

    double frequency = core::configManager.conf["frequency"];

    showMenu = core::configManager.conf["showMenu"];
    startedWithMenuClosed = !showMenu;
	
	showSidebar = core::configManager.conf["showSidebar"];

    gui::freqSelect.setFrequency(frequency);
    gui::freqSelect.frequencyChanged = false;
    sigpath::sourceManager.tune(frequency);
    gui::waterfall.setCenterFrequency(frequency);

    menuWidth = core::configManager.conf["menuWidth"];
    newWidth = menuWidth;

    tuningMode = core::configManager.conf["centerTuning"] ? tuner::TUNER_MODE_CENTER : tuner::TUNER_MODE_NORMAL;
    gui::waterfall.VFOMoveSingleClick = (tuningMode == tuner::TUNER_MODE_CENTER);

    core::configManager.release();
	
	gui::sideBar.init();
	gui::mainView.init();

    // Correct the offset of all VFOs so that they fit on the screen
    float finalBwHalf = gui::waterfall.getBandwidth() / 2.0;
    for (auto& [_name, _vfo] : gui::waterfall.vfos) {
        if (_vfo->lowerOffset < -finalBwHalf) {
            sigpath::vfoManager.setCenterOffset(_name, (_vfo->bandwidth / 2) - finalBwHalf);
            continue;
        }
        if (_vfo->upperOffset > finalBwHalf) {
            sigpath::vfoManager.setCenterOffset(_name, finalBwHalf - (_vfo->bandwidth / 2));
            continue;
        }
    }

    autostart = core::args["autostart"].b();
    initComplete = true;

    core::moduleManager.doPostInitAll();
}

void MainWindow::deinit() {
    gui::sideBar.deinit();    
    gui::mainView.deinit();
}

float* MainWindow::acquireFFTBuffer(void* ctx) {
    return gui::waterfall.getFFTBuffer();
}

void MainWindow::releaseFFTBuffer(void* ctx) {
    gui::waterfall.pushFFT();
}

void MainWindow::vfoAddedHandler(VFOManager::VFO* vfo, void* ctx) {
    MainWindow* _this = (MainWindow*)ctx;
    std::string name = vfo->getName();
    core::configManager.acquire();
    if (!core::configManager.conf["vfoOffsets"].contains(name)) {
        core::configManager.release();
        return;
    }
    double offset = core::configManager.conf["vfoOffsets"][name];
    core::configManager.release();

    double viewBW = gui::waterfall.getViewBandwidth();
    double viewOffset = gui::waterfall.getViewOffset();

    double viewLower = viewOffset - (viewBW / 2.0);
    double viewUpper = viewOffset + (viewBW / 2.0);

    double newOffset = std::clamp<double>(offset, viewLower, viewUpper);

    sigpath::vfoManager.setCenterOffset(name, _this->initComplete ? newOffset : offset);
}

void MainWindow::drawMenu() {
	ImGui::BeginChild("Menu");

	if (gui::menu.draw(firstMenuRender)) {
		core::configManager.acquire();
		json arr = json::array();
		for (int i = 0; i < gui::menu.order.size(); i++) {
			arr[i]["name"] = gui::menu.order[i].name;
			arr[i]["open"] = gui::menu.order[i].open;
		}
		core::configManager.conf["menuElements"] = arr;

		// Update enabled and disabled modules
		for (auto [_name, inst] : core::moduleManager.instances) {
			if (!core::configManager.conf["moduleInstances"].contains(_name)) { continue; }
			core::configManager.conf["moduleInstances"][_name]["enabled"] = inst.instance->isEnabled();
		}

		core::configManager.release(true);
	}
	if (startedWithMenuClosed) {
		startedWithMenuClosed = false;
	}
	else {
		firstMenuRender = false;
	}

	if (ImGui::CollapsingHeader("Debug")) {
		ImGui::Text("Frame time: %.3f ms/frame", ImGui::GetIO().DeltaTime * 1000.0f);
		ImGui::Text("Framerate: %.1f FPS", ImGui::GetIO().Framerate);
		ImGui::Text("Center Frequency: %.0f Hz", gui::waterfall.getCenterFrequency());
		ImGui::Text("Source name: %s", sourceName.c_str());
		ImGui::Checkbox("Show demo window", &demoWindow);
		ImGui::Text("ImGui version: %s", ImGui::GetVersion());

		// ImGui::Checkbox("Bypass buffering", &sigpath::iqFrontEnd.inputBuffer.bypass);

		// ImGui::Text("Buffering: %d", (sigpath::iqFrontEnd.inputBuffer.writeCur - sigpath::iqFrontEnd.inputBuffer.readCur + 32) % 32);

		if (ImGui::Button("Test Bug")) {
			flog::error("Will this make the software crash?");
		}

		if (ImGui::Button("Testing something")) {
			gui::menu.order[0].open = true;
			firstMenuRender = true;
		}

		ImGui::Checkbox("WF Single Click", &gui::waterfall.VFOMoveSingleClick);
		ImGui::Checkbox("Lock Menu Order", &gui::menu.locked);

		ImGui::Spacing();
	}

	ImGui::EndChild();
}

void MainWindow::draw() {
    ImGui::Begin("Main", NULL, WINDOW_FLAGS);
    ImVec4 textCol = ImGui::GetStyleColorVec4(ImGuiCol_Text);

    ImGui::WaterfallVFO* vfo = NULL;
    if (gui::waterfall.selectedVFO != "") {
        vfo = gui::waterfall.vfos[gui::waterfall.selectedVFO];
    }

    // Handle VFO movement
    if (vfo != NULL) {
        if (vfo->centerOffsetChanged) {
            if (tuningMode == tuner::TUNER_MODE_CENTER) {
                tuner::tune(tuner::TUNER_MODE_CENTER, gui::waterfall.selectedVFO, gui::waterfall.getCenterFrequency() + vfo->generalOffset);
            }
            gui::freqSelect.setFrequency(gui::waterfall.getCenterFrequency() + vfo->generalOffset);
            gui::freqSelect.frequencyChanged = false;
            core::configManager.acquire();
            core::configManager.conf["vfoOffsets"][gui::waterfall.selectedVFO] = vfo->generalOffset;
            core::configManager.release(true);
        }
    }

    sigpath::vfoManager.updateFromWaterfall(&gui::waterfall);

    // Handle selection of another VFO
    if (gui::waterfall.selectedVFOChanged) {
        gui::freqSelect.setFrequency((vfo != NULL) ? (vfo->generalOffset + gui::waterfall.getCenterFrequency()) : gui::waterfall.getCenterFrequency());
        gui::waterfall.selectedVFOChanged = false;
        gui::freqSelect.frequencyChanged = false;
    }

    // Handle change in selected frequency
    if (gui::freqSelect.frequencyChanged) {
        gui::freqSelect.frequencyChanged = false;
        tuner::tune(tuningMode, gui::waterfall.selectedVFO, gui::freqSelect.frequency);
        if (vfo != NULL) {
            vfo->centerOffsetChanged = false;
            vfo->lowerOffsetChanged = false;
            vfo->upperOffsetChanged = false;
        }
        core::configManager.acquire();
        core::configManager.conf["frequency"] = gui::waterfall.getCenterFrequency();
        if (vfo != NULL) {
            core::configManager.conf["vfoOffsets"][gui::waterfall.selectedVFO] = vfo->generalOffset;
        }
        core::configManager.release(true);
    }

    // Handle dragging the frequency scale
    if (gui::waterfall.centerFreqMoved) {
        gui::waterfall.centerFreqMoved = false;
        sigpath::sourceManager.tune(gui::waterfall.getCenterFrequency());
        if (vfo != NULL) {
            gui::freqSelect.setFrequency(gui::waterfall.getCenterFrequency() + vfo->generalOffset);
        }
        else {
            gui::freqSelect.setFrequency(gui::waterfall.getCenterFrequency());
        }
        core::configManager.acquire();
        core::configManager.conf["frequency"] = gui::waterfall.getCenterFrequency();
        core::configManager.release(true);
    }

    // Toolbar on top
	// Show/hide menu or/and sidebar
    ImVec2 btnSize(TOOLBAR_BUTTON_SIZE * style::uiScale, TOOLBAR_BUTTON_SIZE * style::uiScale);
    ImGui::PushID(ImGui::GetID("sdrpp_menu_btn"));
    if (ImGui::ImageButton(icons::MENU, btnSize, ImVec2(0, 0), ImVec2(1, 1), 5, ImVec4(0, 0, 0, 0), textCol) || ImGui::IsKeyPressed(ImGuiKey_Menu, false)) {
		if (!showMenu && showSidebar) {
			showMenu = true;
		} else if (showMenu && showSidebar) {
			showSidebar = false;
		} else if (showMenu && !showSidebar) {
			showMenu = false;
		} else {
			showSidebar = true;
		}
        core::configManager.acquire();
        core::configManager.conf["showMenu"] = showMenu;
		core::configManager.conf["showSidebar"] = showSidebar;
        core::configManager.release(true);
    }
    ImGui::PopID();

	// Play/stop button
    ImGui::SameLine();
    bool tmpPlaySate = playing;
    if (playButtonLocked && !tmpPlaySate) { style::beginDisabled(); }
    if (playing) {
        ImGui::PushID(ImGui::GetID("sdrpp_stop_btn"));
        if (ImGui::ImageButton(icons::STOP, btnSize, ImVec2(0, 0), ImVec2(1, 1), 5, ImVec4(0, 0, 0, 0), textCol) || ImGui::IsKeyPressed(ImGuiKey_End, false)) {
            setPlayState(false);
        }
        ImGui::PopID();
    }
    else { // TODO: Might need to check if there even is a device
        ImGui::PushID(ImGui::GetID("sdrpp_play_btn"));
        if (ImGui::ImageButton(icons::PLAY, btnSize, ImVec2(0, 0), ImVec2(1, 1), 5, ImVec4(0, 0, 0, 0), textCol) || ImGui::IsKeyPressed(ImGuiKey_End, false)) {
            setPlayState(true);
        }
        ImGui::PopID();
    }
    if (playButtonLocked && !tmpPlaySate) { style::endDisabled(); }

    // Handle auto-start
    if (autostart) {
        autostart = false;
        setPlayState(true);
    }

	// volumn slider
    ImGui::SameLine();
    float origY = ImGui::GetCursorPosY();
    sigpath::sinkManager.showVolumeSlider(gui::waterfall.selectedVFO, "##_sdrpp_main_volume_", 238 * style::uiScale, btnSize.y, 5, true);

	// Frequency select
    if (freqSelectLocked) { style::beginDisabled();}
    ImGui::SameLine();
    ImGui::SetCursorPosY(origY);
    gui::freqSelect.draw();
    if (freqSelectLocked) { style::endDisabled();}

	// Tuning mode button
    if (tuningModeLocked) { style::beginDisabled();}
    ImGui::SameLine();
    ImGui::SetCursorPosY(origY);
    if (tuningMode == tuner::TUNER_MODE_CENTER) {
        ImGui::PushID(ImGui::GetID("sdrpp_ena_st_btn"));
        if (ImGui::ImageButton(icons::CENTER_TUNING, btnSize, ImVec2(0, 0), ImVec2(1, 1), 5, ImVec4(0, 0, 0, 0), textCol)) {
            tuningMode = tuner::TUNER_MODE_NORMAL;
            gui::waterfall.VFOMoveSingleClick = false;
            core::configManager.acquire();
            core::configManager.conf["centerTuning"] = false;
            core::configManager.release(true);
        }
        ImGui::PopID();
    }
    else { // TODO: Might need to check if there even is a device
        ImGui::PushID(ImGui::GetID("sdrpp_dis_st_btn"));
        if (ImGui::ImageButton(icons::NORMAL_TUNING, btnSize, ImVec2(0, 0), ImVec2(1, 1), 5, ImVec4(0, 0, 0, 0), textCol)) {
            tuningMode = tuner::TUNER_MODE_CENTER;
            gui::waterfall.VFOMoveSingleClick = true;
            tuner::tune(tuner::TUNER_MODE_CENTER, gui::waterfall.selectedVFO, gui::freqSelect.frequency);
            core::configManager.acquire();
            core::configManager.conf["centerTuning"] = true;
            core::configManager.release(true);
        }
        ImGui::PopID();
    }
    if (tuningModeLocked) { style::endDisabled(); }

	// SNR meter
    ImGui::SameLine();
    int snrWidth = ImGui::GetWindowSize().x - ImGui::GetCursorPosX() - 82.0f;
    int snrPos = ImGui::GetCursorPosX() + 10.0f;
    ImGui::SetCursorPosX(snrPos);
    ImGui::SetCursorPosY(origY + (10.0f));
    ImGui::SetNextItemWidth(snrWidth);
    ImGui::SNRMeter((vfo != NULL) ? gui::waterfall.selectedVFOSNR : 0);

    // Logo button
	ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowSize().x - 62.0f);
    ImGui::SetCursorPosY(5.0f);
	ImVec4 transparentColor = ImVec4(0, 0, 0, 0);
	ImGui::PushStyleColor(ImGuiCol_Button, transparentColor);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, transparentColor);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, transparentColor);
    if (ImGui::ImageButton(icons::LOGO, ImVec2(52, 52), ImVec2(0, 0), ImVec2(1, 1), 0)) {
        showCredits = true;
    }
	ImGui::PopStyleColor(3);

    // Reset waterfall lock
    lockWaterfallControls = showCredits;

    // Handle menu resize
    ImVec2 winSize = ImGui::GetWindowSize();
    ImVec2 mousePos = ImGui::GetMousePos();
    if (!lockWaterfallControls && showMenu) {
        float curY = ImGui::GetCursorPosY();
        bool click = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        bool down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        if (grabbingMenu) {
            newWidth = mousePos.x;
            newWidth = std::clamp<float>(newWidth, 250, winSize.x - 250);
            ImGui::GetForegroundDrawList()->AddLine(ImVec2(newWidth, curY), ImVec2(newWidth, winSize.y - 10), ImGui::GetColorU32(ImGuiCol_SeparatorActive));
        }
        if (mousePos.x >= newWidth - (2.0f * style::uiScale) && mousePos.x <= newWidth + (2.0f * style::uiScale) && mousePos.y > curY) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            if (click) {
                grabbingMenu = true;
            }
        }
        else {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
        }
        if (!down && grabbingMenu) {
            grabbingMenu = false;
            menuWidth = newWidth;
            core::configManager.acquire();
            core::configManager.conf["menuWidth"] = menuWidth;
            core::configManager.release(true);
        }
    }

    // Process menu keybinds
    displaymenu::checkKeybinds();

    // Window columns
	if (!showMenu && showSidebar) {
		ImGui::Columns(3, "WindowColumns", false);
        ImGui::SetColumnWidth(0, 8);
		ImGui::SetColumnWidth(1, SIDEBAR_WIDTH);
        ImGui::SetColumnWidth(2, winSize.x - 8 - SIDEBAR_WIDTH);
		gui::sideBar.drawSideBar();
	} else if (showMenu && showSidebar) {
		ImGui::Columns(3, "WindowColumns", false);
        ImGui::SetColumnWidth(0, menuWidth);
		ImGui::SetColumnWidth(1, SIDEBAR_WIDTH * style::uiScale);
        ImGui::SetColumnWidth(2, std::max<int>(winSize.x - menuWidth - SIDEBAR_WIDTH, 100.0f));
		drawMenu();
		gui::sideBar.drawSideBar();
	} else if (showMenu && !showSidebar) {
		ImGui::Columns(3, "WindowColumns", false);
        ImGui::SetColumnWidth(0, menuWidth);
		ImGui::SetColumnWidth(1, 8);
        ImGui::SetColumnWidth(2, winSize.x - menuWidth - 8);
		drawMenu();
		ImGui::NextColumn();
	} else {
		ImGui::Columns(3, "WindowColumns", false);
        ImGui::SetColumnWidth(0, 4);
		ImGui::SetColumnWidth(1, 4);
        ImGui::SetColumnWidth(2, winSize.x - 8);
		ImGui::NextColumn();
	}
	
    // Main view column
    ImGui::NextColumn();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 0));
        
    gui::mainView.drawMainView();
    
	ImGui::PopStyleVar();
	
    ImGui::End();
	
	// Draw function selectors
	gui::funcSelectA.draw();
	gui::funcSelectB.draw();
	
    if (showCredits) {
        credits::show(&showCredits);
    }

    if (demoWindow) {
        ImGui::ShowDemoWindow();
    }
}

void MainWindow::setPlayState(bool _playing) {
    if (_playing == playing) { return; }
    if (_playing) {
        sigpath::iqFrontEnd.flushInputBuffer();
        sigpath::sourceManager.start();
        sigpath::sourceManager.tune(gui::waterfall.getCenterFrequency());
        playing = true;
        onPlayStateChange.emit(true);
    }
    else {
        playing = false;
        onPlayStateChange.emit(false);
        sigpath::sourceManager.stop();
        sigpath::iqFrontEnd.flushInputBuffer();
    }
}

bool MainWindow::sdrIsRunning() {
    return playing;
}

bool MainWindow::isPlaying() {
    return playing;
}

void MainWindow::setFirstMenuRender() {
    firstMenuRender = true;
}

int MainWindow::getTuningMode() {
    return tuningMode;
}

void MainWindow::setTuningMode(int mode) {
    tuningMode = mode;
}
