#pragma once
#include <imgui/imgui.h>
#include <vector>


class SideBar {
public:
    SideBar();
    ~SideBar();
    
    class SidePage {
        friend class SideBar;
    public:
		virtual const char* getLabel() = 0;
		virtual void init() = 0;
		virtual void deinit() = 0;
        virtual void draw() = 0;
        virtual ~SidePage() = default;
        bool isSelected() { return selected; }
    protected:
        virtual void onSelected() { selected = true; }
        virtual void onUnselected() { selected = false; }
    private:
        bool selected;
    };
    
    void init();
	void deinit();
    void drawSideBar();

private:
    std::vector<SidePage *> pages;
    int selectedPage = 0;
};
