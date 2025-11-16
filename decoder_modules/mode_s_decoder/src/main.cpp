#include <imgui.h>
#include <config.h>
#include <core.h>
#include <gui/style.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <module.h>
#include <gui/widgets/folder_select.h>
#include <gui/widgets/mode_s_page.h>
#include <utils/optionlist.h>
#include <rtl_sdr_source_interface.h>
#include "mode_s_decoder.h"
#include "mode_s_dsp.h"
#include "mode_s_decoder_interface.h"


SDRPP_MOD_INFO{
    /* Name:            */ "mode_s_decoder",
    /* Description:     */ "Mode S/ADS-B Decoder"
    /* Author:          */ "UUGear",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ -1
};

ConfigManager config;

void onModeSDetected(char * log, struct mode_s_message * msg) {
	ModeSPage::getInstance().addLog(log);
}

class ModeSDecoderModule : public ModuleManager::Instance {
public:
    ModeSDecoderModule(std::string name) {
        this->name = name;
		
		// Register the module interface
        core::modComManager.registerInterface("Mode S", "Mode S", moduleInterfaceHandler, this);

        // Register the menu
        gui::menu.registerEntry(name, menuHandler, this, this);

        // Prepare decoder context
        prepareContext(ModeSPage::getInstance().getContext(), onModeSDetected);

        // Enable by default
        enable();
    }

    ~ModeSDecoderModule() {
        gui::menu.removeEntry(name);
        // Stop DSP
        if (run) {
            decoder.stop();
        }

        sigpath::sinkManager.unregisterStream(name);
    }

    void postInit() {}

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

private:
	static void onStart(ModeSDecoderModule* _this) {
		_this->rtlSdrWasRunning = gui::mainWindow.isPlaying();

		core::modComManager.callInterface("RTL-SDR", RTL_SDR_SOURCE_IFACE_CMD_GET_SAMPLE_RATE_INDEX, NULL, &_this->savedSampleRateId);
		int newSampleRateId = 5;    // Index for 2MHz in RTLSDRSource's sampleRates
		core::modComManager.callInterface("RTL-SDR", RTL_SDR_SOURCE_IFACE_CMD_SET_SAMPLE_RATE_INDEX, &newSampleRateId, NULL);

        _this->savedSelectedVFO = gui::waterfall.selectedVFO;
        _this->savedFreq = gui::freqSelect.frequency;
        _this->savedTuningMode = gui::mainWindow.getTuningMode();
        if (gui::waterfall.selectedVFO != "") {
            ImGui::WaterfallVFO* vfo = gui::waterfall.vfos[gui::waterfall.selectedVFO];
            _this->savedSelectedVFOOffset = vfo->generalOffset;
        }

        gui::mainWindow.setTuningMode(tuner::TUNER_MODE_CENTER);
        gui::mainWindow.waterfallLocked = true;
        gui::mainWindow.freqSelectLocked = true;
        gui::mainWindow.tuningModeLocked = true;

        if (_this->rtlSdrWasRunning) {  
            gui::mainWindow.setPlayState(false);  
        }  

        if (sigpath::vfoManager.vfoExists("Radio")) {  
            _this->radioWasEnabled = true;  
            core::moduleManager.disableInstance("Radio");  
        }  

        tuner::tune(tuner::TUNER_MODE_CENTER, gui::waterfall.selectedVFO, MODE_S_DEFAULT_FREQ);  

        _this->decoder.init(ModeSPage::getInstance().getContext(), sigpath::sourceManager.getSelectedSource()->stream);  
        _this->decoder.start();  

        gui::mainWindow.setPlayState(true);  

        ModeSPage::getInstance().addLog("Start");  
	}
	
	static void onStop(ModeSDecoderModule* _this) {
		// If stop decoder here, pressing the "Play" button later will freeze the GUI, why?
		//_this->decoder.stop();

		gui::mainWindow.setPlayState(false);
		
        if (_this->radioWasEnabled) {
			core::moduleManager.enableInstance("Radio");
		}

        core::modComManager.callInterface("RTL-SDR", RTL_SDR_SOURCE_IFACE_CMD_SET_SAMPLE_RATE_INDEX, &_this->savedSampleRateId, NULL);

        gui::waterfall.selectedVFO = _this->savedSelectedVFO;
        
        if (gui::waterfall.selectedVFO != "") {
            ImGui::WaterfallVFO* vfo = gui::waterfall.vfos[gui::waterfall.selectedVFO];
            vfo->generalOffset = _this->savedSelectedVFOOffset;
        }
        
        gui::mainWindow.setTuningMode(_this->savedTuningMode);
        tuner::tune(_this->savedTuningMode, gui::waterfall.selectedVFO, _this->savedFreq);

		if (_this->rtlSdrWasRunning) {
			gui::mainWindow.setPlayState(true);
		}

        gui::mainWindow.waterfallLocked = false;
        gui::mainWindow.freqSelectLocked = false;
        gui::mainWindow.tuningModeLocked = false;

		ModeSPage::getInstance().addLog("Stop");
	}

    static void menuHandler(void* ctx) {
        ModeSDecoderModule* _this = (ModeSDecoderModule*)ctx;

        float menuWidth = ImGui::GetContentRegionAvail().x;

        if (!_this->enabled) { style::beginDisabled(); }

        if (ImGui::Checkbox("Mode S/ADS-B Decoder##run", &_this->run)) {
            if (_this->run) {
				onStart(_this);
            } else {
                onStop(_this);
            }
        }

        if (!_this->enabled) { style::endDisabled(); }
    }
	
	static void moduleInterfaceHandler(int code, void* in, void* out, void* ctx) {
        ModeSDecoderModule* _this = (ModeSDecoderModule*)ctx;
		
		if (code == MODE_S_DECODER_IFACE_CMD_IS_RUNNING && out) {
		    bool* _out = (bool*)out;
		    *_out = _this->run;
		} else if (code == MODE_S_DECODER_IFACE_CMD_TOGGLE_RUNNING) {
		    _this->run = !_this->run;
		    if (_this->run) {
				onStart(_this);
            } else {
                onStop(_this);
            }
		}
	}

    std::string name;
    bool enabled = true;
    bool run = false;
    bool radioWasEnabled = false;
    bool rtlSdrWasRunning = false;
    int savedSampleRateId = 0;
    double savedFreq = 0;
    int savedTuningMode;
    std::string savedSelectedVFO;
    double savedSelectedVFOOffset = 0.0f;

    dsp::ModeSDecoder decoder;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    config.setPath(core::args["root"].s() + "/mode_s_decoder_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new ModeSDecoderModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (ModeSDecoderModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
