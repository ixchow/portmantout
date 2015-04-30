#pragma once

#include <vector>
#include <stdint.h>
#include <cassert>
#include <map>
#include <algorithm>

#include <iostream>


class Frac {
public:
	static const uint32_t Digits = 12;
	static const uint32_t MaxDigit = 0xff;

	uint32_t digits[Digits];
	void set_max() {
		for (auto &d : digits) {
			d = MaxDigit;
		}
	}
	void set_zero() {
		for (auto &d : digits) {
			d = 0;
		}
	}
	bool is_nonzero() {
		for (auto &d : digits) {
			if (d != 0) return true;
		}
		return false;
	}

	//if carry_out isn't set, you are *sure* there is no carry
	//NOTE: it is safe to "add" a Frac to itself.
	void add(Frac const &o, bool *carry_out) {
		bool carry = false;
		for (uint32_t i = Digits - 1; i < Digits; --i) {
			uint32_t sum = digits[i] + o.digits[i] + (carry ? 1 : 0);
			if (sum > MaxDigit) {
				digits[i] = sum - (MaxDigit + 1);
				carry = true;
			} else {
				digits[i] = sum;
				carry = false;
			}
		}
		if (carry) {
			assert(carry_out);
			*carry_out = true;
		} else {
			if (carry_out) *carry_out = false;
		}
	}

	std::string rep() const {
		std::string ret = ".";
		for (auto d : digits) {
			ret += "0123456789abcdef"[d/16];
			ret += "0123456789abcdef"[d%16];
		}
		return ret;
	}

	void mul(uint32_t val) {
		Frac acc(*this);
		set_zero();
		while (1) {
			if (val % 2) {
				add(acc, NULL);
			}
			val /= 2;
			if (val == 0) break;
			acc.add(acc, NULL);
		}
	}

	void div(uint32_t val) {
		uint64_t ctx = 0;
		for (auto &d : digits) {
			ctx = ctx * uint64_t(MaxDigit + 1) + uint64_t(d);
			d = ctx / val;
			assert(d <= MaxDigit);
			ctx -= uint64_t(d) * uint64_t(val);
		}
	}
};

//Interval is [base, base + length]
class Interval {
public:
	Frac base;
	Frac length;
};

class Distribution {
public:
	Distribution(std::map< int32_t, uint32_t > const &counts_) : counts(counts_) {
	}
	Distribution(int32_t min, int32_t max, uint32_t initial_count) {
		for (int32_t v = min; v <= max; ++v) {
			counts.insert(std::make_pair(v, initial_count));
		}
	}
	//as range of [0x0000 - 0xffff] interval
	std::pair< uint16_t, uint16_t > range(int32_t val) const {
		//so out of [0x0000, 0xffff], allocate to each value:
		// allocate code space proportional to probability,
		//  **with the caveat** that everything we might see needs at least one
		//  unit of the interval

		uint32_t max = 0xffff;

		assert(counts.size() < max + 1);

		uint32_t total = 0;
		std::vector< std::pair< uint32_t, int32_t > > mass;
		mass.reserve(counts.size());
		for (auto c : counts) {
			mass.emplace_back(c.second, c.first);
			total += mass.back().first;
		}

		std::sort(mass.begin(), mass.end());
		uint32_t remain = max + 1;
		uint32_t base = 0;

		std::pair< uint16_t, uint16_t > ret;
		bool found = false;

		for (auto m : mass) {
			uint32_t length = m.first * remain / total;
			if (length < 1) length = 1;
			if (m.second == val) {
				ret = std::make_pair(base, base + length - 1);
				found = true;
			}
			assert(m.first <= total);
			total -= m.first;

			assert(length <= remain);
			remain -= length;
			assert(base <= max);
			base += length;
		}
		assert(remain == 0);
		assert(base == max + 1);
		assert(found);

		return ret;
	}
	void update_for_val(int32_t val) {
		auto f = counts.find(val);
		assert(f != counts.end());
		assert(f->second > 0);
		f->second += 1;
	}
	std::map< int32_t, uint32_t > counts;
};

class Coder {
public:
	Coder();

	void write(int32_t val, Distribution const &dist);

	void carry();

	void finish();

	Interval interval;

	std::vector< uint8_t > output;

	static void run_test();
};
/*
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
*/
