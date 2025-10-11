#include "encoder.h"
#include <cstring>
#include <cmath>
#include <iostream>
#include <chrono>
#include <sys/time.h>
#include <core.h>
#include <radio_interface.h>
#include <signal_path/signal_path.h>
#include <gui/gui.h>
#include <gui/widgets/waterfall_view.h>


const int demodulation_modes[] = {1, 2, 6, 5, 0, 3, 4, 7};
const int demodulation_modes_count = sizeof(demodulation_modes) / sizeof(demodulation_modes[0]);


// Constructor
Encoder::Encoder(const char* chipA, int lineA, const char* chipB, int lineB, const char* chipC, int lineC)
    : chipA(chipA), lineA(lineA), chipB(chipB), lineB(lineB), chipC(chipC), lineC(lineC), va(-1), vb(-1),
	running(false), cwCallback(nullptr), ccwCallback(nullptr), pressedCallback(nullptr), releasedCallback(nullptr),
	direction(0), speed(1), changingDirection(0), speedingUp(0), lastDownUpEvent(0) {}

// Destructor
Encoder::~Encoder() {
    stop();
}

// Get speed
int Encoder::getSpeed() {
	return speed;
}

// Start monitoring GPIO in a new thread
void Encoder::start() {
    running = true;
    rotationMonitorThread = std::thread(&Encoder::rotationMonitor, this);
	clickingMonitorThread = std::thread(&Encoder::clickingMonitor, this);
	tsStart = getTimestamp();
}

// Stop monitoring
void Encoder::stop() {
    if (running) {
        running = false;
        if (rotationMonitorThread.joinable()) {
            rotationMonitorThread.join();
        }
		if (clickingMonitorThread.joinable()) {
            clickingMonitorThread.join();
        }
    }
}

// Set the CW callback
void Encoder::setCWCallback(std::function<void(Encoder*)> callback) {
    cwCallback = callback;
}

// Set the CCW callback
void Encoder::setCCWCallback(std::function<void(Encoder*)> callback) {
    ccwCallback = callback;
}

// Set the Pressed callback
void Encoder::setPressedCallback(std::function<void(Encoder*)> callback) {
    pressedCallback = callback;
}

// Set the Released callback
void Encoder::setReleasedCallback(std::function<void(Encoder*)> callback) {
    releasedCallback = callback;
}

// Get current timestamp in milliseconds
long long Encoder::getTimestamp() {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
        return -1;
    }
    long long milliseconds = (long long)(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
    return milliseconds;
}

// Get the change of speed
int Encoder::changeSpeed(long long dt) {
	if (dt < 10) {
		speedingUp ++;
		if (speedingUp > ENCODER_MAX_SPEED_UP_STEPS) {
			speedingUp = 0;
			return 1;
		}
		return 0;
	}
	speedingUp = 0;
	if (dt > 100) return -1;
	if (dt > 200) return -2;
	if (dt > 500) return -3;
	if (dt > 1000) return -4;
	return 0;
}

// Set the direction
void Encoder::setDirection(int dir) {
    if (direction != dir) {
        sameDirectionCount = 0;
    } else {
        sameDirectionCount ++;
    }
	tsCur = getTimestamp();
	bool valid = true;
	if (direction == dir) {
		speed += changeSpeed(tsCur - tsPrev);
		if (speed > 5) {
			speed = 5;
		}
		if (speed < 1) {
			speed = 1;
		}
	} else {
		changingDirection ++;
		if (changingDirection > ENCODER_MAX_CHANGING_DIRECTION_STEPS) {
			changingDirection = 0;
			direction = dir;
			speed = 1;
		} else {
			valid = false;
		}
	}
	tsPrev = tsCur;
	if (valid) {
		if (dir == -1) {
			ccwCallback(this);
		} else if (dir == 1) {
			cwCallback(this);
		}
	}
}

