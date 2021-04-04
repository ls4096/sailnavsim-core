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

#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "CelestialSight.h"

#include "ErrLog.h"


#define ERRLOG_ID "CelestialSight"


static unsigned int _randSeed = 0;
static bool isObscuredByCloudRandom(int cloudPercent);


int CelestialSight_init()
{
	_randSeed = time(0);
	return 0;
}

void CelestialSight_shoot(
	time_t t,
	const proteus_GeoPos* pos,
	int cloudPercent,
	double airPressure,
	double airTemp,
	CelestialSight* sight
)
{
	sight->obj = -1;

	if (isObscuredByCloudRandom(cloudPercent))
	{
		// Obscured by clouds, so no sight this time.
		return;
	}

	proteus_CelestialEquatorialCoord ec;
	proteus_CelestialHorizontalCoord hc;

	const double JD = proteus_Celestial_getJulianDayForTime(t);

	if (0 != proteus_Celestial_getEquatorialForObject(JD, PROTEUS_CELESTIAL_OBJ_SUN, &ec))
	{
		ERRLOG("Failed to get equatorial coordinates for Sun!");
		return;
	}

	if (0 != proteus_Celestial_convertEquatorialToHorizontal(JD, pos, &ec, true, airPressure, airTemp, &hc))
	{
		ERRLOG("Failed to convert coordinates for Sun!");
		return;
	}

	if (hc.alt > 0.0)
	{
		// Sun is up, so return sight for Sun.

		sight->obj = PROTEUS_CELESTIAL_OBJ_SUN;
		sight->coord.az = hc.az;
		sight->coord.alt = hc.alt;

		return;
	}
	else if (hc.alt < -12.0)
	{
		// Too dark to see horizon, so no sight possible.
		return;
	}
	else if (hc.alt > -6.0)
	{
		// Sun is down, but it's still too bright to for stars, so no sight possible.
		return;
	}

	// If we're here, then the Sun is between 6 and 12 degrees below horizon (nautical twilight),
	// so we can shoot a star, which we choose randomly.
	int starAttempts = 0;
	while (starAttempts < 20)
	{
		const int star = (rand_r(&_randSeed) % PROTEUS_CELESTIAL_OBJ_Polaris) + 1;

		if (0 != proteus_Celestial_getEquatorialForObject(JD, star, &ec))
		{
			ERRLOG1("Failed to get equatorial coordinates for object %d!", star);
			return;
		}

		if (0 != proteus_Celestial_convertEquatorialToHorizontal(JD, pos, &ec, true, airPressure, airTemp, &hc))
		{
			ERRLOG1("Failed to convert coordinates for object %d!", star);
			return;
		}

		if (hc.alt < 0.0)
		{
			// Below horizon.
			starAttempts++;
			continue;
		}

		sight->obj = star;
		sight->coord.az = hc.az;
		sight->coord.alt = hc.alt;

		break;
	}
}


static bool isObscuredByCloudRandom(int cloudPercent)
{
	const int adjusted = (int)(sqrt((double)(cloudPercent * 100)));
	return ((rand_r(&_randSeed) % 100) + 1 <= adjusted);
}
