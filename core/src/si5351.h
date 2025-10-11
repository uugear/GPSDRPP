#ifndef SI5351_H
#define SI5351_H

#define SI5351_I2C_BUS         "/dev/i2c-2"
#define SI5351_I2C_ADDR        0x60
#define SI5351_I2C_LOCK        "/var/lock/vu_gpsdr_si5153_i2c.lock"

class Si5351 {
public:
    Si5351();
    ~Si5351();
	
	bool initialize();
	
	bool isInitialized();
	
	void turnOnClk1();
	void turnOffClk1();
	
    void turnOnClk2();
	void turnOffClk2();
	
    int writeRegister(unsigned char reg, unsigned char value);
	int readRegister(unsigned char reg);

private:
	bool initialized = false;

    bool clk0;
    bool clk1;
    bool clk2;
    
    int lock_fd;
    int i2c_fd;
	
	int acquireLock();
	void releaseLock();
    void controlOutputs();
};

#endif // SI5351_H
