#pragma once
#include <module.h>
#include <gui/widgets/waterfall.h>
#include <gui/widgets/frequency_select.h>
#include <gui/widgets/menu.h>
#include <gui/dialogs/loading_screen.h>
#include <gui/main_window.h>
#include <gui/theme_manager.h>
#include <gui/widgets/side_bar.h>
#include <gui/widgets/main_view.h>
#include <gui/widgets/func_select.h>

namespace gui {
    SDRPP_EXPORT ImGui::WaterFall waterfall;
    SDRPP_EXPORT FrequencySelect freqSelect;
    SDRPP_EXPORT Menu menu;
    SDRPP_EXPORT ThemeManager themeManager;
    SDRPP_EXPORT MainWindow mainWindow;
    extern SideBar sideBar;
    extern MainView mainView;
	extern ImGui::FunctionSelect funcSelectA;
	extern ImGui::FunctionSelect funcSelectB;

    void selectSource(std::string name);
};