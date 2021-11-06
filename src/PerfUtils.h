/**
 * Copyright (C) 2020-2021 ls4096 <ls4096@8bitbyte.ca>
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

#ifndef _PerfUtils_h_
#define _PerfUtils_h_

#include <stdbool.h>


char* PerfUtils_getRandomName();
double PerfUtils_getRandomLat();
double PerfUtils_getRandomLon();
int PerfUtils_getRandomBoatType();
int PerfUtils_getRandomBoatFlags();
int PerfUtils_getRandomCourse();
bool PerfUtils_getRandomBool();
const char* PerfUtils_getRandomBoatGroupName();


#endif // _PerfUtils_h_
