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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sqlite3.h>

#include "BoatInitParser.h"
#include "BoatWindResponse.h"
#include "ErrLog.h"


#define ERRLOG_ID "BoatInitParser"


static int startSql(const char* sqliteDbFilename);
static BoatInitEntry* getNextSql();

static sqlite3* _sql = 0;
static sqlite3_stmt* _sqlStmtBoat;
static sqlite3_stmt* _sqlStmtBoatLog;


static int startFile(const char* boatInitFilename);
static BoatInitEntry* getNextFile();
static int readBoatInitData(char* s, char** name, double* lat, double* lon, int* type, int* flags);

static FILE* _fp;


int BoatInitParser_start(const char* boatInitFilename, const char* sqliteDbFilename)
{
	int rc = startSql(sqliteDbFilename);
	if (rc <= 0)
	{
		return rc;
	}
	else
	{
		rc = startFile(boatInitFilename);
		return rc;
	}
}

BoatInitEntry* BoatInitParser_getNext()
{
	if (_sql)
	{
		return getNextSql();
	}
	else if (_fp)
	{
		return getNextFile();
	}

	return 0;
}


static int startSql(const char* sqliteDbFilename)
{
	if (!sqliteDbFilename)
	{
		return 1;
	}

	FILE* fdb = fopen(sqliteDbFilename, "r");
	if (fdb == 0)
	{
		if (errno == ENOENT)
		{
			ERRLOG("No SQLite DB file found. Not reading boat init data from there.");
			return 1;
		}
		else
		{
			ERRLOG("SQLite DB file error!");
			return -1;
		}
	}
	else
	{
		fclose(fdb);
	}

	static const char* SELECT_BOAT_STMT_STR = "SELECT name, race, desiredCourse, started, boatType, boatFlags, friendlyName, sailArea FROM Boat WHERE isActive = 1;";
	static const char* SELECT_BOATLOG_STMT_STR = "SELECT lat, lon, courseWater, speedWater, boatStatus, boatLocation, distanceTravelled, damage, leewaySpeed, heelingAngle FROM BoatLog WHERE boatName=? ORDER BY time DESC LIMIT 1;";

	int src;

	if (SQLITE_OK != (src = sqlite3_open(sqliteDbFilename, &_sql)))
	{
		ERRLOG1("Failed to open SQLite DB. sqlite rc=%d", src);
		return -1;
	}

	if (SQLITE_OK != (src = sqlite3_prepare_v2(_sql, SELECT_BOAT_STMT_STR, strlen(SELECT_BOAT_STMT_STR) + 1, &_sqlStmtBoat, 0)))
	{
		ERRLOG1("Failed to prepare Boat select statement. sqlite rc=%d", src);
		return -1;
	}

	if (SQLITE_OK != (src = sqlite3_reset(_sqlStmtBoat)))
	{
		ERRLOG1("Failed to reset Boat select statement. sqlite rc=%d", src);
		return -1;
	}

	if (SQLITE_OK != (src = sqlite3_prepare_v2(_sql, SELECT_BOATLOG_STMT_STR, strlen(SELECT_BOATLOG_STMT_STR) + 1, &_sqlStmtBoatLog, 0)))
	{
		ERRLOG1("Failed to prepare BoatLog select statement. sqlite rc=%d", src);
		return -1;
	}

	if (SQLITE_OK != (src = sqlite3_reset(_sqlStmtBoatLog)))
	{
		ERRLOG1("Failed to reset BoatLog select statement. sqlite rc=%d", src);
		return -1;
	}

	return 0;
}

