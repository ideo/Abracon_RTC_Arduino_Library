#ifndef ABRACONRTC_H_
#define ABRACONRTC_H_

#include <inttypes.h>

// RTC I2C register addresses
#define RTC_ADDR   	  0x56 // 7 bit, without least sig R/W bit

// RTC addresses
#define CTL_1_ADDR	  0x00 // control page register
#define CTL_STAT_ADDR 0x03 // control status register
#define SEC_ADDR   	  0x08 // seconds address
#define MIN_ADDR   	  0x09 // minutes address
#define HOUR_ADDR  	  0x0A // hours address
#define TEMP_ADDR  	  0x20 // temperature address
#define EE_CTL_ADDR   0x30 // EEPROM control address

struct RTCData {
	bool 	hrFormat; // true for 12-hour format, false for 24-hour format
	bool	timeOfDay; // if 12-hour format, true for PM, false for AM
	uint8_t hour10s;
	uint8_t hour1s;
	uint8_t min10s;
	uint8_t min1s;
	uint8_t sec10s;
	uint8_t sec1s;
	uint8_t tempF;
};


class AbraRTC {
	private:
		static uint8_t error;
		static RTCData AbraRTCData;

		static bool writeRegister(uint8_t addr, uint8_t val);
		static bool selectRegister(uint8_t addr);
		static bool readRegister(uint8_t addr, uint8_t &readVal);
		static bool writeBit(uint8_t addr, uint8_t bitPosition, bool val);

		static uint8_t hour12to24(uint8_t RTCHourTimeOfDay, uint8_t RTCHourVal1s, uint8_t RTCHourVal10s);
		static uint8_t hour24to12(uint8_t RTCHourVal1s, uint8_t RTCHourVal10s);
		static bool checkEEPROMBusy();
	public:
		AbraRTC();
		void begin();

		void updateRTC();
		uint8_t getHour1s() { return AbraRTCData.hour1s; }
		uint8_t getHour10s() { return AbraRTCData.hour10s; }
		bool 	getHrFormat() { return AbraRTCData.hrFormat; }
		bool	getTimeOfDay() { return AbraRTCData.timeOfDay; }
		uint8_t getMin1s() { return AbraRTCData.min1s; }
		uint8_t getMin10s() { return AbraRTCData.min10s; }
		uint8_t getSec1s() { return AbraRTCData.sec1s; }
		uint8_t getSec10s() { return AbraRTCData.sec10s; }
		uint8_t getTempF() { return AbraRTCData.tempF; }

		bool setTime(uint8_t hour=0, uint8_t min=0, uint8_t sec=0, bool PM=0);
		bool setTrickleCharge(bool enable);
		bool toggleHrFormat();
		bool setHrFormat(bool newHrFormat);
		bool incHour();
		bool decHour();
		bool incMinute();
		bool decMinute();
};

extern AbraRTC RTC;

#endif
