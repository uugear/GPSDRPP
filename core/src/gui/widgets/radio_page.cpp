#include <gui/widgets/radio_page.h>
#include <rtl_sdr_source_interface.h>
#include <radio_interface.h>
#include <core.h>
#include <gui/gui.h>


RadioPage::RadioPage() {
    
}

RadioPage::~RadioPage() {

}

const char* RadioPage::getLabel() {
	return "Radio";
}

void RadioPage::init() {
    core::configManager.acquire();
    hfLNA = core::configManager.conf["hfLNA"];
    core::configManager.release(true);
    controlLNA();
}

void RadioPage::deinit() {

}

void RadioPage::draw() {
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
	
	// Controls from radio
	std::string vfoName = gui::waterfall.selectedVFO;
	if (core::modComManager.interfaceExists(vfoName)) {
		int mode;
        core::modComManager.callInterface(vfoName, RADIO_IFACE_CMD_GET_MODE, NULL, &mode);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, ImGui::GetStyle().ItemSpacing.y));
		ImGui::Columns(2, "RadioModeColumns##_SIDEBAR", false);
		if (ImGui::RadioButton("WFM##_SIDEBAR", mode == 1) && mode != 1) {
			mode = 1;
			core::modComManager.callInterface(vfoName, RADIO_IFACE_CMD_SET_MODE, &mode, NULL);
        }
		if (ImGui::RadioButton("AM##_SIDEBAR", mode == 2) && mode != 2) {
			mode = 2;
			core::modComManager.callInterface(vfoName, RADIO_IFACE_CMD_SET_MODE, &mode, NULL);
        }
		if (ImGui::RadioButton("LSB##_SIDEBAR", mode == 6) && mode != 6) {
			mode = 6;
			core::modComManager.callInterface(vfoName, RADIO_IFACE_CMD_SET_MODE, &mode, NULL);
        }
		if (ImGui::RadioButton("CW##_SIDEBAR", mode == 5) && mode != 5) {
			mode = 5;
			core::modComManager.callInterface(vfoName, RADIO_IFACE_CMD_SET_MODE, &mode, NULL);
        }
		ImGui::NextColumn();
		if (ImGui::RadioButton("NFM##_SIDEBAR", mode == 0) && mode != 0) {
			mode = 0;
			core::modComManager.callInterface(vfoName, RADIO_IFACE_CMD_SET_MODE, &mode, NULL);
        }
		if (ImGui::RadioButton("DSB##_SIDEBAR", mode == 3) && mode != 3) {
			mode = 3;
			core::modComManager.callInterface(vfoName, RADIO_IFACE_CMD_SET_MODE, &mode, NULL);
        }
		if (ImGui::RadioButton("USB##_SIDEBAR", mode == 4) && mode != 4) {
			mode = 4;
			core::modComManager.callInterface(vfoName, RADIO_IFACE_CMD_SET_MODE, &mode, NULL);
        }
		if (ImGui::RadioButton("RAW##_SIDEBAR", mode == 7) && mode != 7) {
			mode = 7;
			core::modComManager.callInterface(vfoName, RADIO_IFACE_CMD_SET_MODE, &mode, NULL);
        }
        ImGui::Columns(1);
		ImGui::PopStyleVar();
	}
	
	if (core::upConverter.isEnabled()) {
    	if (ImGui::Checkbox("LNA for HF", &hfLNA)) {
    	    controlLNA();
        }
    }
}


void RadioPage::controlLNA() {
    if (hfLNA) {
        system("vgp set 4B4 0");
       	system("vgp set 4D6 1");
    } else {
    	system("vgp set 4B4 1");
       	system("vgp set 4D6 0");
    }
}