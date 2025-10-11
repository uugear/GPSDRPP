#pragma once
#include <gui/widgets/side_bar.h>

class ClkOutPage : public SideBar::SidePage {
public:
    struct Clock {
        const char * frequency;
        unsigned char reg58;
        unsigned char reg59;
        unsigned char reg60;
        unsigned char reg61;
        unsigned char reg62;
        unsigned char reg63;
        unsigned char reg64;
        unsigned char reg65;
    };
    struct Strength {
        const char * current;
        int strength;
    };
	static ClkOutPage& getInstance() {
        static ClkOutPage instance;
        return instance;
    }
	ClkOutPage(const ClkOutPage&) = delete;
    ClkOutPage& operator=(const ClkOutPage&) = delete;
	
	const char* getLabel();
	void init();
	void deinit();
    void draw();
	
private:
	ClkOutPage();
    ~ClkOutPage();
    
    bool clkOut;
    unsigned char reg58;
    unsigned char reg59;
    unsigned char reg60;
    unsigned char reg61;
    unsigned char reg62;
    unsigned char reg63;
    unsigned char reg64;
    unsigned char reg65;
    int strength;
    
    int selClk;
    std::vector<Clock> clocks;
    std::vector<Strength> strengths;
        
    void configClock();
    void loadSettings();
    void saveSettings();
};
