#include <gui/widgets/mode_s_page.h>
#include <mode_s_decoder_interface.h>
#include <rtl_sdr_source_interface.h>
#include <core.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <sstream>
#include <vector>
#include <chrono>
#include <iomanip>

#define MAX_LOG_LINES	500

/* If we don't receive new nessages within MODES_S_AIRCRAFT_INFO_TTL seconds 
 * we remove the aircraft from the list. */
void removeStaleAircrafts(struct mode_s_context * ctx) {
    if (!ctx) return;
    struct aircraft *a = ctx->aircrafts;
    struct aircraft *prev = NULL;
    time_t now = time(NULL);
    while(a) {
        if ((now - a->seen) > ctx->aircraft_info_ttl) {
            struct aircraft *next = a->next;
            /* Remove the element from the linked list, with care
             * if we are removing the first element. */
            free(a);
            if (!prev)
                ctx->aircrafts = next;
            else
                prev->next = next;
            a = next;
        } else {
            prev = a;
            a = a->next;
        }
    }
}

ModeSPage::ModeSPage() {
    
}

ModeSPage::~ModeSPage() {

}

const char* ModeSPage::getLabel() {
	return "Mode S/ADS-B";
}

void ModeSPage::init() {
	
}

void ModeSPage::deinit() {
    // Make sure to stop before exiting the app (or Radio module will stay disabled)
    if (running) {
        core::modComManager.callInterface("Mode S", MODE_S_DECODER_IFACE_CMD_TOGGLE_RUNNING, NULL, NULL);
    }
}

struct mode_s_context * ModeSPage::getContext() {
	return &ctx;
}

void ModeSPage::addLog(const std::string& message) {
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	std::stringstream ss;
	ss << std::put_time(std::localtime(&time_t), "[%H:%M:%S] ");
	
	log_content += ss.str() + message + "\n";
	
	std::vector<std::string> lines;
	std::stringstream stream(log_content);
	std::string line;
	
	while (std::getline(stream, line)) {
		lines.push_back(line);
	}
	
	if (lines.size() > MAX_LOG_LINES) {
		lines.erase(lines.begin(), lines.begin() + (lines.size() - MAX_LOG_LINES));
		log_content.clear();
		for (const auto& l : lines) {
			log_content += l + "\n";
		}
	}
}

void ModeSPage::processMessage(struct mode_s_message * mm) {
   
}

bool ModeSPage::isRunning() {
	return running;
}

