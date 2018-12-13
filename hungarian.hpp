#pragma once

#include <string.h>

#include "stopwatch.hpp"

void min_assignment(uint32_t size, uint8_t const *weights) {
	std::unique_ptr< uint8_t[] > row_potential(new uint8_t[size]);
	std::unique_ptr< uint8_t[] > column_potential(new uint8_t[size]);

	std::unique_ptr< uint8_t[] > residual(new uint8_t[size * size]);

	//initialize potentials with a sweep:
	for (uint32_t c = 0; c < size; ++c) {
		column_potential[c] = 0xff;
	}
	for (uint32_t r = 0; r < size; ++r) {
		uint8_t min = 0xff;
		for (uint32_t c = 0; c < size; ++c) {
			min = std::min(min, weights[r * size + c]);
		}
		row_potential[r] = min;
		for (uint32_t c = 0; c < size; ++c) {
			assert(weights[r * size + c] >= min);
			uint8_t res = weights[r * size + c] - min;
			residual[r * size + c] = res;
			column_potential[c] = std::min(column_potential[c], res);
		}
	}
	for (uint32_t r = 0; r < size; ++r) {
		for (uint32_t c = 0; c < size; ++c) {
			assert(residual[r * size + c] >= column_potential[c]);
			residual[r * size + c] -= column_potential[c];
		}
	}
	auto check_potential = [&]() -> bool {
		uint64_t tight = 0;
		uint64_t error = 0;
		for (uint32_t r = 0; r < size; ++r) {
			for (uint32_t c = 0; c < size; ++c) {
				uint32_t sum = uint32_t(row_potential[r]) + uint32_t(column_potential[c]);
				uint32_t weight = weights[r * size + c];
				if (sum == weight) ++tight;
				if (sum > weight) ++error;
			}
		}
		std::cout << "Have " << tight << " tight edges (" << 100.0 * tight / double(size * size) << "%) and " << error << " edges that are errors." << std::endl;
		return error == 0;
	};
	auto potential_sum = [&]() -> uint32_t {
		uint32_t sum = 0;
		for (uint32_t i = 0; i < size; ++i) {
			sum += row_potential[i];
			sum += column_potential[i];
		}
		return sum;
	};

	std::cout << "After initialization, potential sum is " << potential_sum() << std::endl;
	if (!check_potential()) exit(1);

	stopwatch("init");

	{ //find a maximum matching over only tight edges:
		std::vector< std::vector< uint16_t > > rc(size);
		const uint16_t NoIndex = 0xffff;
		std::vector< uint16_t > cr(size, NoIndex);
		//rc edges are not in the matching, cr edges are in the matching
		for (uint32_t r = 0; r < size; ++r) {
			for (uint32_t c = 0; c < size; ++c) {
				if (row_potential[r] + column_potential[c] == weights[r * size + c]) {
					rc[r].emplace_back(c);
				}
			}
		}
		stopwatch("edges");
		std::vector< bool > r_matched(size, false), c_matched(size, false);
		for (uint32_t c = 0; c < size; ++c) {
			if (cr[c] != NoIndex) {
				c_matched[c] = true;
				r_matched[rc[c]] = true;
			}
		}
		std::vector< uint32_t > r_from(size, NoIndex);
		std::vector< uint32_t > c_from(size, NoIndex);
		std::vector< uint32_t > r_to_expand;
		for (uint32_t r = 0; r < size; ++r) {
			if (r_matched[r]) continue;
			r_from[r] = r;
			r_to_expand.emplace_back(r);
		}
		while (!r_to_expand.empty()) {
			std::vector< uint32_t > next_r_to_expand;
			for (auto r : r_to_expand) {
				for (auto c : rc[r]) {
					if (
					//... etc ...
				}
			}
		}
	}


}
