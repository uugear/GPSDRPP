#pragma once
#include <imgui/imgui.h>
#include <functional>
#include <vector>

class MainView {
public:
    MainView();
    ~MainView();
	
    class TabView {
    public:
		virtual const char* getLabel() = 0;
		virtual void init() = 0;
		virtual void deinit() = 0;
        virtual void draw() = 0;
        virtual ~TabView() = default;
    };
	
	void init();
	
	void deinit();

    bool drawMainView();
        
private:
    
    struct BottomBarStatusIcon {
        const char* label;
        ImTextureID txId;
        std::function<bool()> checkStatusFunc;  // Returns status (true/on or false/off)
    };
    
    bool isGPUEnabled();

	bool selTabDecided;
    int selectedTab = 0;
    std::vector<TabView *> tabs;
    std::vector<BottomBarStatusIcon> icons;
};