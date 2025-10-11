#pragma once
#include <imgui.h>
#include <string>
#include <module.h>

namespace style {
	extern ImFont* smallFont;
    extern ImFont* baseFont;
    extern ImFont* bigFont;
    extern ImFont* hugeFont;
    extern ImFont* digitFont;
	extern ImFont* sidebarFont;
    extern const float uiScale;

    bool setDefaultStyle(std::string resDir);
    bool loadFonts(std::string resDir);
    void beginDisabled();
    void endDisabled();
    void testtt();
}

namespace ImGui {
    void LeftLabel(const char* text);
    void FillWidth();
}