// Monitor encoder rotation
void Encoder::rotationMonitor() {
    struct gpiod_chip *chip_a, *chip_b;
    struct gpiod_line *line_a;
    struct gpiod_line *line_b;
    struct gpiod_line_bulk line_bulk, event_bulk;
    struct gpiod_line_event event;
    int ret;

    // Open GPIO chip
    chip_a = gpiod_chip_open(chipA);
    if (!chip_a) {
        perror("gpiod_chip_open A");
        return;
    }
    if (strcmp(chipA, chipB) == 0) {
        chip_b = chip_a;
    } else {
        chip_b = gpiod_chip_open(chipB);
    }
    if (!chip_b) {
        perror("gpiod_chip_open B");
        return;
    }

    // Get lines for A and B
    line_a = gpiod_chip_get_line(chip_a, lineA);
    line_b = gpiod_chip_get_line(chip_b, lineB);
    if (!line_a || !line_b) {
        perror("gpiod_chip_get_line");
        gpiod_chip_close(chip_a);
        if (chip_b != chip_a) {
            gpiod_chip_close(chip_b);
        }
        return;
    }

    // Request both lines to listen for rising and falling edges
    if (gpiod_line_request_both_edges_events(line_a, "encoder-listener") < 0 ||
        gpiod_line_request_both_edges_events(line_b, "encoder-listener") < 0) {
        perror("gpiod_line_request_both_edges_events");
        gpiod_line_release(line_a);
        gpiod_line_release(line_b);
        gpiod_chip_close(chip_a);
        if (chip_b != chip_a) {
            gpiod_chip_close(chip_b);
        }
        return;
    }

    line_bulk.num_lines = 0;
    gpiod_line_bulk_add(&line_bulk, line_a);
    gpiod_line_bulk_add(&line_bulk, line_b);

    while (running) {
        timespec timeout;
        timeout.tv_sec = 0;
        timeout.tv_nsec = 100000000L;
        ret = gpiod_line_event_wait_bulk(&line_bulk, &timeout, &event_bulk);
        if (ret > 0) {
            for (int i = 0; i < event_bulk.num_lines; i ++) {
                if (gpiod_line_event_read(event_bulk.lines[i], &event) == 0) {
                    int _va = va;
                    int _vb = vb;
                    va = gpiod_line_get_value(line_a);
                    vb = gpiod_line_get_value(line_b);

                    if (event_bulk.lines[i] == line_a && va != _va) {
                        if (event.event_type == GPIOD_LINE_EVENT_RISING_EDGE && va > _va) {
                            if (vb == 0 && cwCallback) {
								setDirection(-1);
                            } else if (vb != 0 && ccwCallback) {
								setDirection(1);
                            }
                        } else if (va < _va) {
                            if (vb == 1 && cwCallback) {
								setDirection(-1);
                            } else if (vb != 1 && ccwCallback) {
								setDirection(1);
                            }
                        }
                    } else if (event_bulk.lines[i] == line_b && _vb != vb) {
                        if (event.event_type == GPIOD_LINE_EVENT_RISING_EDGE && vb > _vb) {
                            if (va == 1 && cwCallback) {
								setDirection(-1);
                            } else if (va != 1 && ccwCallback) {
								setDirection(1);
                            }
                        } else if (vb < _va) {
                            if (va == 0 && cwCallback) {
								setDirection(-1);
                            } else if (va != 0 && ccwCallback) {
								setDirection(1);
                            }
                        }
                    }
                }
            }
        } else if (ret < 0) {
            perror("gpiod_line_event_wait_bulk");
        }
    }

    // Release resources
    gpiod_line_release(line_a);
    gpiod_line_release(line_b);
    gpiod_chip_close(chip_a);
    if (chip_b != chip_a) {
        gpiod_chip_close(chip_b);
    }
}

void Encoder::clickingMonitor() {

	struct gpiod_chip *chip_c = gpiod_chip_open(chipC);
    if (!chip_c) {
        perror("gpiod_chip_open C");
        return;
    }

    // Get the GPIO line
    struct gpiod_line *line_c = gpiod_chip_get_line(chip_c, lineC);
    if (!line_c) {
        perror("gpiod_chip_get_line C");
        gpiod_chip_close(chip_c);
        return;
    }

    // Request the line as input with event monitoring
    if (gpiod_line_request_both_edges_events(line_c, "encoder-listener") < 0) {
        perror("gpiod_line_request_both_edges_events C");
        gpiod_chip_close(chip_c);
        return;
    }

    while (running) {
        struct gpiod_line_event event;
        timespec timeout;
        timeout.tv_sec = 0;
        timeout.tv_nsec = 100000000L;

        // Wait for an event
        int ret = gpiod_line_event_wait(line_c, &timeout);
        if (ret < 0) {
            perror("gpiod_line_event_wait C");
            break;
        }
        if (ret == 0) {
            continue;   // Timeout occurred
        }

        // Read the event
        if (gpiod_line_event_read(line_c, &event) < 0) {
            perror("gpiod_line_event_read C");
            break;
        }

        // Process the event
        if (event.event_type != lastDownUpEvent) {
			if (getTimestamp() - tsStart > 500) {
				if (event.event_type == GPIOD_LINE_EVENT_RISING_EDGE) {
					if (releasedCallback) {
						releasedCallback(this);
					}
				} else {
					if (pressedCallback) {
						pressedCallback(this);
					}
				}
			}
        }
        lastDownUpEvent = event.event_type;
    }

    // Release resources
    gpiod_line_release(line_c);
    gpiod_chip_close(chip_c);
}


