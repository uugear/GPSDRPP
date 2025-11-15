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
#include <utils/flog.h>


bool DeviceRefreshWorker::checkI2CDevice(int bus, int address) {
    char filename[20];
    snprintf(filename, sizeof(filename), "/dev/i2c-%d", bus);
    int file = open(filename, O_RDWR);
    if (file < 0) {
        flog::error("Failed to open I2C bus {0}", bus);
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
        flog::error("Command failed: {0} (exit code: {1})", command, result);
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
					    flog::info("VU GPSDR device found on I2C bus 2, make a power cycle now...");
						
						executeCommand("vgp set 4B2 1");
						delay(2000);
						
						executeCommand("vgp set 4B2 0");
						delay(500);
						
                        // Tell GPS to output 24MHz
                        try {
                            core::configManager.acquire();
                            bool lockToGpsFreq = core::configManager.conf["lockToGpsFreq"];
                            core::configManager.release(true);
                            core::gps.outputReferenceClock(lockToGpsFreq);
                        } catch (const std::exception& e) {
                            flog::error("GPS sendData failed: {0}", e.what());
                        	delay(500);
                        	continue;
                        }
                        delay(500);
						
						// Initialize Si5351
						try {
							core::si5351.initialize();
						} catch (const std::exception& e) {
							flog::error("Si5351 initialize failed: {0}", e.what());
							delay(500);
							continue;
						}
						
						flog::info("Power cycle completed.");
						delay(500);
						
						// Refresh device list
						core::modComManager.callInterface("RTL-SDR", RTL_SDR_SOURCE_IFACE_CMD_REFRESH, NULL, NULL);
					}
				}
			}
        } catch (const std::exception& e) {
            flog::error("Exception in DeviceRefreshWorker thread: {0}", e.what());
        }
		delay(2000);
    }
    
    flog::info("DeviceRefreshWorker thread stopped.");
}

void DeviceRefreshWorker::start() {
    if (workerThread.joinable()) {
        flog::error("DeviceRefreshWorker thread is already running!");
        return;
    }
    shouldStop.store(false);
    workerThread = std::thread(&DeviceRefreshWorker::workerLoop, this);
    flog::info("DeviceRefreshWorker thread started.");
}

void DeviceRefreshWorker::stop() {
    shouldStop.store(true);
    
    if (workerThread.joinable()) {
        workerThread.join();
        flog::info("DeviceRefreshWorker thread joined.");
    }
}

bool DeviceRefreshWorker::isRunning() const {
    return workerThread.joinable() && !shouldStop.load();
}

DeviceRefreshWorker::~DeviceRefreshWorker() {
    stop();
}