static BoatInitEntry* getNextSql()
{
	for (;;)
	{
		int src = sqlite3_step(_sqlStmtBoat);
		if (src == SQLITE_DONE)
		{
			if (SQLITE_OK != (src = sqlite3_finalize(_sqlStmtBoat)))
			{
				ERRLOG1("Failed to finalize Boat statement! sqlite rc=%d", src);
			}
			_sqlStmtBoat = 0;

			if (SQLITE_OK != (src = sqlite3_finalize(_sqlStmtBoatLog)))
			{
				ERRLOG1("Failed to finalize BoatLog statement! sqlite rc=%d", src);
			}
			_sqlStmtBoatLog = 0;

			if (SQLITE_OK != (src = sqlite3_close(_sql)))
			{
				ERRLOG1("Failed to close SQLite DB! sqlite rc=%d", src);
			}
			_sql = 0;

			return 0;
		}
		else if (src == SQLITE_ROW)
		{
			int n = 0;

			const char* boatName = (const char*) sqlite3_column_text(_sqlStmtBoat, n++);
			const char* race = (const char*) sqlite3_column_text(_sqlStmtBoat, n++);
			const double desiredCourse = sqlite3_column_double(_sqlStmtBoat, n++);
			const int started = sqlite3_column_int(_sqlStmtBoat, n++);
			const int boatType = sqlite3_column_int(_sqlStmtBoat, n++);
			const int boatFlags = sqlite3_column_int(_sqlStmtBoat, n++);
			const char* boatFriendlyName = (const char*) sqlite3_column_text(_sqlStmtBoat, n++);
			const double sailArea = sqlite3_column_double(_sqlStmtBoat, n++);

			src = sqlite3_reset(_sqlStmtBoatLog);
			if (src != SQLITE_OK)
			{
				ERRLOG1("Failed to reset BoatLog statement! sqlite rc=%d", src);
				continue;
			}

			src = sqlite3_bind_text(_sqlStmtBoatLog, 1, boatName, -1, 0);
			if (src != SQLITE_OK)
			{
				ERRLOG1("Failed to bind boat name to BoatLog statement! sqlite rc=%d", src);
				continue;
			}

			src = sqlite3_step(_sqlStmtBoatLog);
			if (src == SQLITE_ROW)
			{
				int n = 0;

				const double lat = sqlite3_column_double(_sqlStmtBoatLog, n++);
				const double lon = sqlite3_column_double(_sqlStmtBoatLog, n++);
				const double course = sqlite3_column_double(_sqlStmtBoatLog, n++);
				const double speed = sqlite3_column_double(_sqlStmtBoatLog, n++);
				const int boatStatus = sqlite3_column_int(_sqlStmtBoatLog, n++);
				const int boatLocation = sqlite3_column_int(_sqlStmtBoatLog, n++);
				const double distanceTravelled = sqlite3_column_double(_sqlStmtBoatLog, n++);
				const double damage = sqlite3_column_double(_sqlStmtBoatLog, n++);
				const double leewaySpeed = sqlite3_column_double(_sqlStmtBoatLog, n++);
				const double heelingAngle = sqlite3_column_double(_sqlStmtBoatLog, n++);

				BoatInitEntry* entry = malloc(sizeof(BoatInitEntry));
				if (!entry)
				{
					ERRLOG("Failed to alloc BoatInitEntry!");
					continue;
				}

				Boat* boat = Boat_new(lat, lon, boatType, boatFlags);
				if (!boat)
				{
					ERRLOG("Failed to create new Boat!");
					free(entry);
					entry = 0;
					continue;
				}

				boat->v.angle = course;
				boat->v.mag = speed;
				boat->desiredCourse = desiredCourse;
				boat->distanceTravelled = distanceTravelled;
				boat->damage = damage;
				boat->stop = (boatStatus == 0 && started == 0);
				boat->sailsDown = (BoatWindResponse_isBoatTypeBasic(boatType) && boatLocation == 0 && started == 0);
				boat->movingToSea = (boatLocation == 1 && started == 1);
				boat->sailArea = sailArea;
				boat->leewaySpeed = leewaySpeed;
				boat->heelingAngle = heelingAngle;

				if (boat->stop)
				{
					boat->v.mag = 0.0;
				}

				entry->boat = boat;

				entry->name = strdup(boatName);
				if (!entry->name)
				{
					ERRLOG("Failed to alloc entry->name!");

					free(entry);
					entry = 0;
					free(boat);
					boat = 0;

					continue;
				}

				entry->group = strdup(race);
				if (!entry->group)
				{
					ERRLOG("Failed to alloc entry->group!");

					free(entry->name);
					entry->name = 0;
					free(entry);
					entry = 0;
					free(boat);
					boat = 0;

					continue;
				}

				entry->boatAltName = strdup(boatFriendlyName);
				if (!entry->boatAltName)
				{
					ERRLOG("Failed to alloc entry->boatAltName!");

					free(entry->group);
					entry->group = 0;
					free(entry->name);
					entry->name = 0;
					free(entry);
					entry = 0;
					free(boat);
					boat = 0;

					continue;
				}

				return entry;
			}
			else if (src == SQLITE_DONE)
			{
				// Boat that exists in the Boat table but has nothing logged yet, so assume it was newly added.

				BoatInitEntry* entry = 0;

				sqlite3_stmt* stmt;
				static const char* SELECT_FROM_BOATRACE_STMT_STR = "SELECT startLat, startLon FROM BoatRace WHERE name=?;";

				src = sqlite3_prepare_v2(_sql, SELECT_FROM_BOATRACE_STMT_STR, strlen(SELECT_FROM_BOATRACE_STMT_STR) + 1, &stmt, 0);
				if (SQLITE_OK != src)
				{
					ERRLOG1("Failed to prepare statement! sqlite rc=%d", src);
					continue;
				}

				src = sqlite3_bind_text(stmt, 1, race, -1, 0);
				if (SQLITE_OK != src)
				{
					ERRLOG1("Failed to bind race value to statement! sqlite rc=%d", src);
					goto cleanup;
				}

				src = sqlite3_step(stmt);
				if (SQLITE_ROW != src)
				{
					ERRLOG1("Did not find race! sqlite rc=%d", src);
					goto cleanup;
				}

				const double lat = sqlite3_column_double(stmt, 0);
				const double lon = sqlite3_column_double(stmt, 1);

				entry = malloc(sizeof(BoatInitEntry));
				if (!entry)
				{
					ERRLOG("Failed to alloc BoatInitEntry!");
					goto cleanup;
				}

				Boat* boat = Boat_new(lat, lon, boatType, boatFlags);
				if (!boat)
				{
					ERRLOG("Failed to create new Boat!");
					free(entry);
					entry = 0;
					goto cleanup;
				}

				entry->boat = boat;

				entry->name = strdup(boatName);
				if (!entry->name)
				{
					ERRLOG("Failed to alloc entry->name!");

					free(entry);
					entry = 0;
					free(boat);
					boat = 0;

					goto cleanup;
				}

				entry->group = strdup(race);
				if (!entry->group)
				{
					ERRLOG("Failed to alloc entry->group!");

					free(entry->name);
					entry->name = 0;
					free(entry);
					entry = 0;
					free(boat);
					boat = 0;

					goto cleanup;
				}

				entry->boatAltName = strdup(boatFriendlyName);
				if (!entry->boatAltName)
				{
					ERRLOG("Failed to alloc entry->boatAltName!");

					free(entry->group);
					entry->group = 0;
					free(entry->name);
					entry->name = 0;
					free(entry);
					entry = 0;
					free(boat);
					boat = 0;

					goto cleanup;
				}

cleanup:
				src = sqlite3_finalize(stmt);
				if (SQLITE_OK != src)
				{
					ERRLOG1("Failed to finalize statement! sqlite rc=%d", src);
					continue;
				}

				if (entry)
				{
					return entry;
				}
			}
		}
	}
}

