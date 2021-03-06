#include "Coder.hpp"

#include <random>
#include <cassert>
#include <iostream>
#include <sstream>
#include <ctime>

Coder::Coder() {
	interval.base.set_zero();
	interval.length.set_max();
}

void Coder::write(int32_t val, Distribution const &dist) {
	assert(dist.counts.count(val));

	auto range = dist.range(val);

	interval.length.div(0x10000);
	assert(interval.length.is_nonzero());

	bool carry_out;
	Frac temp = interval.length;
	temp.mul(range.first);
	interval.base.add(temp, &carry_out);

	interval.length.mul(range.second - range.first + 1);

	if (carry_out) {
		carry();
	}

	while (interval.length.digits[0] == 0) {
		assert(interval.length.is_nonzero());
		output.push_back(interval.base.digits[0]);
		interval.base.digits[0] = 0;
		interval.base.mul(Frac::MaxDigit + 1);
		interval.length.mul(Frac::MaxDigit + 1);
	}
}

void Coder::carry() {
	auto o = output.rbegin();
	while (1) {
		assert(o != output.rend());
		*o += 1;
		if (*o > Frac::MaxDigit) {
			*o -= (Frac::MaxDigit + 1);
		} else {
			break;
		}
		++o;
	}
}

void Coder::finish() {
	uint32_t msd_of_base = interval.base.digits[0];

	if (msd_of_base + 1 > Frac::MaxDigit) {
		carry();
	} else {
		output.push_back(msd_of_base + 1);
	}

	//trim trailing zeros:
	while (!output.empty() && output.back() == 0) {
		output.pop_back();
	}
}
/*
Decoder::Decoder(uint8_t max_digit, std::vector< uint8_t > const &_input) : params(max_digit), input(_input) {
	difference = 0; //"coded - base"
	length = params.max_length;

	next_digit = 0;
	for (uint32_t i = 0; i < params.active_digits; ++i) {
		difference = (difference * params.digits) + get_digit();
	}
	assert(difference <= params.max_length);
}

void Decoder::read(uint8_t &val, uint8_t max_val) {
	//figure out encoded value at current offset:
	length /= uint32_t(max_val) + 1;
	val = difference / length;
	difference -= length * val;

	//re-normalize if needed:
	while (length <= params.renorm_length) {
		length *= params.digits;
		uint32_t digit = get_digit();
		difference = difference * params.digits + digit;
	}
}

std::string in_base(uint32_t n, uint8_t max_val) {
	std::vector< uint8_t > ret;
	DigitParams params(max_val);
	for (uint32_t i = 0; i < params.active_digits; ++i) {
		ret.push_back(n % params.digits);
		n /= params.digits;
	}
	assert(n == 0);
	std::ostringstream ss;
	for (auto d = ret.rbegin(); d != ret.rend(); ++d) {
		if (d != ret.rbegin()) ss << "|";
		if (*d < 100 && max_val >= 100) ss << ' ';
		if (*d < 10 && max_val >= 10) ss << ' ';
		ss << int(*d);
	}
	ss << " (" << uint32_t(max_val) + 1 << ")";
	return ss.str();
}

//------------------------------------------
void Coder::run_test() {
	//std::mt19937 mt(0x1112234); //error at 86-digit pass
	std::mt19937 mt(0x3dc0ffee); //error at 87-digit pass
	//std::mt19937 mt(0x1234abcd); //error at 42-digit pass

	clock_t before = std::clock();

	for (uint32_t max_digit = 1; max_digit <= 255; ++max_digit) {
	double min_delta = std::numeric_limits< double >::infinity();
	double max_delta =-std::numeric_limits< double >::infinity();
	DigitParams params(max_digit);
	std::cout << "Digits: " << params.digits << ", active: " << params.active_digits << ", renorm_length: " << params.renorm_length << ", max_length: " << params.max_length << "." << std::endl;
	for (int pass = 0; pass < 3; ++pass) {
	//std::cout << "Digits: " << max_digit + 1 << ", Pass: " << pass << std::endl;
	for (uint32_t iter = 0; iter < 1000; ++iter) {
		std::vector< std::pair< uint8_t, uint8_t > > data;
		data.resize(1 + iter);
		for (auto d = data.begin(); d != data.end(); ++d) {
			uint8_t a = mt();
			uint8_t b = mt() % (uint32_t(a) + 1);
			if (pass == 0) {
				d->first = a;
			} else if (pass == 1) {
				d->first = 0;
				//if (a == 0) d->first = 0;
				//else  d->first = 1;
			} else if (pass == 2) {
				d->first = b;
			}
			d->second = a;
			assert(d->first <= d->second);
		}

		//uint8_t max_digit = 31;

		Coder coder(max_digit);
		for (auto d = data.begin(); d != data.end(); ++d) {
			coder.write(d->first, d->second);
		}
		coder.finish();

		for (auto d = coder.output.begin(); d != coder.output.end(); ++d) {
			assert(*d <= max_digit);
		}

		double log_sum = 0.0;
		for (auto d = data.begin(); d != data.end(); ++d) {
			log_sum += log(double(d->second) + 1.0);
		}
		log_sum /= log(double(max_digit) + 1.0);

		double delta = coder.output.size() - log_sum;
		if (delta < min_delta) min_delta = delta;
		if (delta > max_delta) max_delta = delta;

		//std::cout << "Coded: " << coder.output.size() << " Opt: " << log_sum << "\n";

		Decoder decoder(max_digit, coder.output);
		for (auto d = data.begin(); d != data.end(); ++d) {
			uint8_t test = 0;
			decoder.read(test, d->second);
			//std::cout << "Digit " << (d - data.begin()) << " expected " << uint32_t(d->first) << " of " << (uint32_t(d->second)+1) << " got " << uint32_t(test) << std::endl;
			if (test != d->first) {
				std::cout << "[" << (d - data.begin()) << "/" << data.size() << "] " << int(d->first) << " of " << int(d->second) << ", decoded to " << int(test) << "." << std::endl;
				//std::cout << " last value " << (was_carry ? "was a carry" : "was an output") << std::endl;
				//std::cout << " last base:   " << in_base(last_base, max_digit) << std::endl;
				//std::cout << " last length: " << in_base(last_length, max_digit) << std::endl;
				assert(test == d->first);
			}
		}
	} //for iter
	} //for pass
	std::cout << "Within [" << min_delta << ", " << max_delta << "] digits of optimal." << std::endl;
	}

	clock_t after = std::clock();

	std::cout << "Whole run took " << (after - before) / double(CLOCKS_PER_SEC) << " seconds." << std::endl;
}
*/
