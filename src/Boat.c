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

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <proteus/Compass.h>
#include <proteus/GeoInfo.h>
#include <proteus/Ocean.h>
#include <proteus/Wave.h>
#include <proteus/Weather.h>

#include "Boat.h"

#include "BoatWindResponse.h"


#define FORBIDDEN_LAT (0.0001)

#define MOVE_TO_WATER_DISTANCE (100)


static void updateCourse(Boat* b, time_t curTime);
static void updateVelocity(Boat* b, const proteus_Weather* wx, bool odv, const proteus_OceanData* od, bool wdv, const proteus_WaveData* wd);
static void updateDamage(Boat* b, double windGust, bool takeDamage);
static void stopBoat(Boat* b);
static double getDesiredCourseTrue(const Boat* b, time_t t);
static double convertMag2True(const proteus_GeoPos* pos, time_t t, double compassMag);
static double oceanIceSpeedAdjustmentFactor(bool valid, const proteus_OceanData* od);
static double boatDamageSpeedAdjustmentFactor(const Boat* b);
static double waveSpeedAdjustmentFactor(const Boat* b, bool valid, const proteus_WaveData* wd);
static double getRandDouble(double scale);

static unsigned int _randSeed = 0;


int Boat_init()
{
	_randSeed = time(0);
	return 0;
}

Boat* Boat_new(double lat, double lon, int boatType, int boatFlags)
{
	Boat* boat = malloc(sizeof(Boat));
	if (!boat)
	{
		return 0;
	}

	boat->pos.lat = lat;
	boat->pos.lon = (lon < 180.0 ? lon : lon - 360.0);
	boat->v.angle = 0.0;
	boat->v.mag = 0.0;
	boat->vGround.angle = 0.0;
	boat->vGround.mag = 0.0;

	boat->desiredCourse = 0.0;
	boat->distanceTravelled = 0.0;
	boat->damage = 0.0;

	boat->boatType = boatType;
	boat->boatFlags = boatFlags;

	boat->stop = true;
	boat->sailsDown = false;
	boat->movingToSea = false;

	boat->setImmediateDesiredCourse = true;
	boat->courseMagnetic = (boatFlags & BOAT_FLAG_CELESTIAL); // Default magnetic for celestial navigation mode.

	return boat;
}

void Boat_advance(Boat* b, time_t curTime)
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
				b->v.angle = getDesiredCourseTrue(b, curTime);
				b->setImmediateDesiredCourse = false;
			}
		}
		else
		{
			// Not on water, so check that there is water ahead of us.
			if (Boat_isHeadingTowardWater(b, curTime))
			{
				// Water ahead, so proceed at fixed speed toward it.
				b->v.angle = getDesiredCourseTrue(b, curTime);
				b->v.mag = 0.5;

				b->vGround = b->v;

				proteus_GeoPos_advance(&b->pos, &b->vGround);
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
	const bool oceanDataValid = proteus_Ocean_get(&b->pos, &od);

	proteus_WaveData wd;
	const bool waveDataValid = proteus_Wave_get(&b->pos, &wd);

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
		b->v.mag = windVec->mag * 0.1 *
			oceanIceSpeedAdjustmentFactor(oceanDataValid, &od) *
			waveSpeedAdjustmentFactor(b, waveDataValid, &wd);
	}
	else
	{
		// Update boat damage.
		updateDamage(b, wx.windGust, true);

		// Update course, if necessary.
		updateCourse(b, curTime);

		// Update boat velocity.
		updateVelocity(b, &wx, oceanDataValid, &od, waveDataValid, &wd);
	}

	// Compute "over ground" vector, based on ocean currents (if available).
	b->vGround = b->v;

	if (oceanDataValid)
	{
		// Ocean data is valid, so add ocean current vector to "over ground" vector.
		proteus_GeoVec_add(&b->vGround, &od.current);
	}
	else if (b->vGround.mag < 0.0)
	{
		// Ocean data is not valid, but we still need to ensure that the "over ground" vector has positive magnitude.
		b->vGround.mag = -b->vGround.mag;

		// Negating the magnitude (to make it positive) means we need to flip the angle 180 degrees.
		b->vGround.angle += 180.0;
		if (b->vGround.angle >= 360.0)
		{
			b->vGround.angle -= 360.0;
		}
	}

	// Advance boat by "over ground" vector.
	proteus_GeoPos_advance(&b->pos, &b->vGround);

	// Accumulate distance travelled.
	b->distanceTravelled += b->vGround.mag;

	// Finally, check if we're still in water.
	if (!proteus_GeoInfo_isWater(&b->pos))
	{
		stopBoat(b);
	}
}

