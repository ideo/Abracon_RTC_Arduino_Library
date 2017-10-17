#include <Wire.h>
#include "AbraconRTC.h"

static bool writeRegister(uint8_t addr, uint8_t val) {
	Wire.beginTransmission(RTC_ADDR);
	if (Wire.write(addr)  != 1) {return 0;} // send address of register to write to
	if (Wire.write(val) != 1) {return 0;} // send updated register value
	Wire.endTransmission();

	return 1;
}

static bool selectRegister(uint8_t addr) {
	Wire.beginTransmission(RTC_ADDR);
	if (Wire.write(addr) != 1) {return 0;} // send address of register to select
	Wire.endTransmission();

	return 1;
}

RTCData getRTCData() {
	RTCData newRTCData = {0};
  
	// get time
	// select seconds register to begin reading from
	selectRegister(SEC_ADDR);

	Wire.requestFrom(RTC_ADDR, 3); // request 3 bytes for sec, min, hour
	if (Wire.available() == 3) { // make sure 3 bytes were returned
		uint8_t RTCSecVal  = Wire.read();
		newRTCData.sec1s   = RTCSecVal & 0x0F;
		newRTCData.sec10s  = (RTCSecVal >> 4) & 0x0F;

		uint8_t RTCMinVal  = Wire.read();
		newRTCData.min1s   = RTCMinVal & 0x0F;
		newRTCData.min10s  = (RTCMinVal >> 4) & 0x0F;

		uint8_t RTCHourVal = Wire.read();
		newRTCData.hrFormat  = (RTCHourVal >> 6) & 0x01;
		if (newRTCData.hrFormat) { // 12-hour format
			newRTCData.timeOfDay = (RTCHourVal >> 5) & 0x01;
			newRTCData.hour1s    = RTCHourVal & 0x0F;
			newRTCData.hour10s   = (RTCHourVal >> 4) & 0x01;
		} else { // 24-hour format
			newRTCData.hour1s    = RTCHourVal & 0x0F;
			newRTCData.hour10s   = (RTCHourVal >> 4) & 0x03;
		}
	} else { // something went wrong and we didn't get all 3 time registers
		return newRTCData;
	}

	// get temperature
	// select seconds register to begin reading from
	selectRegister(TEMP_ADDR);

	Wire.requestFrom(RTC_ADDR, 1); // request 1 byte for temperature
	if (Wire.available() == 1) { // make sure 1 byte was returned
		uint8_t RTCTempC = Wire.read() - 60;
		newRTCData.tempF  = RTCTempC * 1.8 + 32;
	}

	return newRTCData;
}

// reset RTC time to midnight
bool setTime(uint8_t hour=0, uint8_t min=0, uint8_t sec=0, bool PM=0) {
	// select hour register to get 12/24-hr preference
	if (!selectRegister(HOUR_ADDR)) {
		return 0;
	}

	uint8_t newHourVal    = 0;
	uint8_t newMinVal     = 0;
	uint8_t newSecVal     = 0;

	if ((min < 60 && min >= 0) && (sec < 60 && sec >= 0)) {
		uint8_t RTCMinVal1s   = min % 10;
		uint8_t RTCMinVal10s  = (min - RTCMinVal1s)/10;
		uint8_t RTCSecVal1s   = sec % 10;
		uint8_t RTCSecVal10s  = (sec - RTCSecVal1s)/10;
		newMinVal = RTCMinVal1s | (RTCMinVal10s << 4);
		newSecVal = RTCSecVal1s | (RTCSecVal10s << 4);
	} else {
		return 0;
	}

	Wire.requestFrom(RTC_ADDR, 1); // request 1 byte for hour
	if (Wire.available() == 1) { // make sure 1 byte was returned
		uint8_t RTCHourVal = Wire.read();
		bool RTCHourFormat = (RTCHourVal >> 6) & 0x01;
		if (RTCHourFormat) { // 12-hour mode
			if (hour <= 12 && hour >= 0) {
				if (hour == 0) {
					hour = 12;
				}
				uint8_t RTCHourVal1s  = hour % 10;
				uint8_t RTCHourVal10s = (hour - RTCHourVal1s)/10;
				bool RTCHourTimeOfDay = 0;
				if (PM) { // afternoon
					RTCHourTimeOfDay = 1;
				}
				newHourVal = RTCHourVal1s | (RTCHourVal10s << 4) | (RTCHourTimeOfDay << 5) | (RTCHourFormat << 6);
			} else {
				return 0;
			}
		} else { // 24-hour mode
			if (hour < 24 && hour >= 0) {
				if (PM) { // weird case where someone puts in 12-hour format but time is set to 24-hour format
					if ((hour + 12) < 24) {
						hour += 12;
					}
				}
				uint8_t RTCHourVal1s  = hour % 10;
				uint8_t RTCHourVal10s = (hour - RTCHourVal1s)/10;
				newHourVal = RTCHourVal1s | (RTCHourVal10s << 4) | (RTCHourFormat << 6);
			} else {
				return 0;
			}
		}
	} else {
		return 0;
	}

	Wire.beginTransmission(RTC_ADDR);
	if (Wire.write(SEC_ADDR)  != 1) {return 0;} // send address of register to write to
	if (Wire.write(newSecVal) != 1) {return 0;} // send updated register value
	if (Wire.write(newMinVal) != 1) {return 0;} // send updated register value
	if (Wire.write(newHourVal) != 1) {return 0;} // send updated register value
	Wire.endTransmission();

	return 1;
}

