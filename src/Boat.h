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

#ifndef _Boat_h_
#define _Boat_h_

#include <stdbool.h>

#include <proteus/GeoVec.h>
#include <proteus/GeoPos.h>


#define BOAT_FLAG_TAKES_DAMAGE (0x01)
#define BOAT_FLAG_WAVE_SPEED_EFFECT (0x02)


typedef struct
{
	proteus_GeoPos pos;
	proteus_GeoVec v;
	proteus_GeoVec vGround;

	double desiredCourse;
	double distanceTravelled;
	double damage;

	int boatType;
	int boatFlags;

	bool stop;
	bool sailsDown;
	bool movingToSea;

	bool setImmediateDesiredCourse;
} Boat;


int Boat_init();

Boat* Boat_new(double lat, double lon, int boatType, int boatFlags);
void Boat_advance(Boat* b);
bool Boat_isHeadingTowardWater(Boat* b);


#endif // _Boat_h_
