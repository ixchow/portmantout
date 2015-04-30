#pragma once

#include <vector>
#include <stdint.h>
#include <cassert>


class Frac {
public:
	const uint32_t Digits = 12;
	const uint32_t MaxDigit = 0xff;

	uint32_t digits[FracDigits];
	void set_max() {
		for (auto &d : digits) {
			d = FracMaxDigit;
		}
	}
	void set_zero() {
		for (auto &d : digits) {
			d = 0;
		}
	}
};

class Interval {
public:
	Frac base;
	Frac length;
};

class Distribution {
public:
	Distribution(int32_t min, int32_t max, uint32_t initial_count) {
		for (int32_t v = min; v <= max; ++v) {
			counts.insert(std::make_pair(v, initial_count));
		}
	}
	//as range of [0x0000 - 0xffff] interval
	std::pair< uint16_t, uint16_t > range(int32_t val) {
		return 
	}
	std::map< int32_t, uint32_t > counts;
};

class Coder {
public:
	Coder();

	void write(int32_t val, Distribution &dist);

	void carry();

	void finish();

	Interval interval;

	std::vector< uint8_t > output;

	static void run_test();
};

class Decoder {
public:
	Decoder(uint8_t max_digit, std::vector< uint8_t > const &_digits);

	void read(uint8_t &val, uint8_t max_val);

	//convenience:
	void read(uint32_t &val, uint32_t max_val) {
		assert(max_val <= 0xff);
		uint8_t temp;
		read(temp, uint8_t(max_val));
		val = temp;
	}

	DigitParams params;

	uint32_t difference;
	uint32_t length;

	uint32_t get_digit() {
		if (next_digit < input.size()) return input[next_digit++];
		else return 0;
	}
	uint32_t next_digit;
	std::vector< uint8_t > input;
};
