#include <gui/gui.h>

namespace gui {
    MainWindow mainWindow;
    ImGui::WaterFall waterfall;
    FrequencySelect freqSelect;
    ThemeManager themeManager;
    Menu menu;
    SideBar sideBar;
    MainView mainView;
	ImGui::FunctionSelect funcSelectA("Encoder A:", 1050, 10, 0);
	ImGui::FunctionSelect funcSelectB("Encoder B:", 150, 10, 2);
};