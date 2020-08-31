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

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <proteus/Compass.h>
#include <proteus/GeoInfo.h>
#include <proteus/Ocean.h>
#include <proteus/Weather.h>

#include "Boat.h"

#include "BoatWindResponse.h"


#define FORBIDDEN_LAT (0.0001)

#define MOVE_TO_WATER_DISTANCE (100)


static void updateCourse(Boat* b, double s);
static void updateVelocity(Boat* b, double s, const proteus_Weather* wx, bool odv, const proteus_OceanData* od);
static void updateDamage(Boat* b, double windGust, bool takeDamage);
static void stopBoat(Boat* b);
static double oceanIceSpeedAdjustmentFactor(bool valid, const proteus_OceanData* od);
static double boatDamageSpeedAdjustmentFactor(const Boat* b);

static unsigned int _randSeed = 0;


int Boat_init()
{
	_randSeed = time(0);
	return 0;
}

Boat* Boat_new(double lat, double lon, int boatType, int boatFlags)
{
	Boat* boat = (Boat*) malloc(sizeof(Boat));

	boat->pos.lat = lat;
	boat->pos.lon = lon;
	boat->v.angle = 0.0;
	boat->v.mag = 0.0;

	boat->desiredCourse = 0.0;
	boat->distanceTravelled = 0.0;
	boat->damage = 0.0;

	boat->boatType = boatType;
	boat->boatFlags = boatFlags;

	boat->stop = true;
	boat->sailsDown = false;
	boat->movingToSea = false;

	boat->setImmediateDesiredCourse = true;

	return boat;
}

void Boat_advance(Boat* b, double s)
{
	if (b->stop)
	{
		// Stopped, so nowhere to go.

		if (b->damage > 0.0)
		{
			// Possibly fix some boat damage.
			updateDamage(b, -1.0, false);
		}

		return;
	}

	if ((b->pos.lat >= 90.0 - FORBIDDEN_LAT) || (b->pos.lat <= -90.0 + FORBIDDEN_LAT))
	{
		// Very close to one of the poles, so stop in order to prevent weird things from happening.
		stopBoat(b);
		return;
	}

	if (b->movingToSea)
	{
		// Possibly on land, moving to sea.

		if (proteus_GeoInfo_isWater(&b->pos))
		{
			// We're on water, so proceed normally.
			b->movingToSea = false;

			if (b->setImmediateDesiredCourse)
			{
				// Probably the first time the boat is being started,
				// so set the course to the desired course immediately.
				b->v.angle = b->desiredCourse;
				b->setImmediateDesiredCourse = false;
			}
		}
		else
		{
			// Not on water, so check that there is water ahead of us.
			if (Boat_isHeadingTowardWater(b))
			{
				// Water ahead, so proceed at fixed speed toward it.
				b->v.angle = b->desiredCourse;
				b->v.mag = s * 0.5;

				proteus_GeoPos_advance(&b->pos, &b->v);
			}
			else
			{
				// No water ahead, so stop!
				stopBoat(b);
			}

			return;
		}
	}

	proteus_OceanData od;
	bool oceanDataValid = proteus_Ocean_get(&b->pos, &od);

	proteus_Weather wx;
	proteus_Weather_get(&b->pos, &wx, true);

	if (b->sailsDown)
	{
		// Sails down, so velocity vector over water is 1/10 of wind.
		proteus_GeoVec* windVec = &wx.wind;

		b->v.angle = windVec->angle + 180.0;
		if (b->v.angle >= 360.0)
		{
			b->v.angle -= 360.0;
		}

		// With sails down, we do not take any additional damage, but we can still repair it.
		updateDamage(b, wx.windGust, false);

		// NOTE: While sails are down, we intentionally do not take into account the boat damage speed adjustment factor.
		b->v.mag = windVec->mag * 0.1 * oceanIceSpeedAdjustmentFactor(oceanDataValid, &od);
	}
	else
	{
		// Update boat damage.
		updateDamage(b, wx.windGust, true);

		// Update course, if necessary.
		updateCourse(b, s);

		// Update boat velocity.
		updateVelocity(b, s, &wx, oceanDataValid, &od);
	}

	// Advance position.
	proteus_GeoVec v = b->v;
	v.mag *= s;
	proteus_GeoPos_advance(&b->pos, &v);

	// Add ocean currents (if applicable).
	if (oceanDataValid)
	{
		od.current.mag *= s;
		proteus_GeoPos_advance(&b->pos, &od.current);

		// Distance travelled increases by the magnitude of the vector sum
		// of the velocity over water and the ocean current.
		proteus_GeoVec_add(&od.current, &v);
		b->distanceTravelled += od.current.mag;
	}
	else
	{
		// Distance travelled increases by just the distance over water.
		b->distanceTravelled += fabs(v.mag);
	}

	// Check if we're still in water.
	if (!proteus_GeoInfo_isWater(&b->pos))
	{
		stopBoat(b);
	}
}

