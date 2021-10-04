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


#define BOAT_FLAG_TAKES_DAMAGE			(0x0001)
#define BOAT_FLAG_WAVE_SPEED_EFFECT		(0x0002)
#define BOAT_FLAG_CELESTIAL			(0x0004)
#define BOAT_FLAG_CELESTIAL_WAVE_EFFECT		(0x0008)
#define BOAT_FLAG_DAMAGE_APPARENT_WIND		(0x0010)


typedef struct
{
	proteus_GeoPos pos;
	proteus_GeoVec v; // Always using true compass angles.
	proteus_GeoVec vGround; // Always using true compass angles.

	double desiredCourse;
	double distanceTravelled;
	double damage;

	int boatType;
	int boatFlags;

	int startingFromLandCount;

	bool stop;
	bool sailsDown;
	bool movingToSea;

	bool setImmediateDesiredCourse;
	bool courseMagnetic;
} Boat;


int Boat_init();

Boat* Boat_new(double lat, double lon, int boatType, int boatFlags);
void Boat_advance(Boat* b, time_t curTime);
bool Boat_isHeadingTowardWater(const Boat* b, time_t curTime);
bool Boat_getWaveAdjustedCelestialAzAlt(const Boat* b, double* az, double* alt);


#endif // _Boat_h_
