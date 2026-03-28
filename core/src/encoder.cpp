#include "encoder.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/time.h>

#include <core.h>
#include <gui/gui.h>
#include <gui/widgets/waterfall_view.h>
#include <radio_interface.h>
#include <signal_path/signal_path.h>

const int demodulation_modes[] = {1, 2, 6, 5, 0, 3, 4, 7};
const int demodulation_modes_count = sizeof(demodulation_modes) / sizeof(demodulation_modes[0]);

std::mutex Encoder::registryMutex;
Encoder* Encoder::pinRegistry[41] = {nullptr};

Encoder::Encoder(int pinA, int pinB, int pinC)
    : pinA(pinA),
      pinB(pinB),
      pinC(pinC),
      direction(0),
      speed(1),
      changingDirection(0),
      lastDownUpEvent(0) {}

Encoder::~Encoder() {
    stop();
}

int Encoder::getSpeed() {
    return speed;
}

void Encoder::start() {
    if (running.exchange(true)) {
        return;
    }

    registerPins(this);

    va = readPinValue(pinA);
    vb = readPinValue(pinB);
    tsStart = getTimestamp();

    create_monitor_thread(pinA, 0, GPIO_BOTH_EDGES, &Encoder::handleMonitorEvent);
    create_monitor_thread(pinB, 0, GPIO_BOTH_EDGES, &Encoder::handleMonitorEvent);
    create_monitor_thread(pinC, 0, GPIO_BOTH_EDGES, &Encoder::handleMonitorEvent);
}

void Encoder::stop() {
    if (!running.exchange(false)) {
        return;
    }

    destroy_monitor_thread(pinA);
    destroy_monitor_thread(pinB);
    destroy_monitor_thread(pinC);
    unregisterPins(this);
}

void Encoder::setCWCallback(std::function<void(Encoder*)> callback) {
    cwCallback = callback;
}

void Encoder::setCCWCallback(std::function<void(Encoder*)> callback) {
    ccwCallback = callback;
}

void Encoder::setPressedCallback(std::function<void(Encoder*)> callback) {
    pressedCallback = callback;
}

void Encoder::setReleasedCallback(std::function<void(Encoder*)> callback) {
    releasedCallback = callback;
}