bool Boat_isHeadingTowardWater(Boat* b)
{
	int d = 0;

	proteus_GeoPos pos;
	memcpy(&pos, &b->pos, sizeof(proteus_GeoPos));

	proteus_GeoVec v;
	v.angle = b->desiredCourse;
	v.mag = 10.0;

	while (d <= MOVE_TO_WATER_DISTANCE + 10)
	{
		if (proteus_GeoInfo_isWater(&pos))
		{
			return true;
		}

		proteus_GeoPos_advance(&pos, &v);
		d += 10;
	}

	return false;
}


static void updateCourse(Boat* b, double s)
{
	const double courseDiff = proteus_Compass_diff(b->v.angle, b->desiredCourse);
	const double courseChangeRate = BoatWindResponse_getCourseChangeRate(b->boatType);

	if (fabs(courseDiff) <= courseChangeRate * s)
	{
		// Desired course is close enough to current course.
		b->v.angle = b->desiredCourse;
		return;
	}

	// Turn towards desired course.
	if (courseDiff < 0.0 && courseDiff >= -179.0)
	{
		// Turn left.
		b->v.angle -= (courseChangeRate * s);
	}
	else if (courseDiff > 0.0 && courseDiff <= 179.0)
	{
		// Turn right.
		b->v.angle += (courseChangeRate * s);
	}
	else
	{
		// Within a degree of being opposite where we want to go,
		// so choose a direction at random.
		if (rand_r(&_randSeed) % 2 == 0)
		{
			// Turn left.
			b->v.angle -= (courseChangeRate * s);
		}
		else
		{
			// Turn right.
			b->v.angle += (courseChangeRate * s);
		}
	}

	if (b->v.angle < 0.0)
	{
		b->v.angle += 360.0;
	}
	else if (b->v.angle >= 360.0)
	{
		b->v.angle -= 360.0;
	}
}

static void updateVelocity(Boat* b, double s, const proteus_Weather* wx, bool odv, const proteus_OceanData* od)
{
	const proteus_GeoVec* windVec = &wx->wind;

	const double angleFromWind = proteus_Compass_diff(windVec->angle, b->v.angle);

	const double spd = BoatWindResponse_getBoatSpeed(windVec->mag, angleFromWind, b->boatType) *
		oceanIceSpeedAdjustmentFactor(odv, od) *
		boatDamageSpeedAdjustmentFactor(b);

	const double speedChangeResponse = BoatWindResponse_getSpeedChangeResponse(b->boatType);

	b->v.mag = ((speedChangeResponse * b->v.mag) + (s * spd)) / (speedChangeResponse + s);
}

#define KNOTS_IN_M_PER_S (1.943844)

#define DAMAGE_INC_THRESH (45.0 / KNOTS_IN_M_PER_S)
#define DAMAGE_DEC_THRESH (25.0 / KNOTS_IN_M_PER_S)

#define DAMAGE_TAKE_FACTOR (0.25 * KNOTS_IN_M_PER_S * KNOTS_IN_M_PER_S / 3600.0) // 0.25% (to max damage) per hour per knot squared above threshold.
#define DAMAGE_REPAIR_FACTOR (0.25 * KNOTS_IN_M_PER_S / 3600.0) // 0.25% per hour per knot below threshold.

static void updateDamage(Boat* b, double windGust, bool takeDamage)
{
	if ((b->boatFlags & BOAT_FLAG_TAKES_DAMAGE) == 0)
	{
		return;
	}

	if (windGust < 0.0)
	{
		proteus_Weather wx;
		proteus_Weather_get(&b->pos, &wx, true);

		windGust = wx.windGust;
	}

	if (windGust < DAMAGE_DEC_THRESH)
	{
		if (b->damage > 0.0)
		{
			// Repair damage.
			b->damage -= ((DAMAGE_DEC_THRESH - windGust) * DAMAGE_REPAIR_FACTOR);
			if (b->damage < 0.0)
			{
				b->damage = 0.0;
			}
		}
	}
	else if (windGust > DAMAGE_INC_THRESH && takeDamage && b->damage < 100.0)
	{
		// Take damage.
		const double threshDiff = windGust - DAMAGE_INC_THRESH;

		b->damage += ((100.0 - b->damage) * (threshDiff * threshDiff * DAMAGE_TAKE_FACTOR * 0.01));
		if (b->damage > 100.0)
		{
			b->damage = 100.0;
		}
	}
}

static void stopBoat(Boat* b)
{
	b->stop = true;
	b->v.mag = 0.0;
}

static double oceanIceSpeedAdjustmentFactor(bool valid, const proteus_OceanData* od)
{
	return (valid ? (1.0 - (od->ice / 100.0f)) : 1.0);
}

static double boatDamageSpeedAdjustmentFactor(const Boat* b)
{
	return ((b->boatFlags & BOAT_FLAG_TAKES_DAMAGE) ? (1.0 - b->damage * 0.01) : 1.0);
}
