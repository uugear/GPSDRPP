#include <utils/flog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <gui/smgui.h>
#include <rtl-sdr.h>
#include "rtl_sdr_source_interface.h"


#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "rtl_sdr_source",
    /* Description:     */ "RTL-SDR source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

const double sampleRates[] = {
    250000,
    1024000,
    1536000,
    1792000,
    1920000,
    2000000,
    2048000,
    2160000,
    2400000,
    2560000,
    2880000,
    3200000
};

const char* sampleRatesTxt[] = {
    "250KHz",
    "1.024MHz",
    "1.536MHz",
    "1.792MHz",
    "1.92MHz",
    "2.0MHz",
    "2.048MHz",
    "2.16MHz",
    "2.4MHz",
    "2.56MHz",
    "2.88MHz",
    "3.2MHz"
};

const char* directSamplingModesTxt = "Disabled\0I branch\0Q branch\0";

class RTLSDRSourceModule : public ModuleManager::Instance {
public:
    RTLSDRSourceModule(std::string name) {
        this->name = name;

        serverMode = (bool)core::args["server"];

        sampleRate = sampleRates[0];

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        strcpy(dbTxt, "--");

        for (int i = 0; i < 11; i++) {
            sampleRateListTxt += sampleRatesTxt[i];
            sampleRateListTxt += '\0';
        }

        refresh();

        config.acquire();
        if (!config.conf["device"].is_string()) {
            selectedDevName = "";
            config.conf["device"] = "";
        }
        else {
            selectedDevName = config.conf["device"];
        }
        config.release(true);
        selectByName(selectedDevName);

        sigpath::sourceManager.registerSource("RTL-SDR", &handler);

		// Register the module interface
        core::modComManager.registerInterface("RTL-SDR", "RTL-SDR", moduleInterfaceHandler, this);
    }

    ~RTLSDRSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("RTL-SDR");
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

    void refresh() {
        devNames.clear();
        devListTxt = "";
        devCount = rtlsdr_get_device_count();
        char buf[1024];
        char snBuf[1024];
        for (int i = 0; i < devCount; i++) {
            // Gather device info
            const char* devName = rtlsdr_get_device_name(i);
            int snErr = rtlsdr_get_device_usb_strings(i, NULL, NULL, snBuf);

            // Build name
            sprintf(buf, "[%s] %s##%d", (!snErr && snBuf[0]) ? snBuf : "No Serial", devName, i);
            devNames.push_back(buf);
            devListTxt += buf;
            devListTxt += '\0';
        }
    }

    void selectFirst() {
        if (devCount > 0) {
            selectById(0);
        }
    }

    void selectByName(std::string name) {
        for (int i = 0; i < devCount; i++) {
            if (name == devNames[i]) {
                selectById(i);
                return;
            }
        }
        selectFirst();
    }