bool Boat_isHeadingTowardWater(const Boat* b, time_t curTime)
{
	int d = 0;

	proteus_GeoPos pos = b->pos;

	proteus_GeoVec v;
	v.angle = getDesiredCourseTrue(b, curTime);
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

bool Boat_getWaveAdjustedCelestialAzAlt(const Boat* b, double* az, double* alt)
{
	if (!(b->boatFlags & BOAT_FLAG_CELESTIAL_WAVE_EFFECT))
	{
		// Boat flag for celestial wave effect is not set, so no adjustments to be made.
		return true;
	}

	proteus_WaveData wd;
	const bool waveDataValid = proteus_Wave_get(&b->pos, &wd);

	if (!waveDataValid)
	{
		// No wave data available, so no adjustments to be made.
		return true;
	}

	const double wh = wd.waveHeight;
	const double wer = BoatWindResponse_getWaveEffectResistance(b->boatType);

	double newAlt = *alt + (1.666667 * getRandDouble(wh) * getRandDouble(wh) / wer);
	if (newAlt < 0.0)
	{
		// Adjusted altitude is below horizon.
		return false;
	}
	else if (newAlt > 90.0)
	{
		newAlt = 90.0 - (newAlt - 90.0);
	}

	double newAz = *az + (100.0 * getRandDouble(wh) * getRandDouble(wh) / wer);
	while (newAz < 0.0)
	{
		newAz += 360.0;
	}
	while (newAz >= 360.0)
	{
		newAz -= 360.0;
	}

	*alt = newAlt;
	*az = newAz;

	return true;
}


static void updateCourse(Boat* b, time_t curTime)
{
	const double desiredCourseTrue = getDesiredCourseTrue(b, curTime);
	const double courseDiff = proteus_Compass_diff(b->v.angle, desiredCourseTrue);
	const double courseChangeRate = BoatWindResponse_getCourseChangeRate(b->boatType);

	if (fabs(courseDiff) <= courseChangeRate)
	{
		// Desired course is close enough to current course.
		b->v.angle = desiredCourseTrue;
		return;
	}

	// Turn towards desired course.
	if (courseDiff < 0.0 && courseDiff >= -179.0)
	{
		// Turn left.
		b->v.angle -= courseChangeRate;
	}
	else if (courseDiff > 0.0 && courseDiff <= 179.0)
	{
		// Turn right.
		b->v.angle += courseChangeRate;
	}
	else
	{
		// Within a degree of being opposite where we want to go,
		// so choose a direction at random.
		if (rand_r(&_randSeed) % 2 == 0)
		{
			// Turn left.
			b->v.angle -= courseChangeRate;
		}
		else
		{
			// Turn right.
			b->v.angle += courseChangeRate;
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

static void updateVelocity(Boat* b, const proteus_Weather* wx, bool odv, const proteus_OceanData* od, bool wdv, const proteus_WaveData* wd)
{
	const proteus_GeoVec* windVec = &wx->wind;

	const double angleFromWind = proteus_Compass_diff(windVec->angle, b->v.angle);

	const double spd = BoatWindResponse_getBoatSpeed(windVec->mag, angleFromWind, b->boatType) *
		oceanIceSpeedAdjustmentFactor(odv, od) *
		boatDamageSpeedAdjustmentFactor(b) *
		waveSpeedAdjustmentFactor(b, wdv, wd);

	const double speedChangeResponse = BoatWindResponse_getSpeedChangeResponse(b->boatType);

	b->v.mag = ((speedChangeResponse * b->v.mag) + spd) / (speedChangeResponse + 1.0);
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

	b->vGround = b->v;

	// FIXME: Should probably also set Boat.started to 0 in the database (if we're using it).
}

static double getDesiredCourseTrue(const Boat* b, time_t t)
{
	if (b->courseMagnetic)
	{
		return convertMag2True(&b->pos, t, b->desiredCourse);
	}
	else
	{
		return b->desiredCourse;
	}
}

static double convertMag2True(const proteus_GeoPos* pos, time_t t, double compassMag)
{
	const double magDec = proteus_Compass_magdec(pos, t);

	double compassTrue = compassMag + magDec;
	if (compassTrue < 0.0)
	{
		compassTrue += 360.0;
	}
	else if (compassTrue > 360.0)
	{
		compassTrue -= 360.0;
	}

	return compassTrue;
}

static double oceanIceSpeedAdjustmentFactor(bool valid, const proteus_OceanData* od)
{
	if (valid)
	{
		return (1.0 - (od->ice / 100.0f));
	}

	return 1.0;
}

static double boatDamageSpeedAdjustmentFactor(const Boat* b)
{
	if (b->boatFlags & BOAT_FLAG_TAKES_DAMAGE)
	{
		return (1.0 - (b->damage * 0.01));
	}

	return 1.0;
}

static double waveSpeedAdjustmentFactor(const Boat* b, bool valid, const proteus_WaveData* wd)
{
	if ((b->boatFlags & BOAT_FLAG_WAVE_SPEED_EFFECT) && valid)
	{
		return (1.0 / exp(wd->waveHeight * wd->waveHeight / BoatWindResponse_getWaveEffectResistance(b->boatType)));
	}

	return 1.0;
}

static double getRandDouble(double scale)
{
	return ((double) ((rand_r(&_randSeed) % 257) - 128)) / 128.0 * scale;
}
