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

#ifndef _tests_assert_h_
#define _tests_assert_h_

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#define EQUALS(x, y) do { \
	if (x != y) { printf("\t%s:%d: %s\n", __FILE__, __LINE__, #x " != " #y); return 1; } \
} while (0)

#define SAILNAVSIM_TESTS_DBL_EPSILON (0.000000001)
#define EQUALS_DBL(x, y) do { \
	if (fabs(x - y) > SAILNAVSIM_TESTS_DBL_EPSILON) { printf("\t%s:%d: %s\n", __FILE__, __LINE__, #x " !~= " #y); return 1; } \
} while (0)

#define SAILNAVSIM_TESTS_FLT_EPSILON (0.00001)
#define EQUALS_FLT(x, y) do { \
	if (fabsf(x - y) > SAILNAVSIM_TESTS_FLT_EPSILON) { printf("\t%s:%d: %s\n", __FILE__, __LINE__, #x " !~= " #y); return 1; } \
} while (0)

#define IS_TRUE(x) do { \
	if ((x) != true) { printf("\t%s:%d: %s\n", __FILE__, __LINE__, #x " is FALSE"); return 1; } \
} while (0)

#define IS_FALSE(x) do { \
	if ((x) != false) { printf("\t%s:%d: %s\n", __FILE__, __LINE__, #x " is TRUE"); return 1; } \
} while (0)

#endif // _tests_assert_h_
