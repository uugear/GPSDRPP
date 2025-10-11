#pragma once
#include <imgui/imgui.h>
#include <fftw3.h>
#include <dsp/types.h>
#include <dsp/stream.h>
#include <signal_path/vfo_manager.h>
#include <string>
#include <utils/event.h>
#include <mutex>
#include <gui/tuner.h>

#define WINDOW_FLAGS ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground

class MainWindow {
public:
    void init();
    void deinit();
    void draw();
    bool sdrIsRunning();
    void setFirstMenuRender();

    static float* acquireFFTBuffer(void* ctx);
    static void releaseFFTBuffer(void* ctx);

    // TODO: Replace with it's own class
    void setVFO(double freq);

    void setPlayState(bool _playing);
    bool isPlaying();

    bool lockWaterfallControls = false;
    bool playButtonLocked = false;

    Event<bool> onPlayStateChange;
    
    int getTuningMode();

private:
    static void vfoAddedHandler(VFOManager::VFO* vfo, void* ctx);
        
    bool isSatellitesFixed();
	
	void drawMenu();
	void drawSidebar();

    // FFT Variables
    int fftSize = 8192 * 8;
    std::mutex fft_mtx;
    fftwf_complex *fft_in, *fft_out;
    fftwf_plan fftwPlan;

    // GUI Variables
    bool firstMenuRender = true;
    bool startedWithMenuClosed = false;
    bool playing = false;
    bool showCredits = false;
    std::string audioStreamName = "";
    std::string sourceName = "";
    int menuWidth = 300;
    bool grabbingMenu = false;
    int newWidth = 300;
    bool showMenu = false;
	bool showSidebar = true;
    int tuningMode = tuner::TUNER_MODE_NORMAL;
    dsp::stream<dsp::complex_t> dummyStream;
    bool demoWindow = false;
    int selectedWindow = 0;

    bool initComplete = false;
    bool autostart = false;

    EventHandler<VFOManager::VFO*> vfoCreatedHandler;
};