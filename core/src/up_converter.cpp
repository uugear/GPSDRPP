#include "up_converter.h"
#include <iostream>
#include <string>
#include <sstream>
#include <utils/flog.h>
#include <core.h>
#include <signal_path/signal_path.h>


// Constructor
UpConverter::UpConverter() {
    
    flog::info("Initialize Up-Converter...");
    
    enabled = false;
    
    core::si5351.turnOffClk1();
    
    system(UP_CONVERTER_CMD_ALT_0);
    system(UP_CONVERTER_CMD_MODE_OUT);
    system(UP_CONVERTER_CMD_SET_HIGH);
}

// Destructor
UpConverter::~UpConverter() {

}

void UpConverter::updateState(double lowerFreq) {
    bool oldState = enabled;
    enabled = (lowerFreq <= UP_CONVERTER_WORKING_FREQ_UP_TO);
    if (enabled != oldState) {
        flog::info("Up-Converter is turned {0}.", enabled ? "ON" : "OFF");
        if (enabled) {
            core::si5351.turnOnClk1();
            system(UP_CONVERTER_CMD_SET_LOW);
			sigpath::sourceManager.setTuningOffset(UP_CONVERTER_TUNING_OFFSET);
        } else {
            core::si5351.turnOffClk1();
            system(UP_CONVERTER_CMD_SET_HIGH);
			sigpath::sourceManager.setTuningOffset(0);
        }
    }
}

bool UpConverter::isEnabled() {
    return enabled;
}