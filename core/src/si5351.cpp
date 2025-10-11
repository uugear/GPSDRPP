#include "si5351.h"
#include <stdio.h>
#include <iostream>
#include <string>
#include <sstream>
#include <utils/flog.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>


// Constructor
Si5351::Si5351()
    : clk0(true), clk1(false), clk2(false) {
}

// Destructor
Si5351::~Si5351() {
    close(i2c_fd);
}

bool Si5351::initialize() {
	initialized = false;

	flog::info("Initialize Si5351...");

	// Open Si5351 I2C device
    i2c_fd = open(SI5351_I2C_BUS, O_RDWR);
    if (i2c_fd < 0) {
        perror("initialize: failed to open I2C bus");
        return false;
    }
    if (ioctl(i2c_fd, I2C_SLAVE, SI5351_I2C_ADDR) < 0) {
        perror("initialize: failed to open Si5351 I2C device");
        close(i2c_fd);
		i2c_fd = -1;
        return false;
    }
    uint8_t test_byte;
	if (read(i2c_fd, &test_byte, 1) < 0) {
		flog::warn("Si5351 is not connected");
		close(i2c_fd);
		i2c_fd = -1;
		return false;
	} else {
		flog::info("Connected to Si5351");
	}

	initialized = true;	// set flag now, or writeRegister will fail

	// Turn off all outputs
    writeRegister(3, 0xFF);

    // PLL Input Source, select the XTAL input
    writeRegister(15, 0x00);

    // Configure PLL A for 864 MHz (36 * 24 MHz)
    writeRegister(26, 0x00);
    writeRegister(27, 0x01);
    writeRegister(28, 0x00);
    writeRegister(29, 0x10);
    writeRegister(30, 0x00);
    writeRegister(31, 0x00);
    writeRegister(32, 0x00);
    writeRegister(33, 0x00);

    // Configure PLL B for 720 MHz (30 * 24 MHz)
    writeRegister(34, 0x00);
    writeRegister(35, 0x01);
    writeRegister(36, 0x00);
    writeRegister(37, 0x0D);
    writeRegister(38, 0x00);
    writeRegister(39, 0x00);
    writeRegister(40, 0x00);
    writeRegister(41, 0x00);

    // Configure Multisynth 0 for 28.8 MHz using PLL A (864 MHz / 30)
    writeRegister(42, 0x00);
    writeRegister(43, 0x01);
    writeRegister(44, 0x00);
    writeRegister(45, 0x0D);
    writeRegister(46, 0x00);
    writeRegister(47, 0x00);
    writeRegister(48, 0x00);
    writeRegister(49, 0x00);

    // Configure Multisynth 1 for 108 MHz using PLL A (864 MHz / 8)
    writeRegister(50, 0x00);
    writeRegister(51, 0x01);
    writeRegister(52, 0x00);
    writeRegister(53, 0x02);
    writeRegister(54, 0x00);
    writeRegister(55, 0x00);
    writeRegister(56, 0x00);
    writeRegister(57, 0x00);

    // Configure Multisynth 2 for 10 MHz using PLL B (720 MHz / 72)
    writeRegister(58, 0x00);
    writeRegister(59, 0x01);
    writeRegister(60, 0x00);
    writeRegister(61, 0x22);
    writeRegister(62, 0x00);
    writeRegister(63, 0x00);
    writeRegister(64, 0x00);
    writeRegister(65, 0x00);

    // Set CLK0 to use Multisynth 0
    writeRegister(16, 0x4F);

    // Set CLK1 to use Multisynth 1
    writeRegister(17, 0x4F);

    // Set CLK2 to use Multisynth 2
    writeRegister(18, 0x6F);

    // Turn off CLK3~CLK7 (doesn't exist)
    writeRegister(19, 0x80);
    writeRegister(20, 0x80);
    writeRegister(21, 0x80);
    writeRegister(22, 0x80);
    writeRegister(23, 0x80);

    // Reset PLL A and B
    writeRegister(177, 0xA0);

    // Control outputs
    controlOutputs();

	return true;
}

bool Si5351::isInitialized() {
	return initialized;
}

void Si5351::turnOnClk1() {
	if (initialized) {
		clk1 = true;
		controlOutputs();
	}
}

void Si5351::turnOffClk1() {
	if (initialized) {
		clk1 = false;
		controlOutputs();
	}
}

void Si5351::turnOnClk2() {
	if (initialized) {
		clk2 = true;
		writeRegister(177, 0x80);   // Reset PLL B
		controlOutputs();
	}
}

void Si5351::turnOffClk2() {
	if (initialized) {
		clk2 = false;
		controlOutputs();
	}
}

int Si5351::acquireLock() {
    lock_fd = open(SI5351_I2C_LOCK, O_CREAT | O_RDWR, 0666);
    if (lock_fd < 0) {
        perror("Failed to open lock file");
        return -1;
    }
    if (flock(lock_fd, LOCK_EX) < 0) {
        perror("Failed to acquire lock");
        close(lock_fd);
        return -1;
    }
    return lock_fd;
}

void Si5351::releaseLock() {
    flock(lock_fd, LOCK_UN);
    close(lock_fd);
}

// Write to the I2C register
int Si5351::writeRegister(unsigned char reg, unsigned char value) {
	if (!initialized && !initialize()) {
		flog::warn("writeRegister: Si5351 is not connected yet");
		return false;
	}

    unsigned char buffer[2] = { reg, value };

    acquireLock();
    if (lock_fd < 0) {
        perror("writeRegister: failed to lock Si5351 I2C device");
        return -1;
    }

    // write the register index and value to I2C device
    if (write(i2c_fd, buffer, 2) != 2) {
        perror("writeRegister: failed to write to Si5351 I2C device");
        releaseLock();
        return -1;
    }

    releaseLock();
    return 0;
}

// Read from the I2C register
int Si5351::readRegister(unsigned char reg) {
	if (!initialized && !initialize()) {
		flog::warn("readRegister: Si5351 is not connected yet");
		return false;
	}

    unsigned char buffer[1] = { reg };

    acquireLock();
    if (lock_fd < 0) {
        perror("readRegister: failed to lock Si5351 I2C device");
        return -1;
    }

    // write the register index to I2C device
    if (write(i2c_fd, buffer, 1) != 1) {
        perror("readRegister: failed to write register address to Si5351 I2C device");
        releaseLock();
        return -1;
    }

    // read the value from the register
    if (read(i2c_fd, buffer, 1) != 1) {
        perror("readRegister: failed to read from Si5351 I2C device");
        releaseLock();
        return -1;
    }

    releaseLock();
    return buffer[0];
}

void Si5351::controlOutputs() {
    unsigned char oec = 0xFF;
	if (clk0) oec &= 0xFE;
	if (clk1) oec &= 0xFD;
	if (clk2) oec &= 0xFB;
    writeRegister(3, oec);
}
