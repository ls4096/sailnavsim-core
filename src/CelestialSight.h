/**
 * Copyright (C) 2021 ls4096 <ls4096@8bitbyte.ca>
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

#ifndef _CelestialSight_h_
#define _CelestialSight_h_

#include <time.h>

#include <proteus/Celestial.h>


typedef struct {
	int obj;
	proteus_CelestialHorizontalCoord coord;
} CelestialSight;


int CelestialSight_init();

void CelestialSight_shoot(
	time_t t,
	const proteus_GeoPos* pos,
	int cloudPercent,
	double airPressure,
	double airTemp,
	CelestialSight* sight
);


#endif // _CelestialSight_h_
