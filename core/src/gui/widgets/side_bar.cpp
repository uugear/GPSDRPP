#include <gui/widgets/side_bar.h>
#include <core.h>
#include <gui/icons.h>
#include <gui/style.h>

#include <gui/widgets/radio_page.h>
#include <gui/widgets/mode_s_page.h>
#include <gui/widgets/gps_page.h>
#include <gui/widgets/clk_out_page.h>


#define SIDE_BAR_PADDING            10
#define SIDE_BAR_PAGE_TITLE_OFFSET  9
#define SIDE_BAR_PAGE_COUNT         4


SideBar::SideBar() {

}

SideBar::~SideBar() {

}

void SideBar::init() {

	pages.push_back(&RadioPage::getInstance());
	pages.push_back(&ModeSPage::getInstance());
	pages.push_back(&GPSPage::getInstance());
	pages.push_back(&ClkOutPage::getInstance());

	for (auto page : pages) {
		page->init();
	}

	core::configManager.acquire();
	selectedPage = core::configManager.conf["selectedPage"];
    core::configManager.release();

    pages[selectedPage]->onSelected();
}

void SideBar::deinit() {
    for (auto page : pages) {
		page->deinit();
	}
}

void SideBar::drawSideBar() {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
	ImGui::NextColumn();

	ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
    ImGui::BeginChild("Sidebar", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::PushFont(style::sidebarFont);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 15.0f));

    // Current page
    ImGui::Spacing();
    pages[selectedPage]->draw();

    float remainingHeight = ImGui::GetContentRegionAvail().y;
    float navigatorHeight = ImGui::GetFrameHeightWithSpacing() * 1.15f;
    ImGui::Dummy(ImVec2(0, remainingHeight - navigatorHeight));

    ImGui::PopStyleVar();
    ImGui::PopFont();

    // Page navigator
    ImVec4 textCol = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    ImVec2 btnSize(30, 30);

    float columnWidth = ImGui::GetContentRegionAvail().x;
    float startY = ImGui::GetCursorPosY();

    bool isDisabled = (selectedPage == 0);
    if (isDisabled) {
        ImGui::BeginDisabled();
    }
    ImGui::ImageButton(icons::PREV, btnSize, ImVec2(0, 0), ImVec2(1, 1), 5, ImVec4(0, 0, 0, 0), textCol);
    if (ImGui::IsItemClicked() || ImGui::IsKeyPressed(ImGuiKey_End, false)) {
        if (selectedPage > 0) {
            pages[selectedPage]->onUnselected();
            selectedPage --;
            pages[selectedPage]->onSelected();
            core::configManager.acquire();
			core::configManager.conf["selectedPage"] = selectedPage;
			core::configManager.release(true);
        }
    }
    if (isDisabled) {
        ImGui::EndDisabled();
    }

    ImGui::PushFont(style::baseFont);
    const char* pageLabel = pages[selectedPage]->getLabel();
    float textWidth = ImGui::CalcTextSize(pageLabel).x;
    float textX = (columnWidth - textWidth) * 0.5f;
    ImGui::SameLine(0, 0);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() - ImGui::GetCursorPosX() + textX);
    ImGui::SetCursorPosY(startY + SIDE_BAR_PAGE_TITLE_OFFSET);
    ImGui::AlignTextToFramePadding();
    ImGui::Text(pageLabel);
    ImGui::PopFont();

    ImGui::SameLine();
    float rightBtnX = columnWidth - btnSize.x - SIDE_BAR_PADDING;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() - ImGui::GetCursorPosX() + rightBtnX);
    ImGui::SetCursorPosY(startY);
    isDisabled = (selectedPage == SIDE_BAR_PAGE_COUNT - 1);
    if (isDisabled) {
        ImGui::BeginDisabled();
    }
    ImGui::ImageButton(icons::NEXT, btnSize, ImVec2(0, 0), ImVec2(1, 1), 5, ImVec4(0, 0, 0, 0), textCol);
    if (ImGui::IsItemClicked() || ImGui::IsKeyPressed(ImGuiKey_End, false)) {
        if (selectedPage < SIDE_BAR_PAGE_COUNT - 1) {
            pages[selectedPage]->onUnselected();
            selectedPage ++;
            pages[selectedPage]->onSelected();
            core::configManager.acquire();
			core::configManager.conf["selectedPage"] = selectedPage;
			core::configManager.release(true);
        }
    }
    if (isDisabled) {
        ImGui::EndDisabled();
    }

	ImGui::EndChild();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();
}