    void selectById(int id) {
        selectedDevName = devNames[id];

        int oret = rtlsdr_open(&openDev, id);
        if (oret < 0) {
            selectedDevName = "";
            flog::error("Could not open RTL-SDR: {0}", oret);
            return;
        }

        gainList.clear();
        int gains[256];
        int n = rtlsdr_get_tuner_gains(openDev, gains);
        gainList = std::vector<int>(gains, gains + n);
        std::sort(gainList.begin(), gainList.end());

        bool created = false;
        config.acquire();
        if (!config.conf["devices"].contains(selectedDevName)) {
            created = true;
            config.conf["devices"][selectedDevName]["sampleRate"] = 2400000.0;
            config.conf["devices"][selectedDevName]["directSampling"] = directSamplingMode;
            config.conf["devices"][selectedDevName]["ppm"] = 0;
            config.conf["devices"][selectedDevName]["biasT"] = biasT;
            config.conf["devices"][selectedDevName]["offsetTuning"] = offsetTuning;
            config.conf["devices"][selectedDevName]["rtlAgc"] = rtlAgc;
            config.conf["devices"][selectedDevName]["tunerAgc"] = tunerAgc;
            config.conf["devices"][selectedDevName]["gain"] = gainId;
        }
        if (gainId >= gainList.size()) { gainId = gainList.size() - 1; }
        updateGainTxt();

        // Load config
        if (config.conf["devices"][selectedDevName].contains("sampleRate")) {
            int selectedSr = config.conf["devices"][selectedDevName]["sampleRate"];
            for (int i = 0; i < 11; i++) {
                if (sampleRates[i] == selectedSr) {
                    srId = i;
                    sampleRate = selectedSr;
                    break;
                }
            }
        }

        if (config.conf["devices"][selectedDevName].contains("directSampling")) {
            directSamplingMode = config.conf["devices"][selectedDevName]["directSampling"];
        }

        if (config.conf["devices"][selectedDevName].contains("ppm")) {
            ppm = config.conf["devices"][selectedDevName]["ppm"];
        }

        if (config.conf["devices"][selectedDevName].contains("biasT")) {
            biasT = config.conf["devices"][selectedDevName]["biasT"];
        }

        if (config.conf["devices"][selectedDevName].contains("offsetTuning")) {
            offsetTuning = config.conf["devices"][selectedDevName]["offsetTuning"];
        }

        if (config.conf["devices"][selectedDevName].contains("rtlAgc")) {
            rtlAgc = config.conf["devices"][selectedDevName]["rtlAgc"];
        }

        if (config.conf["devices"][selectedDevName].contains("tunerAgc")) {
            tunerAgc = config.conf["devices"][selectedDevName]["tunerAgc"];
        }

        if (config.conf["devices"][selectedDevName].contains("gain")) {
            gainId = config.conf["devices"][selectedDevName]["gain"];
            updateGainTxt();
        }

        config.release(created);

        rtlsdr_close(openDev);
    }

private:
    std::atomic<std::chrono::steady_clock::time_point> lastDataTime;
    std::thread watchdogThread;
    std::atomic<bool> recovering{false};

    std::string getBandwdithScaled(double bw) {
        char buf[1024];
        if (bw >= 1000000.0) {
            sprintf(buf, "%.1lfMHz", bw / 1000000.0);
        }
        else if (bw >= 1000.0) {
            sprintf(buf, "%.1lfKHz", bw / 1000.0);
        }
        else {
            sprintf(buf, "%.1lfHz", bw);
        }
        return std::string(buf);
    }

