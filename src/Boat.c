/**
 * Copyright (C) 2020-2024 ls4096 <ls4096@8bitbyte.ca>
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

#include <sailnavsim_advancedboats.h>

#include "Boat.h"

#include "BoatWindResponse.h"
#include "WxUtils.h"


#define FORBIDDEN_LAT (0.0001)
#define MOVE_TO_WATER_DISTANCE (100)
#define STARTING_FROM_LAND_COUNTDOWN (10)


static void updateCourse(Boat* b, time_t curTime);
static void updateVelocity(Boat* b, const proteus_Weather* wx, bool odv, const proteus_OceanData* od, bool wdv, const proteus_WaveData* wd);
static void updateDamage(Boat* b, double windGust, double windAngle, bool takeDamage);
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

	boat->startingFromLandCount = 0;

	boat->stop = true;
	boat->sailsDown = false;
	boat->movingToSea = false;

	boat->setImmediateDesiredCourse = true;
	boat->courseMagnetic = (boatFlags & BOAT_FLAG_CELESTIAL); // Default magnetic for celestial navigation mode.

	boat->sailArea = 0.0;
	boat->leewaySpeed = 0.0;
	boat->heelingAngle = 0.0;

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
			updateDamage(b, -1.0 /* indicates stopped boat */, 0.0, false);
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
				b->leewaySpeed = 0.0;

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

	proteus_Weather wx;
	proteus_Weather_get(&b->pos, &wx, true);

	proteus_OceanData od;
	const bool oceanDataValid = proteus_Ocean_get(&b->pos, &od);

	if (oceanDataValid)
	{
		WxUtils_adjustWindForCurrent(&wx, &od.current);
	}

	proteus_WaveData wd;
	const bool waveDataValid = proteus_Wave_get(&b->pos, &wd);

	const bool advancedBoatType = BoatWindResponse_isBoatTypeAdvanced(b->boatType);

	if (!advancedBoatType && b->sailsDown)
	{
		// Sails down for basic/non-advanced boat types

		// Velocity vector over water is 1/10 of wind.
		const proteus_GeoVec* windVec = &wx.wind;

		b->v.angle = windVec->angle + 180.0;
		if (b->v.angle >= 360.0)
		{
			b->v.angle -= 360.0;
		}

		// With sails down, we do not take any additional damage, but we can still repair it.
		updateDamage(b, wx.windGust, windVec->angle, false);

		// NOTE: While sails are down, we intentionally do not take into account the boat damage speed adjustment factor.
		b->v.mag = windVec->mag * 0.1 *
			oceanIceSpeedAdjustmentFactor(oceanDataValid, &od) *
			waveSpeedAdjustmentFactor(b, waveDataValid, &wd);
	}
	else
	{
		// Update boat damage.
		const bool takeDamage = (!advancedBoatType || b->sailArea > 0.0); // For advanced boat types, only take additional damage if some sail is up.
		updateDamage(b, wx.windGust, wx.wind.angle, takeDamage);

		// Update course, if necessary.
		updateCourse(b, curTime);

		// Update boat velocity.
		updateVelocity(b, &wx, oceanDataValid, &od, waveDataValid, &wd);
	}

	// Compute "over ground" vector, based on leeway and ocean currents (if available).
	b->vGround = b->v;

	if (oceanDataValid)
	{
		// Ocean data is valid, so add ocean current vector to "over ground" vector.

		if (b->startingFromLandCount > 0)
		{
			// Boat has recently started from land, so diminish the effects of the current.
			const double currentFactor = ((double)(STARTING_FROM_LAND_COUNTDOWN - b->startingFromLandCount)) / ((double) STARTING_FROM_LAND_COUNTDOWN);
			od.current.mag *= currentFactor;
		}

		proteus_GeoVec_add(&b->vGround, &od.current);
	}

	if (b->leewaySpeed != 0.0)
	{
		// There is non-zero leeway, so take this into account.
		proteus_GeoVec leewayVec = {
			.angle = b->v.angle + 90.0,
			.mag = b->leewaySpeed
		};

		if (leewayVec.angle >= 360.0)
		{
			leewayVec.angle -= 360.0;
		}

		proteus_GeoVec_add(&b->vGround, &leewayVec);
	}

	// Ensure that the "over ground" vector has positive magnitude.
	if (b->vGround.mag < 0.0)
	{
		b->vGround.mag = -b->vGround.mag;

		// Negating the magnitude (to make it positive) means we need to flip the angle 180 degrees.
		b->vGround.angle += 180.0;
		if (b->vGround.angle >= 360.0)
		{
			b->vGround.angle -= 360.0;
		}
	}

	if (b->startingFromLandCount > 0)
	{
		b->startingFromLandCount--;
	}

	// Advance boat by "over ground" vector.
	proteus_GeoPos_advance(&b->pos, &b->vGround);

	// Accumulate distance travelled.
	b->distanceTravelled += b->vGround.mag;

	// Finally, check if we're still in water.
	if (!proteus_GeoInfo_isWater(&b->pos))
	{
		// We're on land, so stop the boat and reset the land countdown value.
		stopBoat(b);
		b->startingFromLandCount = STARTING_FROM_LAND_COUNTDOWN;
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

	double speedAdjustmentFactor =
			oceanIceSpeedAdjustmentFactor(odv, od) *
			waveSpeedAdjustmentFactor(b, wdv, wd);

	if (BoatWindResponse_isBoatTypeAdvanced(b->boatType))
	{
		// Advanced boat type

		// NOTE: While sails are down, we intentionally do not take into account the boat damage speed adjustment factor.
		if (b->sailArea > 0.0)
		{
			speedAdjustmentFactor *= boatDamageSpeedAdjustmentFactor(b);
		}

		// Since the boat will likely already be going slower from the past iterations due to the "speed adjustment factor" here,
		// we want to ensure the calculations are done on "normal" values without this factor applied, so we divide by it below,
		// then we multiply back in the SAF with the resultant values after performing the advancedboats calculations below.
		// We also avoid dividing by values close to and equal to zero with the modification below.
		const double safModified = ((speedAdjustmentFactor < 0.01) ? 0.01 : speedAdjustmentFactor);

		const AdvancedBoatInputData inputData = {
			.wind_angle = -angleFromWind,
			.wind_speed = windVec->mag,
			.boat_speed_ahead = (b->v.mag / safModified),
			.boat_speed_abeam = (b->leewaySpeed / safModified),
			.sail_area = b->sailArea
		};

		AdvancedBoatOutputData outputData;

		const int32_t rc = sailnavsim_advancedboats_boat_update_v(BoatWindResponse_adjustBoatTypeForAdvanced(b->boatType), &inputData, &outputData);
		if (0 == rc)
		{
			b->v.mag = outputData.boat_speed_ahead * safModified;
			b->leewaySpeed = outputData.boat_speed_abeam * safModified;
			b->heelingAngle = outputData.heeling_angle;
		}
		else
		{
			// Error (shouldn't happen), so to stay sane just set boat's v.mag (speed ahead) and leeway to zero.
			b->v.mag = 0.0;
			b->leewaySpeed = 0.0;
			b->heelingAngle = 0.0;
		}
	}
	else
	{
		// Basic boat type

		const double spd = BoatWindResponse_getBoatSpeed(windVec->mag, angleFromWind, b->boatType) * speedAdjustmentFactor * boatDamageSpeedAdjustmentFactor(b);
		const double speedChangeResponse = BoatWindResponse_getSpeedChangeResponse(b->boatType);

		b->v.mag = ((speedChangeResponse * b->v.mag) + spd) / (speedChangeResponse + 1.0);
	}
}