static int startFile(const char* boatInitFilename)
{
	if (!boatInitFilename)
	{
		return 1;
	}

	_fp = fopen(boatInitFilename, "r");

	if (_fp == 0)
	{
		if (errno == ENOENT)
		{
			return 1;
		}
		else
		{
			return -1;
		}
	}

	return 0;
}

#define BOAT_INIT_ENTRY_BUF_SIZE (1024)

static BoatInitEntry* getNextFile()
{
	if (_fp == 0)
	{
		return 0;
	}

	char buf[BOAT_INIT_ENTRY_BUF_SIZE];

	char* name;
	double lat, lon;
	int type, flags;

	if (fgets(buf, BOAT_INIT_ENTRY_BUF_SIZE, _fp) == buf)
	{
		if (readBoatInitData(buf, &name, &lat, &lon, &type, &flags) != 0)
		{
			goto done;
		}

		BoatInitEntry* entry = malloc(sizeof(BoatInitEntry));
		if (!entry)
		{
			ERRLOG("Failed to alloc BoatInitEntry!");
			goto done;
		}

		Boat* boat = Boat_new(lat, lon, type, flags);
		if (!boat)
		{
			ERRLOG("Failed to create new Boat!");
			free(entry);
			entry = 0;
			goto done;
		}

		entry->boat = boat;
		entry->name = name;
		entry->group = 0; // NOTE: Boat add with group from CSV not currently supported.

		return entry;
	}

done:
	fclose(_fp);
	_fp = 0;

	return 0;
}

static int readBoatInitData(char* s, char** name, double* lat, double* lon, int* type, int* flags)
{
	int rc = 0;

	char* t;
	char* w;

	*name = 0;

	if ((w = strtok_r(s, ",", &t)) == 0)
	{
		rc = -1;
		goto fail;
	}

	*name = strdup(w);
	if (0 == *name)
	{
		rc = -6;
		goto fail;
	}

	if ((w = strtok_r(0, ",", &t)) == 0)
	{
		rc = -2;
		goto fail;
	}
	*lat = strtod(w, 0);

	if ((w = strtok_r(0, ",", &t)) == 0)
	{
		rc = -3;
		goto fail;
	}
	*lon = strtod(w, 0);

	if ((w = strtok_r(0, ",", &t)) == 0)
	{
		rc = -4;
		goto fail;
	}
	*type = atoi(w);

	if ((w = strtok_r(0, ",", &t)) == 0)
	{
		rc = -5;
		goto fail;
	}
	*flags = atoi(w);

	return rc;

fail:
	if (*name)
	{
		free(*name);
		*name = 0;
	}

	return rc;
}
