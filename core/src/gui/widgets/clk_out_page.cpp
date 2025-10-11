#include <gui/widgets/clk_out_page.h>
#include <core.h>
#include <gui/style.h>

#define BUTTON_SPACING  10
#define BUTTON_WIDTH    90
#define BUTTON_HEIGHT   40

ClkOutPage::ClkOutPage() {
    selClk = -1;
    clocks = {
        {"5kHz", 0, 1, 114, 48, 128, 0, 0, 0},
        {"10kHz", 0, 1, 98, 48, 128, 0, 0, 0},
        {"100kHz", 0, 1, 64, 223, 0, 0, 0, 0},
        {"1MHz", 0, 1, 64, 20, 128, 0, 0, 0},
        {"10MHz", 0, 1, 0, 34, 0, 0, 0, 0},
        {"50MHz", 0, 5, 0, 5, 51, 0, 0, 1},
        {"100MHz", 0, 5, 0, 1, 153, 0, 0, 3},
        {"180MHz", 0, 1, 12, 0, 0, 0, 0, 0}
    };
    strengths = {
        {"2 mA", 2},
        {"4 mA", 4},
        {"6 mA", 6},
        {"8 mA", 8}
    };
    strength = 2;
}

ClkOutPage::~ClkOutPage() {

}

const char* ClkOutPage::getLabel() {
	return "CLK OUT";
}

void ClkOutPage::init() {
	loadSettings();
    configClock();
}

void ClkOutPage::deinit() {
    saveSettings();
}

void ClkOutPage::loadSettings() {
    core::configManager.acquire();
	clkOut = core::configManager.conf["clkOut"];
	reg58 = core::configManager.conf["reg58"];
    reg59 = core::configManager.conf["reg59"];
    reg60 = core::configManager.conf["reg60"];
    reg61 = core::configManager.conf["reg61"];
    reg62 = core::configManager.conf["reg62"];
    reg63 = core::configManager.conf["reg63"];
    reg64 = core::configManager.conf["reg64"];
    reg65 = core::configManager.conf["reg65"];
    strength = core::configManager.conf["strength"];
    core::configManager.release();

    int clkCount = clocks.size();
    for (int i = 0; i < clkCount; i ++) {
        Clock clk = clocks[i];
		if (clk.reg58 == reg58 && clk.reg59 == reg59 && clk.reg60 == reg60 && clk.reg61 == reg61
		    && clk.reg62 == reg62 && clk.reg63 == reg63 && clk.reg64 == reg64 && clk.reg65 == reg65) {
		    selClk = i;
		    break;
		}
	}

	if (strength != 2 && strength != 4 && strength !=6 && strength != 8) {
	    strength = 2;
	}
}

void ClkOutPage::saveSettings() {
    core::configManager.acquire();
    core::configManager.conf["clkOut"] = clkOut;
    core::configManager.conf["reg58"] = reg58;
    core::configManager.conf["reg59"] = reg59;
    core::configManager.conf["reg60"] = reg60;
    core::configManager.conf["reg61"] = reg61;
    core::configManager.conf["reg62"] = reg62;
    core::configManager.conf["reg63"] = reg63;
    core::configManager.conf["reg64"] = reg64;
    core::configManager.conf["reg65"] = reg65;
    core::configManager.conf["strength"] = strength;
	core::configManager.release(true);
}

void ClkOutPage::configClock() {
	if (core::si5351.isInitialized()) {
		core::si5351.writeRegister(58, reg58);
		core::si5351.writeRegister(59, reg59);
		core::si5351.writeRegister(60, reg60);
		core::si5351.writeRegister(61, reg61);
		core::si5351.writeRegister(62, reg62);
		core::si5351.writeRegister(63, reg63);
		core::si5351.writeRegister(64, reg64);
		core::si5351.writeRegister(65, reg65);

		unsigned char ctrl = 0x6C;
		ctrl |= ((strength >> 1) - 1);
		ctrl |= clkOut ? 0x00 : 0x80;
		core::si5351.writeRegister(18, ctrl);
		if (clkOut) {
			core::si5351.turnOnClk2();
		} else {
			core::si5351.turnOffClk2();
		}
	}
}

void ClkOutPage::draw() {

    // Output on/off
    if (ImGui::Checkbox("CLK OUT", &clkOut)) {
        configClock();
        saveSettings();
    }

    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, style.FramePadding.y));

    // Drive strength
    ImGui::Spacing();
    ImGui::Text("Drive Strength:");
    for (int i = 0; i < 4; i ++) {
        if (i%2 == 1) {
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + BUTTON_SPACING);
        }
        bool isSelected = (i == (strength >> 1) - 1);
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_TabActive]);
    		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, style.Colors[ImGuiCol_TabHovered]);
    		ImGui::PushStyleColor(ImGuiCol_ButtonActive, style.Colors[ImGuiCol_TabActive]);
        }
        if (ImGui::Button(strengths[i].current, ImVec2(BUTTON_WIDTH, BUTTON_HEIGHT))) {
            if (!isSelected) {
                strength = ((i + 1) << 1);
                configClock();
                saveSettings();
            }
        }
        if (isSelected) {
            ImGui::PopStyleColor(3);
        }
    }

    // Output frequency
    ImGui::Spacing();
    ImGui::Text("Frequency:");
    int clkCount = clocks.size();
    for (int i = 0; i < clkCount; i ++) {
        if (i%2 == 1) {
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + BUTTON_SPACING);
        }
        bool isSelected = (i == selClk);
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_TabActive]);
    		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, style.Colors[ImGuiCol_TabHovered]);
    		ImGui::PushStyleColor(ImGuiCol_ButtonActive, style.Colors[ImGuiCol_TabActive]);
        }
        if (ImGui::Button(clocks[i].frequency, ImVec2(BUTTON_WIDTH, BUTTON_HEIGHT))) {
            if (!isSelected) {
                selClk = i;
                reg58 = clocks[i].reg58;
                reg59 = clocks[i].reg59;
                reg60 = clocks[i].reg60;
                reg61 = clocks[i].reg61;
                reg62 = clocks[i].reg62;
                reg63 = clocks[i].reg63;
                reg64 = clocks[i].reg64;
                reg65 = clocks[i].reg65;
                configClock();
                saveSettings();
            }
        }
        if (isSelected) {
            ImGui::PopStyleColor(3);
        }
    }

    ImGui::PopStyleVar();
}