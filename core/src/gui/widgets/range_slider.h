#pragma once
#include <imgui/imgui.h>

namespace ImGui {
	IMGUI_API bool RangeSliderFloat(const char* label, float* v1, float* v2, float v_min, float v_max, float w, float h, bool vertical, bool display_values = false, float keep_distance = -1.0f, const char* display_format = "(%.3f, %.3f)", float power = 1.0f);
}
