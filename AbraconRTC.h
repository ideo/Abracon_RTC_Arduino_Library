#ifndef ABRACONRTC_H_
#define ABRACONRTC_H_

#include <inttypes.h>

// RTC register addresses
#define RTC_ADDR   	  0x56 // 7 bit, without least sig R/W bit
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

RTCData getRTCData();
uint8_t RTCBegin();
bool setTime(uint8_t hour=0, uint8_t min=0, uint8_t sec=0, bool PM=0); // resets time to midnight
bool setTrickleCharge(bool enable);
bool toggleHrFormat();
bool incHour();
bool decHour();
bool incMinute();
bool decMinute();

#endif