bool toggleHrFormat() {
	// select hour register to begin reading from
	if (!selectRegister(HOUR_ADDR)) {
		return 0;
	}

	bool	RTCHourFormat    = 0; // true for 12-hour, false for 24-hour
	bool	RTCHourTimeOfDay = 0; // true for PM, false for AM
	uint8_t RTCHourVal1s     = 0;
	uint8_t RTCHourVal10s    = 0;

	Wire.requestFrom(RTC_ADDR, 1); // request 1 byte for hour
	if (Wire.available() == 1) { // make sure 1 byte was returned
		uint8_t RTCHourVal = Wire.read();
		RTCHourFormat = (RTCHourVal >> 6) & 0x01;
		if (RTCHourFormat) {
			RTCHourTimeOfDay = (RTCHourVal >> 5) & 0x01;
			RTCHourVal1s     = RTCHourVal & 0x0F;
			RTCHourVal10s    = (RTCHourVal >> 4) & 0x01;
		} else {
			RTCHourVal1s     = RTCHourVal & 0x0F;
			RTCHourVal10s    = (RTCHourVal >> 4) & 0x03;
		}
	} else {
		return 0;
	}

	// toggle hour format and reformat hour data
	uint8_t newHourVal = 0;
	RTCHourFormat = !RTCHourFormat;
	if (RTCHourFormat) { // 24-hour to 12-hour
		RTCHourTimeOfDay = 0;
		if (RTCHourVal10s == 1) {
			if (RTCHourVal1s > 2) {
				RTCHourTimeOfDay = 1;
				RTCHourVal10s    = 0;
				RTCHourVal1s    -= 2;
			} else if (RTCHourVal1s == 2) {
				RTCHourTimeOfDay = 1;
			}
		} else if (RTCHourVal10s == 2) {
			RTCHourTimeOfDay = 1;
			if (RTCHourVal1s > 2) {
				RTCHourVal10s = 1;
				RTCHourVal1s -= 2;
			} else {
				RTCHourVal10s = 0;
				RTCHourVal1s += 8;
			}
		} else if (RTCHourVal1s == 0) { // 0 o'clock
			RTCHourVal1s =  2;
			RTCHourVal10s = 1;
			RTCHourTimeOfDay = 0;
		}
		newHourVal = RTCHourVal1s | (RTCHourVal10s << 4) | (RTCHourTimeOfDay << 5) | (RTCHourFormat << 6);
	} else { // 12-hour to 24-hour
		if (RTCHourTimeOfDay) {
			if (RTCHourVal10s == 0) {
				if (RTCHourVal1s < 8) {
					RTCHourVal1s += 2;
					RTCHourVal10s = 1;
				} else {
					RTCHourVal1s -= 8;
					RTCHourVal10s = 2;
				}
			} else if (RTCHourVal1s < 2) {
				RTCHourVal1s += 2;
				RTCHourVal10s = 2;
			}
		} else if (RTCHourVal1s == 2) { // midnight
			RTCHourVal1s  = 0;
			RTCHourVal10s = 0;
		}
		newHourVal = RTCHourVal1s | (RTCHourVal10s << 4) | (RTCHourFormat << 6);
	}

	// update hour register
	if (!writeRegister(HOUR_ADDR, newHourVal)) {
		return 0;
	}

	return 1;
}