void ModeSPage::draw() {
	// Mode S/ADS-B on/off
	core::modComManager.callInterface("Mode S", MODE_S_DECODER_IFACE_CMD_IS_RUNNING, NULL, &running);
    if (ImGui::Checkbox("Mode S/ADS-B", &running)) {
		core::modComManager.callInterface("Mode S", MODE_S_DECODER_IFACE_CMD_TOGGLE_RUNNING, NULL, NULL);
    }

	// Controls from source
	if (core::modComManager.interfaceExists("RTL-SDR")) {
		bool rtlAgc;
		core::modComManager.callInterface("RTL-SDR", RTL_SDR_SOURCE_IFACE_CMD_IS_RTL_AGC, NULL, &rtlAgc);
		if (ImGui::Checkbox("RTL AGC", &rtlAgc)) {
            core::modComManager.callInterface("RTL-SDR", RTL_SDR_SOURCE_IFACE_CMD_TOGGLE_RTL_AGC, NULL, NULL);
        }
		bool tunerAgc;
		core::modComManager.callInterface("RTL-SDR", RTL_SDR_SOURCE_IFACE_CMD_IS_TUNER_AGC, NULL, &tunerAgc);
        if (ImGui::Checkbox("Tuner AGC", &tunerAgc)) {
            core::modComManager.callInterface("RTL-SDR", RTL_SDR_SOURCE_IFACE_CMD_TOGGLE_TUNER_AGC, NULL, NULL);
        }
        int gainCount;
        core::modComManager.callInterface("RTL-SDR", RTL_SDR_SOURCE_IFACE_CMD_GET_GAIN_COUNT, NULL, &gainCount);
        if (!tunerAgc && gainCount > 0) {
            int gainId;
            core::modComManager.callInterface("RTL-SDR", RTL_SDR_SOURCE_IFACE_CMD_GET_GAIN_ID, NULL, &gainId);
            if (ImGui::Button("-", ImVec2(45, 35))) {
                if (gainId > 0) {
                    gainId --;
                }
                core::modComManager.callInterface("RTL-SDR", RTL_SDR_SOURCE_IFACE_CMD_SET_GAIN_ID, &gainId, NULL);
            }
            ImGui::SameLine(0, 5);
            char dbTxt[32];
            core::modComManager.callInterface("RTL-SDR", RTL_SDR_SOURCE_IFACE_CMD_GET_DB_TEXT, NULL, &dbTxt);
            ImGui::Text(dbTxt);
            ImGui::SameLine(0, 5);
            if (ImGui::Button("+", ImVec2(45, 35))) {
                if (gainId < gainCount - 1) {
                    gainId ++;
                }
                core::modComManager.callInterface("RTL-SDR", RTL_SDR_SOURCE_IFACE_CMD_SET_GAIN_ID, &gainId, NULL);
            }
        }
	}
	
    // Aircraft list
    ImGui::PushFont(style::smallFont);
    ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0, 0, 0, 0));
    if (ImGui::BeginTable("SelectableTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        
        ImGui::TableSetupColumn("ICAO", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Speed", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Altitude", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Lat", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Lon", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
            
        if (running) {
            removeStaleAircrafts(&ctx);
            struct aircraft *a = ctx.aircrafts;
            time_t current_time = time(NULL);
            while(a) {
                char label[32];
                ImGui::TableNextRow();

                const float max_time = 60.0f;
                time_t time_diff = current_time - a->seen;
                float progress = 1.0f - std::min(time_diff / max_time, 1.0f);

                ImGui::TableSetColumnIndex(0);
                ImVec2 row_start = ImGui::GetCursorScreenPos();
                ImVec2 row_end = row_start;
                if (ImGui::Selectable(a->hexaddr, &a->highlighted, ImGuiSelectableFlags_SpanAllColumns)) {
                }

                if (a->highlighted) {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, 
                        ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.7f, 0.3f)));
                }
                
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%-7d", a->speed);
                
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%-9d", a->altitude);
                
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%-7.03f", a->lat);
                
                ImGui::TableSetColumnIndex(4);
                ImVec2 col_pos = ImGui::GetCursorScreenPos();
                ImVec2 col_size = ImGui::GetContentRegionAvail();
                row_end.x = col_pos.x + col_size.x;
                ImGui::Text("%-7.03f", a->lon);
                    
                    
                float progress_height = 13.0f;
                ImVec2 progress_p0 = ImVec2(row_start.x - 5, row_start.y - 1);
                ImVec2 progress_p1 = ImVec2(row_end.x + 20, row_start.y + progress_height);
                
                if (progress > 0.0f) {
                    ImVec2 progress_end = ImVec2(progress_p0.x + (progress_p1.x - progress_p0.x) * progress, progress_p1.y);
                    
                    ImVec4 progress_color;
                    if (progress > 0.5f) {
                        progress_color = ImVec4(2.0f * (1.0f - progress), 1.0f, 0.0f, 0.3f);
                    } else {
                        progress_color = ImVec4(1.0f, 2.0f * progress, 0.0f, 0.3f);
                    }
                    ImU32 progress_color_u32 = ImGui::GetColorU32(progress_color);
                    
                    ImDrawList* draw_list = ImGui::GetWindowDrawList();
                    draw_list->AddRectFilled(progress_p0, progress_end, progress_color_u32);
                }  
                
                a = a->next;
            }
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleColor(2);
    
	// Log area
	const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing() + 25;
	if (ImGui::BeginChild("LogScrollingRegion", ImVec2(0, -footer_height_to_reserve), true)) {
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextUnformatted(log_content.c_str());
		ImGui::PopTextWrapPos();
		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
			ImGui::SetScrollHereY(1.0f);
		}
	}
	ImGui::EndChild();

	ImGui::PopFont();
}