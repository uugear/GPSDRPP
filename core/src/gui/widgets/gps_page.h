#pragma once
#include <gui/widgets/side_bar.h>
#include <gui/widgets/loading_animation.h>

class GPSPage : public SideBar::SidePage {
public:
    GPSPage();
    ~GPSPage();

	static GPSPage& getInstance() {
        static GPSPage instance;
        return instance;
    }
    
    const char* getLabel();
    void init();
    void deinit();
    void draw();
    void onSelected();
    void onUnselected();

private:
    void drawSatelliteTable();

    bool lockToGpsFreq = false;    
    LoadingAnimation loader;
};