#define KTS_IN_MPS (1.943844)

#define DAMAGE_DECREASE_THRESHOLD (25.0 / KTS_IN_MPS)

#define DAMAGE_TAKE_FACTOR (0.25 * KTS_IN_MPS * KTS_IN_MPS / 3600.0) // 0.25% (to max damage) per hour per knot squared above threshold.
#define DAMAGE_REPAIR_FACTOR (0.25 * KTS_IN_MPS / 3600.0) // 0.25% per hour per knot below threshold.

static void updateDamage(Boat* b, double windGust, double windAngle, bool takeDamage)
{
	if ((b->boatFlags & BOAT_FLAG_TAKES_DAMAGE) == 0)
	{
		return;
	}

	// Caller providing windGust < 0.0 indicates "stopped" boat.
	if (windGust < 0.0)
	{
		proteus_Weather wx;
		proteus_Weather_get(&b->pos, &wx, true);

		// NOTE: No need to adjust wind for ocean currents here
		//       since windGust < 0.0 indicates "stopped" boat.

		windGust = wx.windGust;
		windAngle = wx.wind.angle;
	}

	if ((b->boatFlags & BOAT_FLAG_DAMAGE_APPARENT_WIND))
	{

		// Use apparent wind instead of true wind for damage calculations.
		proteus_GeoVec appWindGust = { .angle = windAngle, .mag = windGust };
		proteus_GeoVec_add(&appWindGust, &b->v);

		if (b->leewaySpeed != 0.0)
		{
			// There is non-zero leeway, so take this into account.
			proteus_GeoVec leewayVec = {
				.angle = b->v.angle + 90.0,
				.mag = b->leewaySpeed
			};

			if (leewayVec.angle >= 360.0)
			{
				leewayVec.angle -= 360.0;
			}

			proteus_GeoVec_add(&appWindGust, &leewayVec);
		}

		windGust = appWindGust.mag;
	}

	const double damageTakeThreshold = BoatWindResponse_getDamageWindGustThreshold(b->boatType);

	if (windGust < DAMAGE_DECREASE_THRESHOLD)
	{
		if (b->damage > 0.0)
		{
			// Repair damage.
			b->damage -= ((DAMAGE_DECREASE_THRESHOLD - windGust) * DAMAGE_REPAIR_FACTOR);
			if (b->damage < 0.0)
			{
				b->damage = 0.0;
			}
		}
	}
	else if (windGust > damageTakeThreshold && takeDamage && b->damage < 100.0)
	{
		// Take damage.
		const double threshDiff = windGust - damageTakeThreshold;

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
	b->leewaySpeed = 0.0;
	b->heelingAngle = 0.0;

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
