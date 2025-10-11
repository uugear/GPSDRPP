#include <gui/widgets/main_view.h>
#include <cstdio>
#include <string>
#include <algorithm>
#include <cctype>
#include <gui/style.h>
#include <gui/gui.h>
#include <gui/icons.h>
#include <core.h>
#include <gui/widgets/waterfall_view.h>
#include <gui/widgets/map_view.h>

#define BOTTOM_BAR_TAB_SCALE            1.6f
#define BOTTOM_BAR_TAB_GAP              5
#define BOTTOM_BAR_TAB_PADDING          100

#define BOTTOM_BAR_STATUS_ICON_SIZE     28
#define BOTTOM_BAR_STATUS_ICON_SPACE    40


MainView::MainView() {

}

MainView::~MainView() {

}

void MainView::init() {
	
	tabs.push_back(&WaterfallView::getInstance());
	tabs.push_back(&MapView::getInstance());
	
	for (auto tab : tabs) {
		tab->init();
	}
	
    icons = {
        {"SatFixed", icons::SAT_FIXED, [this]() { return (core::gps.getFixQuality() != 0); }},
        {"UpConv", icons::UP_CONV, [this]() { return core::upConverter.isEnabled(); }},
        {"GpuEnabled", icons::GPU_ENABLED, [this]() { return isGPUEnabled(); }},
    };

	selTabDecided = false;
	selectedTab = 0;
}

void MainView::deinit() {
    for (auto tab : tabs) {
		tab->deinit();
	}
}

bool MainView::drawMainView() {
    
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
	ImGui::BeginChild("Main View", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    
    bool selTabChanged = false;
    ImGuiStyle& style = ImGui::GetStyle();
    
    float tabHeight = ImGui::GetFrameHeightWithSpacing() * BOTTOM_BAR_TAB_SCALE;
    
    // Draw tab view
    int tabCount = tabs.size();
    ImGui::BeginChild("Content", ImVec2(0, - tabHeight - 5.0f), false);
    if (selectedTab >= 0 && selectedTab < tabCount) {
        tabs[selectedTab]->draw();
    }
    ImGui::EndChild();
    
    // Tabs
    ImGui::PushFont(style::baseFont);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    for (int i = 0; i < tabCount; i++) {
        if (i > 0) {
            ImGui::SameLine();
        }
        bool isSelected = (i == selectedTab);

		ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[isSelected ? ImGuiCol_TabActive : ImGuiCol_Tab]);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, style.Colors[ImGuiCol_TabHovered]);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, style.Colors[ImGuiCol_TabActive]);

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + BOTTOM_BAR_TAB_GAP);
        float buttonWidth = ImGui::CalcTextSize(tabs[i]->getLabel()).x + BOTTOM_BAR_TAB_PADDING;
        if (ImGui::Button(tabs[i]->getLabel(), ImVec2(buttonWidth, tabHeight + (selectedTab == i ? 5.0f : 0.0f)))) {
            if (selectedTab != i) {
                selectedTab = i;
                selTabChanged = true;
				core::configManager.acquire();
				core::configManager.conf["selectedTab"] = selectedTab;
				core::configManager.release(true);
            }
        }
        
        ImGui::PopStyleColor(3);
    }
    ImGui::PopStyleVar(2);
    ImGui::PopFont();
        
    // Status icons
    int iconCount = icons.size();
    ImVec4 onStateColor = style.Colors[ImGuiCol_SliderGrab];
    ImVec4 offStateColor = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
	float winWidth = ImGui::GetWindowSize().x;
	float curPosX = ImGui::GetCursorPosX();
    for (int i = 0; i < iconCount; i++) {
		ImGui::SameLine(winWidth - curPosX - (i + 1) * BOTTOM_BAR_STATUS_ICON_SPACE);
        bool sChanged = icons[i].checkStatusFunc();
        ImGui::Image((void*)(intptr_t)icons[i].txId, ImVec2(BOTTOM_BAR_STATUS_ICON_SIZE, BOTTOM_BAR_STATUS_ICON_SIZE), 
                     ImVec2(0,0), ImVec2(1,1),
                     sChanged ? onStateColor : offStateColor);
    }
    
    ImGui::EndChild();
	ImGui::PopStyleVar();
	
	if (!selTabDecided) {
		core::configManager.acquire();
		int bkSelTab = selectedTab;
		selectedTab = core::configManager.conf["selectedTab"];
		core::configManager.release();
		selTabDecided = true;
		selTabChanged = (bkSelTab != selectedTab);
	}
    
    return selTabChanged;
}

bool MainView::isGPUEnabled() {
    static bool cached = false;
    static bool gpuEnabled = false;
    if (cached) {
        return gpuEnabled;
    }
    FILE* pipe = popen("glxinfo -B | grep \"Device:\"", "r");
    if (!pipe) {
        cached = true;
        gpuEnabled = false;
        return gpuEnabled;
    }
    char buffer[256];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    std::string lowerResult = result;
    std::transform(lowerResult.begin(), lowerResult.end(), lowerResult.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    if (lowerResult.find("softpipe") != std::string::npos || lowerResult.find("llvmpipe") != std::string::npos) {
        gpuEnabled = false;
    } else {
        gpuEnabled = true;
    }
    cached = true;
    return gpuEnabled;
}