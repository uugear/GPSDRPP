#pragma once

#include <thread>
#include <chrono>
#include <atomic>

class DeviceRefreshWorker {
private:
    std::thread workerThread;
    std::atomic<bool> shouldStop{false};
    
    // Check if device exists on I2C bus at specified address
    bool checkI2CDevice(int bus, int address);
    
    // Safe delay function
    void delay(int milliseconds);
    
    // Safe wrapper for executing system commands
    int executeCommand(const char* command);
    
    // Main worker loop
    void workerLoop();
    
public:
    // Start background thread
    void start();
    
    // Stop background thread
    void stop();
    
    // Check if thread is running
    bool isRunning() const;
    
    // Destructor ensures thread is properly stopped
    ~DeviceRefreshWorker();
};