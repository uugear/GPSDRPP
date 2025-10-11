#include <gui/widgets/func_select.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <gui/style.h>

namespace ImGui {

	const char *functions[] = {
		"Tuning Frequency",
		"Frequency Ruler",
		"Zooming",
		"Demodulation",
		"Volume",
	};
	const int functions_count = sizeof(functions) / sizeof(functions[0]);

    FunctionSelect::FunctionSelect(const char * identifier, int posX, int posY, int select) {
		id = (char *)identifier;
		x = posX;
		y = posY;
		sel = select;
		highlight = sel;
		popup_start_time = 0.0f;
		auto_close_delay = 2.0f;
	}

	bool FunctionSelect::isVisible() {
		return visible;
	}

	void FunctionSelect::setVisible(bool show) {
		if (show) {
			if (visible) {
				highlight ++;
				if (highlight >= functions_count) {
					highlight = 0;
				}
				popup_start_time = ImGui::GetTime();
			} else {
			    highlight = sel;
				newly_shown = true;
			}
		}
		visible = show;
	}
	
	int FunctionSelect::getSel() {
	    return sel;
	}

	void FunctionSelect::draw() {
		if (visible) {
			if (newly_shown) {
				ImGui::SetNextWindowPos(ImVec2(x, y));
				ImGui::OpenPopup(id);
				popup_start_time = ImGui::GetTime();
				newly_shown = false;
			}
			ImGuiStyle& style = ImGui::GetStyle();
			ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 5.0f);
			ImGui::PushStyleColor(ImGuiCol_Border, style.Colors[ImGuiCol_Tab]);
			ImGui::PushStyleColor(ImGuiCol_Header, style.Colors[ImGuiCol_TabUnfocusedActive]);
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, style.Colors[ImGuiCol_TabHovered]);
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, style.Colors[ImGuiCol_TabActive]);
			if (ImGui::BeginPopup(id)) {
				ImGui::PushFont(style::sidebarFont);
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(30, 30));
				    
				float elapsed_time = ImGui::GetTime() - popup_start_time;
                float remaining_time = auto_close_delay - elapsed_time;
                if (remaining_time <= 0) {
                    sel = highlight;
                    ImGui::CloseCurrentPopup();
                    visible = false;
                }
                
                ImGui::SetWindowFontScale(1.5f);
                ImVec2 textSize = ImGui::CalcTextSize(id);
                ImVec2 windowSize = ImGui::GetWindowSize();
                float centerX = (windowSize.x - textSize.x) * 0.5f;
                ImGui::SetCursorPosX(centerX);
                ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TabUnfocusedActive]);
                ImGui::Text(id);
                ImGui::PopStyleColor();
                ImGui::SetWindowFontScale(1.0f);

                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, style.Colors[ImGuiCol_TabUnfocusedActive]);                    
                float reverse_progress = std::max(0.0f, remaining_time / auto_close_delay);
                ImGui::ProgressBar(reverse_progress, ImVec2(-1, 5.0f), "");
                ImGui::PopStyleColor();
				    
				for (int i = 0; i < functions_count; i++) {
					bool is_highlighted = (i == highlight);

					if (ImGui::Selectable(functions[i], is_highlighted)) {
						sel = i;
						printf("Selected func %d\n", i);
						ImGui::CloseCurrentPopup();
						visible = false;
					}

					if (is_highlighted) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::PopStyleVar();
				ImGui::PopFont();
				ImGui::EndPopup();
			} else {
				visible = false;
			}
			ImGui::PopStyleColor(4);
			ImGui::PopStyleVar();
		}
	}
}