long long Encoder::getTimestamp() {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
        return -1;
    }
    return (long long)(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

int Encoder::changeSpeed(long long dt) {
    if (dt <= 0) {
        return 1;
    }

    speedSamples[speedSampleIndex] = dt;
    speedSampleIndex = (speedSampleIndex + 1) % ENCODER_SPEED_SAMPLE_COUNT;
    if (speedSampleCount < ENCODER_SPEED_SAMPLE_COUNT) {
        speedSampleCount++;
    }

    long long total = 0;
    for (int i = 0; i < speedSampleCount; i++) {
        total += speedSamples[i];
    }
    long long avgDt = total / speedSampleCount;

    if (avgDt < 20) return 8;
    if (avgDt < 40) return 4;
    if (avgDt < 80) return 2;
    return 1;
}

void Encoder::setDirection(int dir) {
    if (direction != dir) {
        sameDirectionCount = 0;
    } else {
        sameDirectionCount++;
    }

    tsCur = getTimestamp();
    bool valid = true;
    if (direction == dir) {
        speed = changeSpeed(tsCur - tsPrev);
    } else {
        changingDirection++;
        if (changingDirection > ENCODER_MAX_CHANGING_DIRECTION_STEPS) {
            changingDirection = 0;
            direction = dir;
            speed = 1;
            speedSampleIndex = 0;
            speedSampleCount = 0;
            std::fill(std::begin(speedSamples), std::end(speedSamples), 0);
        } else {
            valid = false;
        }
    }
    tsPrev = tsCur;

    if (valid) {
        if (dir == -1) {
            if (ccwCallback) ccwCallback(this);
        } else if (dir == 1) {
            if (cwCallback) cwCallback(this);
        }
    }
}

void Encoder::handleMonitorEvent(void *ctx) {
    auto *mt = static_cast<MonitorThread*>(ctx);
    if (!mt) {
        return;
    }

    Encoder* enc = lookupByPin(mt->pin);
    if (!enc || !enc->running.load()) {
        return;
    }

    enc->processMonitorEvent(mt->pin, mt->latest_event);
}

Encoder* Encoder::lookupByPin(int pin) {
    if (pin <= 0 || pin >= 41) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(registryMutex);
    return pinRegistry[pin];
}

void Encoder::registerPins(Encoder* enc) {
    std::lock_guard<std::mutex> lock(registryMutex);
    pinRegistry[enc->pinA] = enc;
    pinRegistry[enc->pinB] = enc;
    pinRegistry[enc->pinC] = enc;
}

void Encoder::unregisterPins(Encoder* enc) {
    std::lock_guard<std::mutex> lock(registryMutex);
    if (pinRegistry[enc->pinA] == enc) pinRegistry[enc->pinA] = nullptr;
    if (pinRegistry[enc->pinB] == enc) pinRegistry[enc->pinB] = nullptr;
    if (pinRegistry[enc->pinC] == enc) pinRegistry[enc->pinC] = nullptr;
}

int Encoder::readPinValue(int pin) const {
    if (pin <= 0 || pin > 40 || is_power_pin(pin) || NAMES[pin] == nullptr) {
        return -1;
    }
    int ch = get_chip_number((char *)NAMES[pin]);
    int ln = get_line_number((char *)NAMES[pin]);
    if (ch < 0 || ln < 0) {
        return -1;
    }
    return get(ch, ln);
}

void Encoder::processMonitorEvent(int pin, int edge) {
    if (pin == pinC) {
        processClickEvent(edge);
    } else if (pin == pinA || pin == pinB) {
        processRotationEvent(pin, edge);
    }
}

void Encoder::processRotationEvent(int pin, int edge) {
    int _va = va;
    int _vb = vb;
    va = readPinValue(pinA);
    vb = readPinValue(pinB);
    if (va < 0 || vb < 0) {
        return;
    }

    if (pin == pinA && va != _va) {
        if (edge == GPIO_RISING_EDGE && va > _va) {
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
    } else if (pin == pinB && _vb != vb) {
        if (edge == GPIO_RISING_EDGE && vb > _vb) {
            if (va == 1 && cwCallback) {
                setDirection(-1);
            } else if (va != 1 && ccwCallback) {
                setDirection(1);
            }
        } else if (vb < _vb) {
            if (va == 0 && cwCallback) {
                setDirection(-1);
            } else if (va != 0 && ccwCallback) {
                setDirection(1);
            }
        }
    }
}

void Encoder::processClickEvent(int edge) {
    if (edge == lastDownUpEvent) {
        return;
    }

    if (getTimestamp() - tsStart > 500) {
        if (edge == GPIO_RISING_EDGE) {
            if (releasedCallback) {
                releasedCallback(this);
            }
        } else if (edge == GPIO_FALLING_EDGE) {
            if (pressedCallback) {
                pressedCallback(this);
            }
        }
    }
    lastDownUpEvent = edge;
}

void Encoder::tuningFrequency(bool cw) {
    double snapInterval = sigpath::vfoManager.getSnapInterval(gui::waterfall.selectedVFO);
    double baseStep;
    if (snapInterval >= 50000.0) {
        baseStep = snapInterval / 10.0;
    } else if (snapInterval >= 5000.0) {
        baseStep = snapInterval / 5.0;
    } else {
        baseStep = snapInterval / 2.0;
    }
    double deltaFreq = baseStep * this->getSpeed();
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
        double lowerFreq = gui::waterfall.getLowerFrequency();
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
    double frequency = core::configManager.conf["frequency"];
    double visibleRange = gui::waterfall.getViewBandwidth();
    double baseStep = visibleRange / 40.0;
    double offset = baseStep * this->getSpeed();
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
        double offset2 = sigpath::vfoManager.getOffset(gui::waterfall.selectedVFO);
        sigpath::vfoManager.setOffset(gui::waterfall.selectedVFO, offset2);
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
        for (int i = 0; i < demodulation_modes_count; i++) {
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
        printf("Unknown encoder function: %d", func);
        break;
    }
}
