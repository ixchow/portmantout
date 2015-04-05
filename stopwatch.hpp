#include <iostream>
#ifdef __MACH__
#include <mach/mach.h>
#include <mach/mach_time.h>
namespace {
void stopwatch(const char *name) {
	uint64_t now = mach_absolute_time();
	static uint64_t prev = now;

	static double abs_to_sec = 0.0;
	if (abs_to_sec == 0.0) {
		mach_timebase_info_data_t tb;
		mach_timebase_info(&tb);
		abs_to_sec = 1.0e-9 * double(tb.numer) / double(tb.denom);
	}
	double elapsed = double(now - prev) * abs_to_sec;
	std::cout << name << ": " << elapsed  * 1000.0 << "ms" << std::endl;
}
}
#else
#include <time.h>
namespace {
void stopwatch(const char *name) {
	struct timespec ts;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
	double now = double(ts.tv_sec) + 1.0e-9 * double(ts.tv_nsec);
	static double prev = now;

	std::cout << name << ": " << (now - prev) * 1000.0 << "ms" << std::endl;

	prev = now;

}
}
#endif