bool incHour() {
	// get hour register data
	if (!selectRegister(HOUR_ADDR)) {
		return 0;
	}

	bool	RTCHourFormat    = 0; // true for 12-hour, false for 24-hour
	bool	RTCHourTimeOfDay = 0; // true for PM, false for AM
	uint8_t RTCHourVal1s     = 0;
	uint8_t RTCHourVal10s    = 0;

	Wire.requestFrom(RTC_ADDR, 1); // request 1 byte for hour
	if (Wire.available() == 1) { // make sure 1 byte was returned
		uint8_t RTCHourVal = Wire.read();
		RTCHourFormat = (RTCHourVal >> 6) & 0x01;
		if (RTCHourFormat) {
			RTCHourTimeOfDay = (RTCHourVal >> 5) & 0x01;
			RTCHourVal1s     = RTCHourVal & 0x0F;
			RTCHourVal10s    = (RTCHourVal >> 4) & 0x01;
		} else {
			RTCHourVal1s     = RTCHourVal & 0x0F;
			RTCHourVal10s    = (RTCHourVal >> 4) & 0x03;
		}
	} else {
		return 0;
	}

	// increment hour
	uint8_t newHourVal = 0;
	if (RTCHourFormat) { // 12-hour format
		if (RTCHourVal10s) {
			if (RTCHourVal1s == 2) { // 12 o'clock
				// reset to 1 o'clock
				RTCHourVal1s     = 1;
				RTCHourVal10s    = 0;
			} else {
				RTCHourVal1s++;
				if (RTCHourVal1s == 2) {
					RTCHourTimeOfDay = !RTCHourTimeOfDay;
				}
			}
		} else if (RTCHourVal1s == 9) {
			// 10 o'clock
			RTCHourVal1s  = 0;
			RTCHourVal10s = 1;
		} else {
			RTCHourVal1s++;
		}
		newHourVal = RTCHourVal1s | (RTCHourVal10s << 4) | (RTCHourTimeOfDay << 5) | (RTCHourFormat << 6);
	} else { // 24-hour format
		if (RTCHourVal10s == 2) {
			if (RTCHourVal1s == 3) { // 23 o'clock
				// reset to 0 o'clock
				RTCHourVal1s  = 0;
				RTCHourVal10s = 0;
			} else {
				RTCHourVal1s++;
			}
		} else if (RTCHourVal1s == 9) {
			RTCHourVal1s  = 0;
			RTCHourVal10s++;
		} else {
			RTCHourVal1s++;
		}
		newHourVal = RTCHourVal1s | (RTCHourVal10s << 4) | (RTCHourFormat << 6);
	}

	// update hour register
	if (!writeRegister(HOUR_ADDR, newHourVal)) {
		return 0;
	}

	return 1;
}

