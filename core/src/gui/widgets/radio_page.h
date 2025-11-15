#pragma once
#include <gui/widgets/side_bar.h>

class RadioPage : public SideBar::SidePage {
public:
	static RadioPage& getInstance() {
        static RadioPage instance;
        return instance;
    }
	RadioPage(const RadioPage&) = delete;
    RadioPage& operator=(const RadioPage&) = delete;
	
	const char* getLabel();
	void init();
	void deinit();
    void draw();
	
private:
	RadioPage();
    ~RadioPage();
    void controlLNA();
    
    bool hfLNA;
};
