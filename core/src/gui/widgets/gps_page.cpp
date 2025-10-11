#include <gui/widgets/gps_page.h>
#include <core.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <imgui.h>
#include <sstream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <cstdlib>
#include <cctype>
#include <iostream>
#include <string>


GPSPage::GPSPage() {

}

GPSPage::~GPSPage() {

}

const char* GPSPage::getLabel() {
    return "GPS";
}

void GPSPage::init() {
    
}

void GPSPage::deinit() {

}

void GPSPage::draw() {
    const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing() + 10;
    if (ImGui::BeginChild("GPS_Content", ImVec2(0, -footer_height_to_reserve), true)) {
        ImGui::Text("Status: ");
        ImGui::SameLine();
        if (core::gps.getFixQuality() != 0) {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "FIXED");
            ImGui::Text("Satellites: %d/%zu", core::gps.getUsedSatellites(), core::gps.getSatellites().size());
        } else {
            ImGui::Text("%c", loader.getLoadingText());
            ImGui::Text("Satellites: %zu", core::gps.getSatellites().size());
        }
        ImGui::Text("NMEAs: %d", core::gps.getProcessedNMEA());
        drawSatelliteTable();
    }
    ImGui::EndChild();
}

void GPSPage::drawSatelliteTable() {
    ImGui::PushFont(style::smallFont);
    ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0, 0, 0, 0));
    if (ImGui::BeginTable("SatelliteTable", 3, 
                         ImGuiTableFlags_Borders | 
                         ImGuiTableFlags_RowBg | 
                         ImGuiTableFlags_ScrollY)) {
        
        ImGui::TableSetupColumn("SYSTEM", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("PRN", ImGuiTableColumnFlags_WidthFixed, 25.0f);
        ImGui::TableSetupColumn("C/NO (dB-Hz)", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        
        // sort satellites by system
        std::vector<std::string> systems = {"GPS", "GLONASS", "BEIDOU", "GALILEO", "SBAS", "QZSS", "UNKNOWN"};
        
        std::vector<Satellite> satellites = core::gps.getSatellites();
        
        for (const auto& system : systems) {
            std::vector<Satellite> system_sats;
            
            // collect satellites in this system
            for (const auto& sat : satellites) {
                if (sat.system == system && sat.valid) {
                    system_sats.push_back(sat);
                }
            }
            
            // sort by PRN
            std::sort(system_sats.begin(), system_sats.end(), 
                     [](const Satellite& a, const Satellite& b) {
                         return a.prn < b.prn;
                     });
            
            // show satellites for the system
            for (const auto& sat : system_sats) {
                ImGui::TableNextRow();
                
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", sat.system.c_str());
                
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%d", sat.prn);
                
                ImGui::TableSetColumnIndex(2);
                if (sat.cno > 0) {
                    // show C/NO value
                    ImGui::Text("%2d ", sat.cno);
                    
                    // signal strength bar
                    ImGui::SameLine(20);
                    float progress = std::min(sat.cno / 50.0f, 1.0f);
                    ImVec4 progressBarColor;
                    if (sat.cno >= 30) {
                        progressBarColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
                    } else if (sat.cno >= 20) {
                        progressBarColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
                    } else if (sat.cno >= 10) {
                        progressBarColor = ImVec4(1.0f, 0.75f, 0.0f, 1.0f);
                    } else {
                        progressBarColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
                    }
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, progressBarColor);
                    ImGui::ProgressBar(progress, ImVec2(65, 10), "");
                    ImGui::PopStyleColor();
                } else {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "--");
                }
            }
        }
        
        ImGui::EndTable();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopFont();
}

void GPSPage::onSelected() {
    SidePage::onSelected();
    core::gps.startReader();
}

void GPSPage::onUnselected() {
    SidePage::onUnselected();
    core::gps.stopReader();
}