bool decHour() {
	// get hour register data
	if (!selectRegister(HOUR_ADDR)) {
		return 0;
	}

	bool	RTCHourFormat    = 0; // true for 12-hour, false for 24-hour
	bool	RTCHourTimeOfDay = 0; // true for PM, false for AM
	uint8_t RTCHourVal1s     = 0;
	uint8_t RTCHourVal10s    = 0;

	Wire.requestFrom(RTC_ADDR, 1); // request 1 byte for hour
	if (Wire.available() == 1) { // make sure 1 byte was returned
		uint8_t RTCHourVal = Wire.read();
		RTCHourFormat = (RTCHourVal >> 6) & 0x01;
		if (RTCHourFormat) {
			RTCHourTimeOfDay = (RTCHourVal >> 5) & 0x01;
			RTCHourVal1s     = RTCHourVal & 0x0F;
			RTCHourVal10s    = (RTCHourVal >> 4) & 0x01;
		} else {
			RTCHourVal1s     = RTCHourVal & 0x0F;
			RTCHourVal10s    = (RTCHourVal >> 4) & 0x03;
		}
	} else {
		return 0;
	}

	// decrement hour
	uint8_t newHourVal = 0;
	if (RTCHourFormat) { // 12-hour format
		if (RTCHourVal10s) {
			if (RTCHourVal1s == 0) { // 10 o'clock
				// 9 o'clock
				RTCHourVal1s  = 9;
				RTCHourVal10s = 0;
			} else {
				RTCHourVal1s--;
				if (RTCHourVal1s == 1) {
					RTCHourTimeOfDay = !RTCHourTimeOfDay;
				}
			}
		} else if (RTCHourVal1s == 1) {
			// 12 o'clock
			RTCHourVal1s  = 2;
			RTCHourVal10s = 1;
		} else {
			RTCHourVal1s--;
		}
		newHourVal = RTCHourVal1s | (RTCHourVal10s << 4) | (RTCHourTimeOfDay << 5) | (RTCHourFormat << 6);
	} else {
		if (RTCHourVal10s > 0) {
			if (RTCHourVal1s == 0) {
				RTCHourVal1s = 9;
				RTCHourVal10s--;
			} else {
				RTCHourVal1s--;
			}
		} else {
			if (RTCHourVal1s == 0) {
				RTCHourVal1s  = 3;
				RTCHourVal10s = 2;
			} else {
				RTCHourVal1s--;
			}
		}
		newHourVal = RTCHourVal1s | (RTCHourVal10s << 4) | (RTCHourFormat << 6);
	}

	// update hour register
	if (!writeRegister(HOUR_ADDR, newHourVal)) {
		return 0;
	}

	return 1;
}

bool incMinute() {
	// get minute register data
	if (!selectRegister(MIN_ADDR)) {
		return 0;
	}

	uint8_t RTCMinVal1s  = 0;
	uint8_t RTCMinVal10s = 0;

	Wire.requestFrom(RTC_ADDR, 1); // request 1 byte for minute
	if (Wire.available() == 1) { // make sure 1 byte was returned
		uint8_t RTCMinVal = Wire.read();
		RTCMinVal1s  = RTCMinVal & 0x0F;
		RTCMinVal10s = (RTCMinVal >> 4) & 0x0F;
	} else {
		return 0;
	}

	// increment min
	if (RTCMinVal10s == 5) {
		if (RTCMinVal1s == 9) {
			// reset to 0 min
			RTCMinVal1s  = 0;
			RTCMinVal10s = 0;
		} else {
			RTCMinVal1s++;
		}
	} else if (RTCMinVal1s == 9) {
		RTCMinVal1s = 0;
		RTCMinVal10s++;
	} else {
		RTCMinVal1s++;
	}
	uint8_t newMinVal = RTCMinVal1s | (RTCMinVal10s << 4);

	// update min register
	if (!writeRegister(MIN_ADDR, newMinVal)) {
		return 0;
	}

	return 1;
}

bool decMinute() {
	// get minute register data
	if (!selectRegister(MIN_ADDR)) {
		return 0;
	}

	uint8_t RTCMinVal1s  = 0;
	uint8_t RTCMinVal10s = 0;

	Wire.requestFrom(RTC_ADDR, 1); // request 1 byte for minute
	if (Wire.available() == 1) { // make sure 1 byte was returned
		uint8_t RTCMinVal = Wire.read();
		RTCMinVal1s  = RTCMinVal & 0x0F;
		RTCMinVal10s = (RTCMinVal >> 4) & 0x0F;
	} else {
		return 0;
	}

	// decrement min
	if (RTCMinVal1s == 0) {
		RTCMinVal1s  = 9;
		if (RTCMinVal10s == 0) {
			RTCMinVal10s = 5;
		} else {
			RTCMinVal10s--;
		}
	} else {
		RTCMinVal1s--;
	}
	uint8_t newMinVal = RTCMinVal1s | (RTCMinVal10s << 4);

	// update min register
	if (!writeRegister(MIN_ADDR, newMinVal)) {
		return 0;
	}

	return 1;
}
