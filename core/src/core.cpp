#include <server.h>
#include "imgui.h"
#include <stdio.h>
#include <gui/main_window.h>
#include <gui/style.h>
#include <gui/gui.h>
#include <gui/icons.h>
#include <version.h>
#include <utils/flog.h>
#include <gui/widgets/bandplan.h>
#include <stb_image.h>
#include <config.h>
#include <core.h>
#include <filesystem>
#include <gui/menus/theme.h>
#include <backend.h>
#include <iostream>
#include "dev_refresh_worker.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>

#include <gui/widgets/waterfall_view.h>
#include <gui/widgets/map_view.h>
#include <gui/widgets/mode_s_page.h>

#ifndef INSTALL_PREFIX
#define INSTALL_PREFIX "/usr"
#endif

const char* GPIO_CHIP_2 = "/dev/gpiochip2";
const char* GPIO_CHIP_4 = "/dev/gpiochip4";

namespace core {
    
    VUGPSDR_Initializer g_vugpsdr_initializer;
    VUGPSDR_Initializer::VUGPSDR_Initializer() {
        
        // turn on the device
        flog::info("VU GPSDR power on");
    	system("vgp alt 4B2 0");
    	system("vgp mode 4B2 OUT");
    	system("vgp set 4B2 0");
    	
    	system("vgp alt 4B4 0");
    	system("vgp mode 4B4 OUT");
    	system("vgp set 4B4 1");
		
		system("vgp alt 4D6 0");
    	system("vgp mode 4D6 OUT");
    	system("vgp set 4D6 0");

    	// make sure user can access I2C device
    	flog::info("Check permissions for accessing I2C device...");
    	FILE* fp = fopen(GPS_I2C_BUS, "r+");
        if (fp) {
            flog::info("Current user can access I2C device.");
            fclose(fp);
        } else {
            flog::info("Current user can NOT access I2C device.");
            system("sudo usermod -a -G i2c $USER");
            flog::info("Added current user to 'i2c' group.");
            system("sudo newgrp i2c");
    	}
    }
    
    ConfigManager configManager;
    ModuleManager moduleManager;
    ModuleComManager modComManager;
    CommandArgsParser args;
    Gps gps;
    Si5351 si5351;
    UpConverter upConverter;

    void setInputSampleRate(double samplerate) {
        // Forward this to the server
        if (args["server"].b()) { server::setInputSampleRate(samplerate); return; }
        
        // Update IQ frontend input samplerate and get effective samplerate
        sigpath::iqFrontEnd.setSampleRate(samplerate);
        double effectiveSr  = sigpath::iqFrontEnd.getEffectiveSamplerate();
        
        // Reset zoom
        gui::waterfall.setBandwidth(effectiveSr);
        gui::waterfall.setViewOffset(0);
        gui::waterfall.setViewBandwidth(effectiveSr);
        WaterfallView::getInstance().setBandwidth(1.0);

        // Debug logs
        flog::info("New DSP samplerate: {0} (source samplerate is {1})", effectiveSr, samplerate);
    }
    
    namespace encoders {
        // Callback when encoder A rotate clockwise
        void on_encoder_a_cw(Encoder *enc) {
            if (!gui::funcSelectA.isVisible()) {
        	    enc->onRotate(gui::funcSelectA.getSel(), true);
        	}
        }
        
        // Callback when encoder A rotate counter-clockwise
        void on_encoder_a_ccw(Encoder *enc) {
            if (!gui::funcSelectA.isVisible()) {
        	    enc->onRotate(gui::funcSelectA.getSel(), false);
        	}
        }
        
        // Callback when encoder A is pressed
        void on_encoder_a_pressed(Encoder *enc) {
            // do nothing
        }
        
        // Callback when encoder A is released
        void on_encoder_a_released(Encoder *enc) {
			gui::funcSelectA.setVisible(true);
        }
        
        // Callback when encoder B rotate clockwise
        void on_encoder_b_cw(Encoder *enc) {
            if (!gui::funcSelectB.isVisible()) {
                enc->onRotate(gui::funcSelectB.getSel(), true);
            }
        }
        
