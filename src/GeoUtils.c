/**
 * Copyright (C) 2020-2022 ls4096 <ls4096@8bitbyte.ca>
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

#include <proteus/GeoInfo.h>
#include <proteus/ScalarConv.h>

#include "GeoUtils.h"


static bool isLandFoundOnCircle(const proteus_GeoPos* pos, double r, int n);


// Samples points inside a circle of the visibility radius for detecting nearby land.
#define MIN_RADIUS (30.0)
#define MAX_RADIUS (31000.0)
#define MAX_SAMPLE_POINTS_ON_CIRCLE (32)
bool GeoUtils_isApproximatelyNearVisibleLand(const proteus_GeoPos* pos, float visibility)
{
	if (!proteus_GeoInfo_isWater(pos))
	{
		return true;
	}

	int n = 4;
	for (double r = MIN_RADIUS; r <= visibility && r <= MAX_RADIUS; r *= 2.0)
	{
		if (isLandFoundOnCircle(pos, r, n))
		{
			return true;
		}

		if (n < MAX_SAMPLE_POINTS_ON_CIRCLE)
		{
			n *= 2;
		}
	}

	if (visibility > MIN_RADIUS)
	{
		// Check one last circle at the outer limits of visibility.
		if (isLandFoundOnCircle(pos, visibility, n))
		{
			return true;
		}
	}

	return false;
}


// Calculations to "look around" approximately uniformly (at "n" points) around an approximate circle (of somewhat-radius "r" metres) from the given position ("pos").
static bool isLandFoundOnCircle(const proteus_GeoPos* pos, double r, int n)
{
	proteus_GeoPos p;

	const double cosLat = cos(proteus_ScalarConv_deg2rad(pos->lat));

	for (int i = 0; i < n; i++)
	{
		// We could make this more exact, but a close-enough approximation suffices here and will run faster...

		p.lat = pos->lat +
			(r * cos(i * 2.0 * M_PI / n) / 111120.0);

		p.lon = pos->lon +
			(r * sin(i * 2.0 * M_PI / n) / (111120.0 * cosLat));

		if (p.lat > 90.0)
		{
			p.lat = 90.0;
		}
		else if (p.lat < -90.0)
		{
			p.lat = -90.0;
		}

		bool lonModified = false;
		if (p.lon >= 180.0)
		{
			p.lon -= 360.0;
			lonModified = true;
		}
		else if (p.lon < -180.0)
		{
			p.lon += 360.0;
			lonModified = true;
		}

		// Where our latitude is close to -90 or +90, the calculated longitude value may be very strange,
		// so, if we modified the longitude above, we do one more check just to be sure...
		if (lonModified && (p.lon < -180.0 || p.lon >= 180.0))
		{
			// Longitude still out of bounds!
			if (p.lat >= 0)
			{
				// Northern hemisphere very near the pole, so it's all water.
				return false;
			}
			else
			{
				// Southern hemisphere very near the pole, so it's all land.
				return true;
			}
		}

		if (!proteus_GeoInfo_isWater(&p))
		{
			return true;
		}
	}

	return false;
}
