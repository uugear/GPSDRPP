#pragma once
#include <config.h>
#include <module.h>
#include <module_com.h>
#include "command_args.h"
#include "up_converter.h"
#include "si5351.h"
#include "encoder.h"
#include "gps.h"

namespace core {
	
    struct VUGPSDR_Initializer {
        VUGPSDR_Initializer();
    };

    SDRPP_EXPORT ConfigManager configManager;
    SDRPP_EXPORT ModuleManager moduleManager;
    SDRPP_EXPORT ModuleComManager modComManager;
    SDRPP_EXPORT CommandArgsParser args;
    SDRPP_EXPORT Gps gps;
    SDRPP_EXPORT Si5351 si5351;
    SDRPP_EXPORT UpConverter upConverter;

    void setInputSampleRate(double samplerate);
    
    namespace encoders {
        void on_encoder_a_cw(Encoder *enc);
        void on_encoder_a_ccw(Encoder *enc);
        void on_encoder_a_pressed(Encoder *enc);
        void on_encoder_a_released(Encoder *enc);
        
        void on_encoder_b_cw(Encoder *enc);
        void on_encoder_b_ccw(Encoder *enc);
        void on_encoder_b_pressed(Encoder *enc);
        void on_encoder_b_released(Encoder *enc);
    }
};

int gpsdrpp_main(int argc, char* argv[]);