#include <gui/style.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <config.h>
#include <utils/flog.h>
#include <filesystem>

namespace style {
	ImFont* smallFont;
    ImFont* baseFont;
    ImFont* bigFont;
    ImFont* hugeFont;
    ImFont* digitFont;
	ImFont* sidebarFont;
    ImVector<ImWchar> baseRanges;
    ImVector<ImWchar> bigRanges;
    ImVector<ImWchar> hugeRanges;
    ImVector<ImWchar> digitRanges;

    const float uiScale = 1.0f;

    bool loadFonts(std::string resDir) {
        ImFontAtlas* fonts = ImGui::GetIO().Fonts;
        if (!std::filesystem::is_directory(resDir)) {
            flog::error("Invalid resource directory: {0}", resDir);
            return false;
        }
		
        // Create base font range
        ImFontGlyphRangesBuilder baseBuilder;
        baseBuilder.AddRanges(fonts->GetGlyphRangesDefault());
        baseBuilder.BuildRanges(&baseRanges);

        // Create big font range
        ImFontGlyphRangesBuilder bigBuilder;
        bigBuilder.AddRanges(fonts->GetGlyphRangesDefault());
        bigBuilder.BuildRanges(&bigRanges);

        // Create huge font range
        ImFontGlyphRangesBuilder hugeBuilder;
		hugeBuilder.AddChar('G');
		hugeBuilder.AddChar('P');
		hugeBuilder.AddChar('S');
		hugeBuilder.AddChar('D');
		hugeBuilder.AddChar('R');
		hugeBuilder.AddChar('+');
		hugeBuilder.AddChar(' ');
        hugeBuilder.BuildRanges(&hugeRanges);
        
        // Create digit font range
        ImFontGlyphRangesBuilder digitBuilder;
        const ImWchar digitRange[] = { '.', '9', 0 };
        digitBuilder.AddRanges(digitRange);
        digitBuilder.BuildRanges(&digitRanges);
        
        // Add bigger fonts for frequency select and title
		smallFont = fonts->AddFontFromFileTTF(((std::string)(resDir + "/fonts/Roboto-Medium.ttf")).c_str(), 11.0f, NULL, baseRanges.Data);
        baseFont = fonts->AddFontFromFileTTF(((std::string)(resDir + "/fonts/Roboto-Medium.ttf")).c_str(), 16.0f, NULL, baseRanges.Data);
        bigFont = fonts->AddFontFromFileTTF(((std::string)(resDir + "/fonts/Roboto-Medium.ttf")).c_str(), 35.0f, NULL, bigRanges.Data);
        hugeFont = fonts->AddFontFromFileTTF(((std::string)(resDir + "/fonts/Roboto-Medium.ttf")).c_str(), 128.0f, NULL, hugeRanges.Data);
        digitFont = fonts->AddFontFromFileTTF(((std::string)(resDir + "/fonts/7Seg.otf")).c_str(), 85.0f, NULL, digitRanges.Data);
		sidebarFont = fonts->AddFontFromFileTTF(((std::string)(resDir + "/fonts/Roboto-Medium.ttf")).c_str(), 26.0f, NULL, baseRanges.Data);

        return true;
    }

    void beginDisabled() {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        auto& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;
        ImVec4 btnCol = colors[ImGuiCol_Button];
        ImVec4 frameCol = colors[ImGuiCol_FrameBg];
        ImVec4 textCol = colors[ImGuiCol_Text];
        btnCol.w = 0.15f;
        frameCol.w = 0.30f;
        textCol.w = 0.65f;
        ImGui::PushStyleColor(ImGuiCol_Button, btnCol);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, frameCol);
        ImGui::PushStyleColor(ImGuiCol_Text, textCol);
    }

    void endDisabled() {
        ImGui::PopItemFlag();
        ImGui::PopStyleColor(3);
    }
}

namespace ImGui {
    void LeftLabel(const char* text) {
        float vpos = ImGui::GetCursorPosY();
        ImGui::SetCursorPosY(vpos + GImGui->Style.FramePadding.y);
        ImGui::TextUnformatted(text);
        ImGui::SameLine();
        ImGui::SetCursorPosY(vpos);
    }

    void FillWidth() {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    }
}
