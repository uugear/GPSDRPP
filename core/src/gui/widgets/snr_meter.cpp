#include <gui/widgets/volume_meter.h>
#include <algorithm>
#include <gui/style.h>

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <imgui/imgui_internal.h>

#define SNR_METER_BAR_HEIGHT    40

namespace ImGui {
    void SNRMeter(float val, const ImVec2& size_arg) {
        ImGuiWindow* window = GetCurrentWindow();
        ImGuiStyle& style = GImGui->Style;

        ImVec2 min = window->DC.CursorPos;
        ImVec2 size = CalcItemSize(size_arg, CalcItemWidth(), 26);
        ImRect bb(min, min + size);

        ImU32 text = ImGui::GetColorU32(ImGuiCol_Text);

        ItemSize(size, style.FramePadding.y);
        if (!ItemAdd(bb, 0)) {
            return;
        }

        val = std::clamp<float>(val, 0, 100);
        float ratio = size.x / 90;
        float it = size.x / 9;
        char buf[32];

		ImU32 fill = ImGui::GetColorU32(ImGuiCol_CheckMark);
        window->DrawList->AddRectFilled(min + ImVec2(0, 1), min + ImVec2(roundf((float)val * ratio), SNR_METER_BAR_HEIGHT), fill);
        window->DrawList->AddRect(min + ImVec2(0, 1), min + ImVec2(size.x + 1, (SNR_METER_BAR_HEIGHT)), text);
        
        for (int i = 1; i < 9; i++) {
            window->DrawList->AddLine(min + ImVec2(roundf((float)i * it), 0), min + ImVec2(roundf((float)i * it), 5.0f), text, 1.0f);
            window->DrawList->AddLine(min + ImVec2(roundf((float)i * it), SNR_METER_BAR_HEIGHT - 1), min + ImVec2(roundf((float)i * it), (SNR_METER_BAR_HEIGHT - 5.0f) - 1), text, 1.0f);
            sprintf(buf, "%d", i * 10);
            ImVec2 sz = ImGui::CalcTextSize(buf);
            window->DrawList->AddText(min + ImVec2(roundf(((float)i * it) - (sz.x / 2.0)) + 1, 13.0f), text, buf);
        }
    }
}