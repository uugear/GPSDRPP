#include "dev_refresh_worker.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <core.h>
#include <rtl_sdr_source_interface.h>


bool DeviceRefreshWorker::checkI2CDevice(int bus, int address) {
    char filename[20];
    snprintf(filename, sizeof(filename), "/dev/i2c-%d", bus);
    int file = open(filename, O_RDWR);
    if (file < 0) {
        std::cerr << "Failed to open I2C bus " << bus << std::endl;
        return false;
    }
    bool deviceExists = (ioctl(file, I2C_SLAVE, address) >= 0);
    close(file);
    return deviceExists;
}

void DeviceRefreshWorker::delay(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

int DeviceRefreshWorker::executeCommand(const char* command) {
    int result = system(command);
    if (result != 0) {
        std::cerr << "Command failed: " << command << " (exit code: " << result << ")" << std::endl;
    }
    return result;
}

void DeviceRefreshWorker::workerLoop() {
    while (!shouldStop.load()) {
        try {
			if (core::modComManager.interfaceExists("RTL-SDR")) {
				int count;
				core::modComManager.callInterface("RTL-SDR", RTL_SDR_SOURCE_IFACE_CMD_GET_DEVICE_COUNT, NULL, &count);
				if (count == 0) {
					if (checkI2CDevice(2, 0x0b)) {
						std::cout << "VU GPSDR device found on I2C bus 2, make a power cycle now..." << std::endl;
						
						executeCommand("vgp set 4B2 1");
						delay(2000);
						
						executeCommand("vgp set 4B2 0");
						delay(500);
						
                        // Tell GPS to output 24MHz
                        core::configManager.acquire();
                        bool lockToGpsFreq = core::configManager.conf["lockToGpsFreq"];
                        core::configManager.release(true);
                        if (lockToGpsFreq) {
                            core::gps.outputReferenceClock(lockToGpsFreq);
                        }
                        delay(500);
						
						// Initialize Si5351
						try {
							core::si5351.initialize();
						} catch (const std::exception& e) {
							std::cerr << "Si5351 initialize failed: " << e.what() << std::endl;
						}
						
						std::cout << "Power cycle completed." << std::endl;
						
						delay(500);
						
						// Refresh device list
						core::modComManager.callInterface("RTL-SDR", RTL_SDR_SOURCE_IFACE_CMD_REFRESH, NULL, NULL);
					}
				}
			}
        } catch (const std::exception& e) {
            std::cerr << "Exception in DeviceRefreshWorker thread: " << e.what() << std::endl;
        }
		delay(2000);
    }
    
    std::cout << "DeviceRefreshWorker thread stopped." << std::endl;
}

void DeviceRefreshWorker::start() {
    if (workerThread.joinable()) {
        std::cerr << "DeviceRefreshWorker thread is already running!" << std::endl;
        return;
    }
    
    shouldStop.store(false);
    workerThread = std::thread(&DeviceRefreshWorker::workerLoop, this);
    std::cout << "DeviceRefreshWorker thread started." << std::endl;
}

void DeviceRefreshWorker::stop() {
    shouldStop.store(true);
    
    if (workerThread.joinable()) {
        workerThread.join();
        std::cout << "DeviceRefreshWorker thread joined." << std::endl;
    }
}

bool DeviceRefreshWorker::isRunning() const {
    return workerThread.joinable() && !shouldStop.load();
}

DeviceRefreshWorker::~DeviceRefreshWorker() {
    stop();
}