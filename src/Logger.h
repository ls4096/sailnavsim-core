/**
 * Copyright (C) 2020-2023 ls4096 <ls4096@8bitbyte.ca>
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

#ifndef _Logger_h_
#define _Logger_h_

#include <stdbool.h>

#include <proteus/GeoPos.h>
#include <proteus/GeoVec.h>
#include <proteus/Wave.h>
#include <proteus/Weather.h>
#include <proteus/Ocean.h>

#include "Boat.h"

typedef struct
{
	// Time
	time_t time;

	// Boat name
	char* boatName;

	// Boat position
	proteus_GeoPos boatPos;

	// Boat velocity (through water)
	proteus_GeoVec boatVecWater;

	// Boat velocity (over ground)
	proteus_GeoVec boatVecGround;

	// Compass magnetic declination
	double compassMagDec;

	// Boat distance travelled
	double distanceTravelled;

	// Boat damage
	double damage;

	// Weather data (including wind)
	proteus_Weather wx;
	double windGustAngle;

	// Ocean data
	proteus_OceanData oceanData;
	bool oceanDataValid;

	// Wave data
	proteus_WaveData waveData;
	bool waveDataValid;

	// Boat status (0: stopped; 1: moving - sailing; 2: moving - sails down)
	unsigned char boatState;

	// Boat location state (0: water; 1: landed)
	unsigned char locState;

	// Whether or not log entry is "visible" (applies to celestial navigation mode)
	bool reportVisible;
} LogEntry;

typedef struct
{
	// Time
	time_t time;

	// Boat name
	const char* boatName;

	// Celestial object ID (see proteus/Celestial.h)
	int obj;

	// Object sighted azimuth
	double az;

	// Object sighted altitude
	double alt;

	// Compass magnetic declination
	double compassMagDec;
} CelestialSightEntry;

int Logger_init(const char* csvLoggerDir, const char* sqliteDbFilename);
void Logger_fillLogEntry(Boat* boat, const char* name, time_t t, bool reportVisible, LogEntry* log);
void Logger_writeLogs(LogEntry* logEntries, unsigned int lCount, CelestialSightEntry* csEntries, unsigned int csCount);

#endif // _Logger_h_
