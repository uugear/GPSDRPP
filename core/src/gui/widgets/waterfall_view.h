#pragma once
#include <gui/widgets/main_view.h>

class WaterfallView : public MainView::TabView {
public:
	static WaterfallView& getInstance() {
        static WaterfallView instance;
        return instance;
    }
	WaterfallView(const WaterfallView&) = delete;
    WaterfallView& operator=(const WaterfallView&) = delete;
    
	const char* getLabel();
	void init();
	void deinit();
    void draw();
	
	void setBandwidth(float bandwidth);
	float getBandwidth();
	
	void onZoom();

private:
	WaterfallView();
    ~WaterfallView();

    float fftMin = -70.0;
    float fftMax = 0.0;
    float prevFftMin = -70.0;
    float prevFftMax = 0.0;
    int fftHeight = 300;
    float bw = 1.0;

};
