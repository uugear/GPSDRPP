#include <gui/widgets/waterfall_view.h>
#include <gui/widgets/range_slider.h>
#include <gui/gui.h>
#include <core.h>

#define WATERFALL_CONTROLS_WIDTH        50

WaterfallView::WaterfallView() {
    
}

WaterfallView::~WaterfallView() {

}

const char* WaterfallView::getLabel() {
	return "Waterfall";
}

void WaterfallView::init() {
	core::configManager.acquire();
	
    fftMin = core::configManager.conf["min"];
	prevFftMin = fftMin;
    gui::waterfall.setFFTMin(fftMin);
    gui::waterfall.setWaterfallMin(fftMin);
	
    fftMax = core::configManager.conf["max"];
	prevFftMax = fftMax;
    gui::waterfall.setFFTMax(fftMax);
    gui::waterfall.setWaterfallMax(fftMax);
	
	setBandwidth(1.0f);
    gui::waterfall.vfoFreqChanged = false;
    gui::waterfall.centerFreqMoved = false;
    gui::waterfall.selectFirstVFO();
	
    fftHeight = core::configManager.conf["fftHeight"];
	gui::waterfall.setFFTHeight(fftHeight);
	
    core::configManager.release();
}

void WaterfallView::deinit() {

}

void WaterfallView::draw() {
    // If FFT height is changed, save to configuration
    int _fftHeight = gui::waterfall.getFFTHeight();
    if (fftHeight != _fftHeight) {
        fftHeight = _fftHeight;
        core::configManager.acquire();
        core::configManager.conf["fftHeight"] = fftHeight;
        core::configManager.release(true);
    }
    
    ImVec2 winSize = ImGui::GetWindowSize();
        
    ImGui::WaterfallVFO* vfo = NULL;
    if (gui::waterfall.selectedVFO != "") {
        vfo = gui::waterfall.vfos[gui::waterfall.selectedVFO];
    }
        
    ImGui::Columns(2, "WaterfallColumns", false);
    ImGui::SetColumnWidth(0, winSize.x - WATERFALL_CONTROLS_WIDTH);
    ImGui::SetColumnWidth(1, WATERFALL_CONTROLS_WIDTH);

    ImGui::BeginChild("Waterfall");
    gui::waterfall.draw();
    ImGui::EndChild();

    int tuningMode = gui::mainWindow.getTuningMode();
    if (!gui::mainWindow.lockWaterfallControls) {
        // Handle arrow keys
        if (vfo != NULL && (gui::waterfall.mouseInFFT || gui::waterfall.mouseInWaterfall)) {
            bool freqChanged = false;
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) && !gui::freqSelect.digitHovered) {
                double nfreq = gui::waterfall.getCenterFrequency() + vfo->generalOffset - vfo->snapInterval;
                nfreq = roundl(nfreq / vfo->snapInterval) * vfo->snapInterval;
                tuner::tune(tuningMode, gui::waterfall.selectedVFO, nfreq);
                freqChanged = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) && !gui::freqSelect.digitHovered) {
                double nfreq = gui::waterfall.getCenterFrequency() + vfo->generalOffset + vfo->snapInterval;
                nfreq = roundl(nfreq / vfo->snapInterval) * vfo->snapInterval;
                tuner::tune(tuningMode, gui::waterfall.selectedVFO, nfreq);
                freqChanged = true;
            }
            if (freqChanged) {
                core::configManager.acquire();
                core::configManager.conf["frequency"] = gui::waterfall.getCenterFrequency();
                if (vfo != NULL) {
                    core::configManager.conf["vfoOffsets"][gui::waterfall.selectedVFO] = vfo->generalOffset;
                }
                core::configManager.release(true);
            }
        }

        // Handle scrollwheel
        int wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0 && (gui::waterfall.mouseInFFT || gui::waterfall.mouseInWaterfall)) {
            double nfreq;
            if (vfo != NULL) {
                // Select factor depending on modifier keys
                double interval;
                if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
                    interval = vfo->snapInterval * 10.0;
                }
                else if (ImGui::IsKeyDown(ImGuiKey_LeftAlt)) {
                    interval = vfo->snapInterval * 0.1;
                }
                else {
                    interval = vfo->snapInterval;
                }

                nfreq = gui::waterfall.getCenterFrequency() + vfo->generalOffset + (interval * wheel);
                nfreq = roundl(nfreq / interval) * interval;
            }
            else {
                nfreq = gui::waterfall.getCenterFrequency() - (gui::waterfall.getViewBandwidth() * wheel / 20.0);
            }
            tuner::tune(tuningMode, gui::waterfall.selectedVFO, nfreq);
            gui::freqSelect.setFrequency(nfreq);
            core::configManager.acquire();
            core::configManager.conf["frequency"] = gui::waterfall.getCenterFrequency();
            if (vfo != NULL) {
                core::configManager.conf["vfoOffsets"][gui::waterfall.selectedVFO] = vfo->generalOffset;
            }
            core::configManager.release(true);
        }
    }
    
    ImGui::NextColumn();
    ImGui::BeginChild("WaterfallControls");
    
    ImGui::SetCursorPosX(0);
    if (ImGui::RangeSliderFloat("##_range_v", &fftMin, &fftMax, -160.0f, 0.0f, 40, fftHeight - 60.0f, true, false, 10.0f)) {
        
        if (prevFftMin != fftMin) {
            fftMin = std::min<float>(fftMax - 10, fftMin);
            core::configManager.acquire();
            core::configManager.conf["min"] = fftMin;
            core::configManager.release(true);
            prevFftMin = fftMin;
        } else if (prevFftMax != fftMax) {
            fftMax = std::max<float>(fftMax, fftMin + 10);
            core::configManager.acquire();
            core::configManager.conf["max"] = fftMax;
            core::configManager.release(true);
            prevFftMax = fftMax;
        }
    }
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.0) - (ImGui::CalcTextSize("Range").x / 2.0));
    ImGui::TextUnformatted("Range");
    
    ImGui::NewLine();
    ImGui::NewLine();

    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.0) - (ImGui::CalcTextSize("Zoom").x / 2.0));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 15);
	ImGui::TextUnformatted("Zoom");
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.0) - 25);
    ImVec2 wfSliderSize(45.0, ImGui::GetWindowSize().y - ImGui::GetCursorPosY());
    if (ImGui::VSliderFloat("##_7_", wfSliderSize, &bw, 1.0, 0.0, "")) {
        onZoom();
    }

    ImGui::EndChild();

    gui::waterfall.setFFTMin(fftMin);
    gui::waterfall.setFFTMax(fftMax);
    gui::waterfall.setWaterfallMin(fftMin);
    gui::waterfall.setWaterfallMax(fftMax);
}


void WaterfallView::onZoom() {
    double factor = (double)bw * (double)bw;

    // Map 0.0 -> 1.0 to 1000.0 -> bandwidth
    double wfBw = gui::waterfall.getBandwidth();
    double delta = wfBw - 1000.0;
    double finalBw = std::min<double>(1000.0 + (factor * delta), wfBw);
    gui::waterfall.setViewBandwidth(finalBw);

    ImGui::WaterfallVFO* vfo = NULL;
    if (gui::waterfall.selectedVFO != "") {
        vfo = gui::waterfall.vfos[gui::waterfall.selectedVFO];
    }
    if (vfo != NULL) {
        gui::waterfall.setViewOffset(vfo->centerOffset); // center vfo on screen
    }
}


void WaterfallView::setBandwidth(float bandwidth) {
    bw = bandwidth;
}


float WaterfallView::getBandwidth() {
    return bw;
}