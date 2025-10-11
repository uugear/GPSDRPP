#ifndef ENCODER_H
#define ENCODER_H

#include <gpiod.h>
#include <thread>
#include <atomic>
#include <functional>

#define ENCODER_MAX_CHANGING_DIRECTION_STEPS	2
#define ENCODER_MAX_SPEED_UP_STEPS 				3

#define ENCODER_FUNC_TUNING_FREQUENCY           0
#define ENCODER_FUNC_FREQUENCY_RULER            1
#define ENCODER_FUNC_ZOOMING                    2
#define ENCODER_FUNC_DEMODULATION               3
#define ENCODER_FUNC_VOLUME                     4


class Encoder {
public:
    Encoder(const char* chipA, int lineA, const char* chipB, int lineB, const char* chipC, int lineC);
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
	long long getTimestamp();
	int changeSpeed(long long dt);
	void setDirection(int dir);
    void rotationMonitor();
	void clickingMonitor();
	
	void tuningFrequency(bool cw);
    void frequencyRuler(bool cw);
    void zooming(bool cw);
    void demodulation(bool cw);
    void volume(bool cw);
    
    std::function<void(Encoder*)> cwCallback;
    std::function<void(Encoder*)> ccwCallback;
	std::function<void(Encoder*)> pressedCallback;
	std::function<void(Encoder*)> releasedCallback;

	long long tsPrev, tsCur;
    std::thread rotationMonitorThread;
	std::thread clickingMonitorThread;
    std::atomic<bool> running;
    int va, vb;
    const char *chipA, *chipB, *chipC;
    int lineA, lineB, lineC;
	int direction;	// ccw=-1, none=0, cw=1
	int speed;		// max=5, min=1
	int changingDirection;
	int speedingUp;
	int lastDownUpEvent;
	int sameDirectionCount = 0;
	long long tsStart;
};

#endif // ENCODER_H