void Encoder::tuningFrequency(bool cw) {
    double snapInterval = sigpath::vfoManager.getSnapInterval(gui::waterfall.selectedVFO);
    double deltaFreq = snapInterval * this->getSpeed() / 10;
    double centerFreq = gui::waterfall.getCenterFrequency();
    if (cw) {
        double upperOffset = sigpath::vfoManager.getUpperOffset(gui::waterfall.selectedVFO);
    	double upperFreq = gui::waterfall.getUpperFrequency();
    	if (core::configManager.conf["centerTuning"] || centerFreq + upperOffset + deltaFreq >= upperFreq) {
    	    centerFreq += deltaFreq;
    	    gui::waterfall.setCenterFrequency(centerFreq);
    	    gui::waterfall.centerFreqMoved = true;
    	} else {
    	    double offset = sigpath::vfoManager.getOffset(gui::waterfall.selectedVFO);
        	offset += deltaFreq;
        	sigpath::vfoManager.setOffset(gui::waterfall.selectedVFO, offset);
    	}
    } else {
    	double lowerOffset = sigpath::vfoManager.getLowerOffset(gui::waterfall.selectedVFO);
    	double lowerFreq = gui::waterfall.getLowerFrequency();;
    	if (core::configManager.conf["centerTuning"] || centerFreq + lowerOffset - deltaFreq <= lowerFreq) {
    	    centerFreq -= deltaFreq;
    	    gui::waterfall.setCenterFrequency(centerFreq);
    	    gui::waterfall.centerFreqMoved = true;
    	} else {
    	    double offset = sigpath::vfoManager.getOffset(gui::waterfall.selectedVFO);
        	offset -= deltaFreq;
        	sigpath::vfoManager.setOffset(gui::waterfall.selectedVFO, offset);
    	}
    }
}


void Encoder::frequencyRuler(bool cw) {
    double snapInterval = sigpath::vfoManager.getSnapInterval(gui::waterfall.selectedVFO);
    double frequency = core::configManager.conf["frequency"];
    double offset = snapInterval * this->getSpeed() / 2.0;
    if (cw) {
        frequency -= offset;
    } else {
        frequency += offset;
    }
    core::configManager.conf["frequency"] = frequency;
    tuner::normalTuning("", frequency);
	if (core::configManager.conf["centerTuning"]) {
		gui::waterfall.centerFreqMoved = true;
	} else {
		double offset = sigpath::vfoManager.getOffset(gui::waterfall.selectedVFO);
		sigpath::vfoManager.setOffset(gui::waterfall.selectedVFO, offset);
	}
}


void Encoder::zooming(bool cw) {
    double curBw = WaterfallView::getInstance().getBandwidth();
    curBw += cw ? 0.05 : -0.05;
    curBw = std::max<double>(0.0, std::min<double>(curBw, 1.0));
    WaterfallView::getInstance().setBandwidth(curBw);
    WaterfallView::getInstance().onZoom();
}


void Encoder::demodulation(bool cw) {
    if (sameDirectionCount < 20) {
        return;
    }
    sameDirectionCount = 0;
    
    std::string vfoName = gui::waterfall.selectedVFO;
	if (core::modComManager.interfaceExists(vfoName)) {
        int mode;
        core::modComManager.callInterface(vfoName, RADIO_IFACE_CMD_GET_MODE, NULL, &mode);
        int index = -1;
        for (int i = 0; i < demodulation_modes_count; i ++) {
            if (demodulation_modes[i] == mode) {
                index = i;
                break;
            }
        }
        if (index == -1) {
            index = 0;
        } else {
            index += cw ? 1 : -1;
            index = std::max<int>(0, std::min<int>(index, demodulation_modes_count - 1));
        }
        mode = demodulation_modes[index];
		core::modComManager.callInterface(vfoName, RADIO_IFACE_CMD_SET_MODE, &mode, NULL);
    }
}


void Encoder::volume(bool cw) {
    float vol = sigpath::sinkManager.getStreamVolume(gui::waterfall.selectedVFO);
    if (vol >= 0.0) {
        vol += cw ? 0.05 : -0.05;
        vol = std::max<double>(0.0, std::min<double>(vol, 1.0));
        sigpath::sinkManager.setStreamVolume(gui::waterfall.selectedVFO, vol);
    }
}


void Encoder::onRotate(int func, bool cw) {
    switch (func) {
        case ENCODER_FUNC_TUNING_FREQUENCY:
            tuningFrequency(cw);
        	break;
        case ENCODER_FUNC_FREQUENCY_RULER:
            frequencyRuler(cw);
            break;
        case ENCODER_FUNC_ZOOMING:
            zooming(cw);
            break;
        case ENCODER_FUNC_DEMODULATION:
            demodulation(cw);
            break;
        case ENCODER_FUNC_VOLUME:
            volume(cw);
            break;
        default:
            printf("Unknown encoder function: %d\n", func);
            break;
    }
}