    static void menuSelected(void* ctx) {
        RTLSDRSourceModule* _this = (RTLSDRSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        flog::info("RTLSDRSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        RTLSDRSourceModule* _this = (RTLSDRSourceModule*)ctx;
        flog::info("RTLSDRSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        RTLSDRSourceModule* _this = (RTLSDRSourceModule*)ctx;
        if (_this->running) { return; }
        if (_this->selectedDevName == "") {
            flog::error("No device selected");
            return;
        }

        int oret = rtlsdr_open(&_this->openDev, _this->devId);
        if (oret < 0) {
            flog::error("Could not open RTL-SDR");
            return;
        }

        flog::info("RTL-SDR Sample Rate: {0}", _this->sampleRate);

        rtlsdr_set_sample_rate(_this->openDev, _this->sampleRate);
        rtlsdr_set_center_freq(_this->openDev, _this->freq);
        rtlsdr_set_freq_correction(_this->openDev, _this->ppm);
        rtlsdr_set_tuner_bandwidth(_this->openDev, 0);
        rtlsdr_set_direct_sampling(_this->openDev, _this->directSamplingMode);
        rtlsdr_set_bias_tee(_this->openDev, _this->biasT);
        rtlsdr_set_agc_mode(_this->openDev, _this->rtlAgc);
        rtlsdr_set_tuner_gain(_this->openDev, _this->gainList[_this->gainId]);
        if (_this->tunerAgc) {
            rtlsdr_set_tuner_gain_mode(_this->openDev, 0);
        }
        else {
            rtlsdr_set_tuner_gain_mode(_this->openDev, 1);
            rtlsdr_set_tuner_gain(_this->openDev, _this->gainList[_this->gainId]);
        }
        rtlsdr_set_offset_tuning(_this->openDev, _this->offsetTuning);

        _this->asyncCount = (int)roundf(_this->sampleRate / (200 * 512)) * 512;

        _this->running = true;

        _this->workerThread = std::thread(&RTLSDRSourceModule::worker, _this);

        _this->lastDataTime.store(std::chrono::steady_clock::now());
        _this->watchdogThread = std::thread([_this]() {
            while (_this->running) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                auto now = std::chrono::steady_clock::now();
                auto last = _this->lastDataTime.load();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count();
                if (elapsed > 70 && !_this->recovering.load()) {
                    _this->recovering.store(true);
                    rtlsdr_cancel_async(_this->openDev);
                }
            }
        });

        flog::info("RTLSDRSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        RTLSDRSourceModule* _this = (RTLSDRSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        _this->stream.stopWriter();
        rtlsdr_cancel_async(_this->openDev);
        if (_this->watchdogThread.joinable()) { _this->watchdogThread.join(); }
        if (_this->workerThread.joinable()) { _this->workerThread.join(); }
        _this->stream.clearWriteStop();
        rtlsdr_close(_this->openDev);
        flog::info("RTLSDRSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        RTLSDRSourceModule* _this = (RTLSDRSourceModule*)ctx;
        if (_this->running) {
            uint32_t newFreq = freq;
            int i;
            for (i = 0; i < 10; i++) {
                rtlsdr_set_center_freq(_this->openDev, freq);
                if (rtlsdr_get_center_freq(_this->openDev) == newFreq) { break; }
            }
            if (i > 1) {
                flog::warn("RTL-SDR took {0} attempts to tune...", i);
            }
        }
        _this->freq = freq;
        flog::info("RTLSDRSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void onToggleRtlAgc(void* ctx) {
        RTLSDRSourceModule* _this = (RTLSDRSourceModule*)ctx;
        if (_this->running) {
            rtlsdr_set_agc_mode(_this->openDev, _this->rtlAgc);
        }
        if (_this->selectedDevName != "") {
            config.acquire();
            config.conf["devices"][_this->selectedDevName]["rtlAgc"] = _this->rtlAgc;
            config.release(true);
        }
    }

    static void onToggleTunerAgc(void* ctx) {
        RTLSDRSourceModule* _this = (RTLSDRSourceModule*)ctx;
        if (_this->running) {
            if (_this->tunerAgc) {
                rtlsdr_set_tuner_gain_mode(_this->openDev, 0);
            }
            else {
                rtlsdr_set_tuner_gain_mode(_this->openDev, 1);
                rtlsdr_set_tuner_gain(_this->openDev, _this->gainList[_this->gainId]);
            }
        }
        if (_this->selectedDevName != "") {
            config.acquire();
            config.conf["devices"][_this->selectedDevName]["tunerAgc"] = _this->tunerAgc;
            config.release(true);
        }
    }

    static void onToggleBiasT(void* ctx) {
        RTLSDRSourceModule* _this = (RTLSDRSourceModule*)ctx;
        if (_this->running) {
            rtlsdr_set_bias_tee(_this->openDev, _this->biasT);
        }
        if (_this->selectedDevName != "") {
            config.acquire();
            config.conf["devices"][_this->selectedDevName]["biasT"] = _this->biasT;
            config.release(true);
        }
    }

    static void menuHandler(void* ctx) {
        RTLSDRSourceModule* _this = (RTLSDRSourceModule*)ctx;

        if (_this->running) { SmGui::BeginDisabled(); }
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_rtlsdr_dev_sel_", _this->name), &_this->devId, _this->devListTxt.c_str())) {
            _this->selectById(_this->devId);
            core::setInputSampleRate(_this->sampleRate);
            if (_this->selectedDevName != "") {
                config.acquire();
                config.conf["device"] = _this->selectedDevName;
                config.release(true);
            }
        }

        if (SmGui::Combo(CONCAT("##_rtlsdr_sr_sel_", _this->name), &_this->srId, _this->sampleRateListTxt.c_str())) {
            _this->sampleRate = sampleRates[_this->srId];
            core::setInputSampleRate(_this->sampleRate);
            if (_this->selectedDevName != "") {
                config.acquire();
                config.conf["devices"][_this->selectedDevName]["sampleRate"] = _this->sampleRate;
                config.release(true);
            }
        }

        SmGui::SameLine();
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(CONCAT("Refresh##_rtlsdr_refr_", _this->name)/*, ImVec2(refreshBtnWdith, 0)*/)) {
            _this->refresh();
            _this->selectByName(_this->selectedDevName);
            core::setInputSampleRate(_this->sampleRate);
        }

        if (_this->running) { SmGui::EndDisabled(); }

        // Rest of rtlsdr config here
        SmGui::LeftLabel("Direct Sampling");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_rtlsdr_ds_", _this->name), &_this->directSamplingMode, directSamplingModesTxt)) {
            if (_this->running) {
                rtlsdr_set_direct_sampling(_this->openDev, _this->directSamplingMode);

                // Update gains (fix for librtlsdr bug)
                if (_this->directSamplingMode == false) {
                    rtlsdr_set_agc_mode(_this->openDev, _this->rtlAgc);
                    if (_this->tunerAgc) {
                        rtlsdr_set_tuner_gain_mode(_this->openDev, 0);
                    }
                    else {
                        rtlsdr_set_tuner_gain_mode(_this->openDev, 1);
                        rtlsdr_set_tuner_gain(_this->openDev, _this->gainList[_this->gainId]);
                    }
                }
            }
            if (_this->selectedDevName != "") {
                config.acquire();
                config.conf["devices"][_this->selectedDevName]["directSampling"] = _this->directSamplingMode;
                config.release(true);
            }
        }

        SmGui::LeftLabel("PPM Correction");
        SmGui::FillWidth();
        if (SmGui::InputInt(CONCAT("##_rtlsdr_ppm_", _this->name), &_this->ppm, 1, 10)) {
            _this->ppm = std::clamp<int>(_this->ppm, -1000000, 1000000);
            if (_this->running) {
                rtlsdr_set_freq_correction(_this->openDev, _this->ppm);
            }
            if (_this->selectedDevName != "") {
                config.acquire();
                config.conf["devices"][_this->selectedDevName]["ppm"] = _this->ppm;
                config.release(true);
            }
        }

        if (_this->tunerAgc || _this->gainList.size() == 0) { SmGui::BeginDisabled(); }

        SmGui::LeftLabel("Gain");
        SmGui::FillWidth();
        SmGui::ForceSync();
        // TODO: FIND ANOTHER WAY
        if (_this->serverMode) {
            if (SmGui::SliderInt(CONCAT("##_rtlsdr_gain_", _this->name), &_this->gainId, 0, _this->gainList.size() - 1, SmGui::FMT_STR_NONE)) {
                _this->updateGainTxt();
                if (_this->running) {
                    rtlsdr_set_tuner_gain(_this->openDev, _this->gainList[_this->gainId]);
                }
                if (_this->selectedDevName != "") {
                    config.acquire();
                    config.conf["devices"][_this->selectedDevName]["gain"] = _this->gainId;
                    config.release(true);
                }
            }
        }
        else {
            if (ImGui::SliderInt(CONCAT("##_rtlsdr_gain_", _this->name), &_this->gainId, 0, _this->gainList.size() - 1, _this->dbTxt)) {
                _this->updateGainTxt();
                if (_this->running) {
                    rtlsdr_set_tuner_gain(_this->openDev, _this->gainList[_this->gainId]);
                }
                if (_this->selectedDevName != "") {
                    config.acquire();
                    config.conf["devices"][_this->selectedDevName]["gain"] = _this->gainId;
                    config.release(true);
                }
            }
        }


        if (_this->tunerAgc || _this->gainList.size() == 0) { SmGui::EndDisabled(); }

        if (SmGui::Checkbox(CONCAT("Bias T##_rtlsdr_rtl_biast_", _this->name), &_this->biasT)) {
            onToggleBiasT(ctx);
        }

        // If Up-Converter is enabled, update offsetTuning
        if (core::upConverter.isEnabled() != _this->offsetTuning) {
            _this->offsetTuning = core::upConverter.isEnabled();
            if (_this->running) {
                rtlsdr_set_offset_tuning(_this->openDev, _this->offsetTuning);
            }
            if (_this->selectedDevName != "") {
                config.acquire();
                config.conf["devices"][_this->selectedDevName]["offsetTuning"] = _this->offsetTuning;
                config.release(true);
            }
        }

        if (SmGui::Checkbox(CONCAT("Offset Tuning##_rtlsdr_rtl_ofs_", _this->name), &_this->offsetTuning)) {
            if (_this->running) {
                rtlsdr_set_offset_tuning(_this->openDev, _this->offsetTuning);
            }
            if (_this->selectedDevName != "") {
                config.acquire();
                config.conf["devices"][_this->selectedDevName]["offsetTuning"] = _this->offsetTuning;
                config.release(true);
            }
        }

        if (SmGui::Checkbox(CONCAT("RTL AGC##_rtlsdr_rtl_agc_", _this->name), &_this->rtlAgc) || _this->toggleRtlAgc) {
            onToggleRtlAgc(ctx);
        }

        SmGui::ForceSync();
        if (SmGui::Checkbox(CONCAT("Tuner AGC##_rtlsdr_tuner_agc_", _this->name), &_this->tunerAgc) || _this->toggleTunerAgc) {
            onToggleTunerAgc(ctx);
        }
    }

    void worker() {
        while (running) {
            rtlsdr_reset_buffer(openDev);
            rtlsdr_read_async(openDev, asyncHandler, this, 0, asyncCount);
            if (running) {
                flog::warn("rtlsdr_read_async exited, retrying...");
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    static void asyncHandler(unsigned char* buf, uint32_t len, void* ctx) {
        RTLSDRSourceModule* _this = (RTLSDRSourceModule*)ctx;

        auto now = std::chrono::steady_clock::now();
        auto last = _this->lastDataTime.load();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count();
        if (elapsed > 200 && _this->recovering.load()) {
            flog::info("Data flow recovered! Gap was {}ms", elapsed);
            _this->recovering.store(false);
        }

        _this->lastDataTime.store(now);

        int sampCount = len / 2;
        for (int i = 0; i < sampCount; i++) {
            _this->stream.writeBuf[i].re = ((float)buf[i * 2] - 127.4) / 128.0f;
            _this->stream.writeBuf[i].im = ((float)buf[(i * 2) + 1] - 127.4) / 128.0f;
        }
        if (!_this->stream.swap(sampCount)) { return; }
    }

    void updateGainTxt() {
        sprintf(dbTxt, "%.1f dB", (float)gainList[gainId] / 10.0f);
    }

	static void moduleInterfaceHandler(int code, void* in, void* out, void* ctx) {
        RTLSDRSourceModule* _this = (RTLSDRSourceModule*)ctx;

		if (code == RTL_SDR_SOURCE_IFACE_CMD_GET_GAIN_COUNT && out) {
		    int* _out = (int*)out;
		    *_out = _this->gainList.size();
		} else if (code == RTL_SDR_SOURCE_IFACE_CMD_GET_GAIN_ID && out) {
		    int* _out = (int*)out;
		    *_out = _this->gainId;
		} else if (code == RTL_SDR_SOURCE_IFACE_CMD_SET_GAIN_ID && in) {
		    int* _in = (int*)in;
		    _this->gainId = *_in;
		    if (_this->running) {
    		    rtlsdr_set_tuner_gain_mode(_this->openDev, 1);
                rtlsdr_set_tuner_gain(_this->openDev, _this->gainList[_this->gainId]);
            }
            _this->updateGainTxt();
            if (_this->selectedDevName != "") {
                config.acquire();
                config.conf["devices"][_this->selectedDevName]["gain"] = _this->gainId;
                config.release(true);
            }
		} else if (code == RTL_SDR_SOURCE_IFACE_CMD_GET_DB_TEXT && out) {
		    char* _out = (char*)out;
		    strcpy(_out, _this->dbTxt);
		} else if (code == RTL_SDR_SOURCE_IFACE_CMD_IS_RUNNING && out) {
            bool* _out = (bool*)out;
            *_out = _this->running;
        } else if (code == RTL_SDR_SOURCE_IFACE_CMD_IS_RTL_AGC && out) {
			bool* _out = (bool*)out;
            *_out = _this->rtlAgc;
		} else if (code == RTL_SDR_SOURCE_IFACE_CMD_IS_TUNER_AGC && out) {
			bool* _out = (bool*)out;
            *_out = _this->tunerAgc;
		} else if (code == RTL_SDR_SOURCE_IFACE_CMD_IS_BIAS_T && out) {
			bool* _out = (bool*)out;
            *_out = _this->biasT;
		} else if (code == RTL_SDR_SOURCE_IFACE_CMD_TOGGLE_RTL_AGC) {
            _this->rtlAgc = !_this->rtlAgc;
			onToggleRtlAgc(ctx);
        } else if (code == RTL_SDR_SOURCE_IFACE_CMD_TOGGLE_TUNER_AGC) {
            _this->tunerAgc = !_this->tunerAgc;
			onToggleTunerAgc(ctx);
        } else if (code == RTL_SDR_SOURCE_IFACE_CMD_TOGGLE_BIAS_T) {
            _this->biasT = !_this->biasT;
			onToggleBiasT(ctx);
        } else if (code == RTL_SDR_SOURCE_IFACE_CMD_GET_SAMPLE_RATE_INDEX && out) {
            int* _out = (int*)out;
            *_out = _this->srId;
        } else if (code == RTL_SDR_SOURCE_IFACE_CMD_SET_SAMPLE_RATE_INDEX && in) {
            int* _in = (int*)in;
            _this->srId = *_in;
            _this->sampleRate = sampleRates[_this->srId];
            core::setInputSampleRate(_this->sampleRate);
        } else if (code == RTL_SDR_SOURCE_IFACE_CMD_GET_DEVICE_COUNT && out) {
			int* _out = (int*)out;
		    *_out = rtlsdr_get_device_count();
		} else if (code == RTL_SDR_SOURCE_IFACE_CMD_REFRESH) {
			_this->refresh();
			_this->selectByName(_this->selectedDevName);
            core::setInputSampleRate(_this->sampleRate);
		}
	}

    std::string name;
    rtlsdr_dev_t* openDev;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    double sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    double freq;
    std::string selectedDevName = "";
    int devId = 0;
    int srId = 0;
    int devCount = 0;
    std::thread workerThread;
    bool serverMode = false;

    int ppm = 0;

    bool biasT = false;

    int gainId = 0;
    std::vector<int> gainList;

    bool rtlAgc = false;
    bool tunerAgc = false;
    bool offsetTuning = false;

    int directSamplingMode = 0;

    // Handler stuff
    int asyncCount = 0;

    char dbTxt[128];

    std::vector<std::string> devNames;
    std::string devListTxt;
    std::string sampleRateListTxt;

    bool toggleBiasT = false;
    bool toggleRtlAgc = false;
    bool toggleTunerAgc = false;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["devices"] = json({});
    def["device"] = 0;
    config.setPath(core::args["root"].s() + "/rtl_sdr_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new RTLSDRSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (RTLSDRSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}