        // Callback when encoder B rotate counter-clockwise
        void on_encoder_b_ccw(Encoder *enc) {
            if (!gui::funcSelectB.isVisible()) {
                enc->onRotate(gui::funcSelectB.getSel(), false);
            }
        }
        
        // Callback when encoder B is pressed
        void on_encoder_b_pressed(Encoder *enc) {
            // do nothing
        }
        
        // Callback when encoder B is released
        void on_encoder_b_released(Encoder *enc) {
			gui::funcSelectB.setVisible(true);
        }
    }
};


// main
int gpsdrpp_main(int argc, char* argv[]) {
    flog::info("GPSDR++ v" VERSION_STR);

    // Define command line options and parse arguments
    core::args.defineAll();
    if (core::args.parse(argc, argv) < 0) { return -1; } 

    // Show help and exit if requested
    if (core::args["help"].b()) {
        core::args.showHelp();
        return 0;
    }
    
    // Tell GPS to output 24MHz timepulse
    core::gps.outputReferenceClock(false);

    bool serverMode = (bool)core::args["server"];

    // Check root directory
    std::string root = (std::string)core::args["root"];
    if (!std::filesystem::exists(root)) {
        flog::warn("Root directory {0} does not exist, creating it", root);
        if (!std::filesystem::create_directories(root)) {
            flog::error("Could not create root directory {0}", root);
            return -1;
        }
    }

    // Check that the path actually is a directory
    if (!std::filesystem::is_directory(root)) {
        flog::error("{0} is not a directory", root);
        return -1;
    }
	
	flog::info("Root directory is: {0}", root);
	
	MapView::getInstance().setRootPath(root);

    // ======== DEFAULT CONFIG ========
    json defConfig;
    defConfig["bandColors"]["amateur"] = "#FF0000FF";
    defConfig["bandColors"]["aviation"] = "#00FF00FF";
    defConfig["bandColors"]["broadcast"] = "#0000FFFF";
    defConfig["bandColors"]["marine"] = "#00FFFFFF";
    defConfig["bandColors"]["military"] = "#FFFF00FF";
    defConfig["bandPlan"] = "General";
    defConfig["bandPlanEnabled"] = true;
    defConfig["bandPlanPos"] = 0;
    defConfig["centerTuning"] = false;
    defConfig["colorMap"] = "Classic";
    defConfig["fftHold"] = false;
    defConfig["fftHoldSpeed"] = 60;
    defConfig["fftSmoothing"] = false;
    defConfig["fftSmoothingSpeed"] = 100;
    defConfig["snrSmoothing"] = false;
    defConfig["snrSmoothingSpeed"] = 20;
    defConfig["fastFFT"] = false;
    defConfig["fftRate"] = 20;
    defConfig["fftSize"] = 65536;
    defConfig["fftWindow"] = 2;
    defConfig["frequency"] = 100000000.0;
    defConfig["fullWaterfallUpdate"] = false;
    defConfig["maximized"] = false;
    defConfig["fullscreen"] = true;
    
    // Side Bar
    defConfig["selectedPage"] = 0;
    
    // Radio Page
    defConfig["hfLNA"] = true;
    
    // GPS Page
    defConfig["lockToGpsFreq"] = false;
    
    // CLK OUT Page
    defConfig["clkOut"] = true;
    defConfig["reg58"] = 0;
    defConfig["reg59"] = 1;
    defConfig["reg60"] = 0;
    defConfig["reg61"] = 34;
    defConfig["reg62"] = 0;
    defConfig["reg63"] = 0;
    defConfig["reg64"] = 0;
    defConfig["reg65"] = 0;
    defConfig["strength"] = 2;
	
	// Main View
	defConfig["selectedTab"] = 0;
	
	// Waterfall View
	defConfig["min"] = -120.0;
	defConfig["max"] = 0.0;
	defConfig["fftHeight"] = 300;
	
	// Map View
	defConfig["tileServer"] = "https://tile.openstreetmap.org/";
	defConfig["zoom"] = 19;
	defConfig["panLon"] = 0.0;
	defConfig["panLat"] = 0.0;
	defConfig["longitude"] = 0.0;
	defConfig["latitude"] = 0.0;

    // Menu
    defConfig["menuElements"] = json::array();

    defConfig["menuElements"][0]["name"] = "Source";
    defConfig["menuElements"][0]["open"] = true;

    defConfig["menuElements"][1]["name"] = "Radio";
    defConfig["menuElements"][1]["open"] = true;

    defConfig["menuElements"][2]["name"] = "Recorder";
    defConfig["menuElements"][2]["open"] = true;

    defConfig["menuElements"][3]["name"] = "Sinks";
    defConfig["menuElements"][3]["open"] = true;

    defConfig["menuElements"][3]["name"] = "Frequency Manager";
    defConfig["menuElements"][3]["open"] = true;

    defConfig["menuElements"][4]["name"] = "VFO Color";
    defConfig["menuElements"][4]["open"] = true;

    defConfig["menuElements"][6]["name"] = "Band Plan";
    defConfig["menuElements"][6]["open"] = true;

    defConfig["menuElements"][7]["name"] = "Display";
    defConfig["menuElements"][7]["open"] = true;
	
    defConfig["menuElements"][8]["name"] = "Mode S";
    defConfig["menuElements"][8]["open"] = true;

    defConfig["menuWidth"] = 300;

    // Module instances
    defConfig["moduleInstances"]["RTL-SDR Source"]["module"] = "rtl_sdr_source";
    defConfig["moduleInstances"]["RTL-SDR Source"]["enabled"] = true;
    defConfig["moduleInstances"]["Audio Sink"] = "audio_sink";
    defConfig["moduleInstances"]["Radio"] = "radio";
    defConfig["moduleInstances"]["Mode S"]["module"] = "mode_s_decoder";
	defConfig["moduleInstances"]["Mode S"]["enabled"] = true;
    defConfig["moduleInstances"]["Frequency Manager"] = "frequency_manager";
    defConfig["moduleInstances"]["Recorder"] = "recorder";

    // Themes
    defConfig["theme"] = "UUGear";

    defConfig["uiScale"] = 1.0f;

    defConfig["modules"] = json::array();

    defConfig["offsetMode"] = (int)0; // Off
    defConfig["offset"] = 0.0;
    defConfig["showMenu"] = false;
	defConfig["showSidebar"] = true;
    defConfig["showWaterfall"] = true;
    defConfig["source"] = "";
    defConfig["decimationPower"] = 0;
    defConfig["iqCorrection"] = false;
    defConfig["invertIQ"] = false;

    defConfig["streams"]["Radio"]["muted"] = false;
    defConfig["streams"]["Radio"]["sink"] = "Audio";
    defConfig["streams"]["Radio"]["volume"] = 1.0f;

    defConfig["windowSize"]["h"] = 720;
    defConfig["windowSize"]["w"] = 1280;

    defConfig["vfoOffsets"] = json::object();

    defConfig["vfoColors"]["Radio"] = "#FFFFFF";

    defConfig["lockMenuOrder"] = false;

    defConfig["modulesDirectory"] = INSTALL_PREFIX "/lib/gpsdrpp/plugins";
    defConfig["resourcesDirectory"] = INSTALL_PREFIX "/share/gpsdrpp";

    // Load config
    flog::info("Loading config");
    core::configManager.setPath(root + "/config.json");
    core::configManager.load(defConfig);
    core::configManager.enableAutoSave();
    core::configManager.acquire();

    // Fix missing elements in config
    for (auto const& item : defConfig.items()) {
        if (!core::configManager.conf.contains(item.key())) {
            flog::info("Missing key in config {0}, repairing", item.key());
            core::configManager.conf[item.key()] = defConfig[item.key()];
        }
    }

    // Remove unused elements
    auto items = core::configManager.conf.items();
    for (auto const& item : items) {
        if (!defConfig.contains(item.key())) {
            flog::info("Unused key in config {0}, repairing", item.key());
            core::configManager.conf.erase(item.key());
        }
    }

    // Update to new module representation in config if needed
    for (auto [_name, inst] : core::configManager.conf["moduleInstances"].items()) {
        if (!inst.is_string()) { continue; }
        std::string mod = inst;
        json newMod;
        newMod["module"] = mod;
        newMod["enabled"] = true;
        core::configManager.conf["moduleInstances"][_name] = newMod;
    }
	
	// Force fullscreen at start
	core::configManager.conf["fullscreen"] = true;

    core::configManager.release(true);

    if (serverMode) { return server::main(); }

    core::configManager.acquire();
    std::string resDir = core::configManager.conf["resourcesDirectory"];
    json bandColors = core::configManager.conf["bandColors"];
    core::configManager.release();

    // Assert that the resource directory is absolute and check existence
    resDir = std::filesystem::absolute(resDir).string();
    if (!std::filesystem::is_directory(resDir)) {
        flog::error("Resource directory doesn't exist! Please make sure that you've configured it correctly in config.json (check readme for details)");
        return 1;
    }

    // Initialize backend
    int biRes = backend::init(resDir);
    if (biRes < 0) { return biRes; }

    // Initialize SmGui in normal mode
    SmGui::init(false);

    if (!style::loadFonts(resDir)) { return -1; }
    thememenu::init(resDir);
    LoadingScreen::init();

    LoadingScreen::show("Loading icons");
    flog::info("Loading icons");
    if (!icons::load(resDir)) { return -1; }

    LoadingScreen::show("Loading band plans");
    flog::info("Loading band plans");
    bandplan::loadFromDir(resDir + "/bandplans");

    LoadingScreen::show("Loading band plan colors");
    flog::info("Loading band plans color table");
    bandplan::loadColorTable(bandColors);
    
    // Initialize Si5351 to output correct clock
    core::si5351.initialize();
    
	// Initialize encoders
    LoadingScreen::show("Initializing encoders");
    flog::info("Initializing encoders");
	system("vgp alt 2B2 0");
	system("vgp alt 2D3 0");
	system("vgp alt 2A4 0");
	system("vgp alt 4B0 0");
	system("vgp alt 4B3 0");
	system("vgp alt 4B5 0");	
	
    Encoder encoderA(GPIO_CHIP_4, 8, GPIO_CHIP_4, 11, GPIO_CHIP_4, 13);
    encoderA.setCWCallback(core::encoders::on_encoder_a_cw);
    encoderA.setCCWCallback(core::encoders::on_encoder_a_ccw);
    encoderA.setPressedCallback(core::encoders::on_encoder_a_pressed);
    encoderA.setReleasedCallback(core::encoders::on_encoder_a_released);
    encoderA.start();

    Encoder encoderB(GPIO_CHIP_2, 10, GPIO_CHIP_2, 27, GPIO_CHIP_2, 4);
    encoderB.setCWCallback(core::encoders::on_encoder_b_cw);
    encoderB.setCCWCallback(core::encoders::on_encoder_b_ccw);
    encoderB.setPressedCallback(core::encoders::on_encoder_b_pressed);
    encoderB.setReleasedCallback(core::encoders::on_encoder_b_released);
    encoderB.start();

    gui::mainWindow.init();
	
	DeviceRefreshWorker worker;
	worker.start();

    flog::info("Ready.");

    // Run render loop (TODO: CHECK RETURN VALUE)
    backend::renderLoop();

    ModeSPage::getInstance().deinit();
        
    gui::mainWindow.deinit();

    // Shut down all modules
    for (auto& [name, mod] : core::moduleManager.modules) {
        mod.end();
    }

    // Terminate backend (TODO: CHECK RETURN VALUE)
    backend::end();

    sigpath::iqFrontEnd.stop();
    
    core::configManager.disableAutoSave();
    core::configManager.save();

    encoderA.stop();
    encoderB.stop();
	
	worker.stop();

    flog::info("Exiting successfully");
    return 0;
}
