/**
 * Copyright (C) 2020 ls4096 <ls4096@8bitbyte.ca>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

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
