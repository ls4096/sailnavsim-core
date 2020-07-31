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

#ifndef _Boat_h_
#define _Boat_h_

#include <stdbool.h>

#include <proteus/GeoVec.h>
#include <proteus/GeoPos.h>


typedef struct
{
	proteus_GeoPos pos;
	proteus_GeoVec v;

	double desiredCourse;

	int boatType;

	bool stop;
	bool sailsDown;
	bool movingToSea;

	bool setImmediateDesiredCourse;
} Boat;


int Boat_init();

Boat* Boat_new(double lat, double lon, int boatType);
void Boat_advance(Boat* b, double s);
bool Boat_isHeadingTowardWater(Boat* b);


#endif // _Boat_h_
