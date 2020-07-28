#include "ErrLog.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

void ErrLog_log(const char* id, const char* msg, ...)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);

	char fmt[4096];
	sprintf(fmt, "[%ld.%03ld] %s: %s\n", ts.tv_sec, ts.tv_nsec / 1000000, id, msg);

	va_list arg;
	va_start(arg, msg);
	vfprintf(stderr, fmt, arg);
	va_end(arg);
}
