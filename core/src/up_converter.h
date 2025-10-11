#ifndef UP_CONVERTER_H
#define UP_CONVERTER_H

/*
 * This up-converter will up-shift frequencies by 108 MHz, and it is used when tuning below 30 MHz.
 * When being enabled, it will
 *  - turn on CLK1 in Si5351 to output 108 MHz local oscillator frequency.
 *  - pull down GPIO4_B1 to control RF switches, which will put itself into the signal path.
 *  - call sigpath::sourceManager.setTuningOffset() to set the tunning offset.
 */

#define UP_CONVERTER_CMD_ALT_0      "vgp alt 4B1 0"
#define UP_CONVERTER_CMD_MODE_OUT   "vgp mode 4B1 OUT"
#define UP_CONVERTER_CMD_SET_HIGH   "vgp set 4B1 1"
#define UP_CONVERTER_CMD_SET_LOW    "vgp set 4B1 0"

#define UP_CONVERTER_WORKING_FREQ_UP_TO	30000000.0
#define UP_CONVERTER_TUNING_OFFSET		108000000

class UpConverter {
public:
    UpConverter();
    ~UpConverter();
    
    void updateState(double lowerFreq);
    bool isEnabled();
	
private:
    bool enabled;

};

#endif // UP_CONVERTER_H
