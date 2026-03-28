#ifndef ENCODER_H
#define ENCODER_H

#include <thread>
#include <atomic>
#include <functional>
#include <mutex>
#include "vgplib.h"

#define ENCODER_MAX_CHANGING_DIRECTION_STEPS    2
#define ENCODER_SPEED_SAMPLE_COUNT              4

#define ENCODER_FUNC_TUNING_FREQUENCY           0
#define ENCODER_FUNC_FREQUENCY_RULER            1
#define ENCODER_FUNC_ZOOMING                    2
#define ENCODER_FUNC_DEMODULATION               3
#define ENCODER_FUNC_VOLUME                     4

class Encoder {
public:
    Encoder(int pinA, int pinB, int pinC);
    ~Encoder();

    int getSpeed();

    void start();
    void stop();

    void setCWCallback(std::function<void(Encoder*)> callback);
    void setCCWCallback(std::function<void(Encoder*)> callback);
    void setPressedCallback(std::function<void(Encoder*)> callback);
    void setReleasedCallback(std::function<void(Encoder*)> callback);

    void onRotate(int func, bool cw);

private:
    static void handleMonitorEvent(void *ctx);
    static Encoder* lookupByPin(int pin);
    static void registerPins(Encoder* enc);
    static void unregisterPins(Encoder* enc);

    void processMonitorEvent(int pin, int edge);
    void processRotationEvent(int pin, int edge);
    void processClickEvent(int edge);
    int readPinValue(int pin) const;

    long long getTimestamp();
    int changeSpeed(long long dt);
    void setDirection(int dir);

    void tuningFrequency(bool cw);
    void frequencyRuler(bool cw);
    void zooming(bool cw);
    void demodulation(bool cw);
    void volume(bool cw);

    std::function<void(Encoder*)> cwCallback;
    std::function<void(Encoder*)> ccwCallback;
    std::function<void(Encoder*)> pressedCallback;
    std::function<void(Encoder*)> releasedCallback;

    long long tsPrev{}, tsCur{};
    std::atomic<bool> running{false};
    int va{-1}, vb{-1};
    int pinA, pinB, pinC;
    int direction;   // ccw=-1, none=0, cw=1
    int speed;       // dynamic multiplier: 1, 2, 4, 8
    int changingDirection;
    long long speedSamples[ENCODER_SPEED_SAMPLE_COUNT] {};
    int speedSampleIndex{0};
    int speedSampleCount{0};
    int lastDownUpEvent;
    int sameDirectionCount = 0;
    long long tsStart{};

    static std::mutex registryMutex;
    static Encoder* pinRegistry[41];
};

#endif // ENCODER_H
