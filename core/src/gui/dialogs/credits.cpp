#include <gui/dialogs/credits.h>
#include <imgui.h>
#include <gui/icons.h>
#include <gui/style.h>
#include <config.h>
#include <version.h>
#include <backend.h>
#include <imgui_internal.h>

namespace credits {
	
    ImVec2 imageSize(128.0f, 128.0f);
	
    void init() {
		
    }

    void show(bool * showCredits) {
		ImGui::Begin("Main", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground);
		
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 20.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
        ImVec2 dispSize = ImGui::GetIO().DisplaySize;
        ImVec2 center = ImVec2(dispSize.x / 2.0f, dispSize.y / 2.0f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::OpenPopup("Credits");
        
		if (ImGui::BeginPopupModal("Credits", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove)) {
			ImGui::PushFont(style::hugeFont);
			ImGui::TextUnformatted("GP");
			ImGui::SameLine(0, 0);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.176f, 0.733f, 0.596f, 1.0f));	//#2dbb98
			ImGui::TextUnformatted("S");
			ImGui::PopStyleColor();
			ImGui::SameLine(0, 0);
			ImGui::TextUnformatted("DR++ ");
			ImGui::PopFont();
			ImGui::SameLine();
			ImGui::Image(icons::LOGO, imageSize);
			ImGui::Spacing();
			ImGui::Spacing();
			ImGui::Spacing();
			
			ImGui::TextUnformatted(
			"GPSDR++ is based on the open-source SDR++ project, originally created by Alexandre Rouma (ON5RYZ) and enriched by many talented contributors.\n"
			"We deeply appreciate their dedication and hard work, which form the foundation of this software.\n"
			"For more information about SDR++, visit https://github.com/AlexandreRouma/SDRPlusPlus\n\n"
			"GPSDR++ is based on SDR++ v1.2.0 and has been modified by Dun Cat B.V.(UUGear) to support VU GPSDR hardware.\n"
			"The source code of GPSDR++ can be found at https://github.com/uugear/GPSDRPP"
			);

			ImGui::Spacing();
			ImGui::Spacing();
			ImGui::Spacing();
			ImGui::TextUnformatted("GPSDR++ v" VERSION_STR " (Built at " __TIME__ ", " __DATE__ ")");

			ImGui::Spacing();
			ImGui::Spacing();
			ImGui::Spacing();
			ImGui::PushFont(style::sidebarFont);
			if (ImGui::Button("Return to the App", ImVec2(200, 50))) {
				*showCredits = false;
				ImGui::CloseCurrentPopup();
			}
			
			ImGui::SameLine(0, 90);
			
			if (ImGui::Button("Minimize the App", ImVec2(200, 50))) {
				*showCredits = false;
				backend::minimizeWindow();
				ImGui::CloseCurrentPopup();
			}
			
			ImGui::SameLine(0, 90);
			
			if (ImGui::Button("Quit the App", ImVec2(150, 50))) {
				*showCredits = false;
				backend::quitApplication();
				ImGui::CloseCurrentPopup();
			}
			ImGui::PopFont();
			ImGui::EndPopup();
		}
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
		
		ImGui::End();
    }
}