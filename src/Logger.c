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

#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sqlite3.h>

#include <proteus/Compass.h>
#include <proteus/GeoInfo.h>
#include <proteus/Ocean.h>
#include <proteus/Wave.h>
#include <proteus/Weather.h>

#include "Logger.h"

#include "Boat.h"
#include "ErrLog.h"


#define ERRLOG_ID "Logger"
#define THREAD_NAME "Logger"

#define CSV_LOGGER_DIR_PATH_MAXLEN (4096 - 512)
#define CSV_LOGGER_LINE_BUF_SIZE (2048)


typedef struct LogEntries LogEntries;

struct LogEntries
{
	LogEntry* logs;
	unsigned int lCount;

	CelestialSightEntry* cs;
	unsigned int csCount;

	LogEntries* next;
};


static pthread_t _loggerThread = 0;
static LogEntries* _logs = 0;
static LogEntries* _logsLast = 0;
static pthread_mutex_t _logsLock;
static pthread_cond_t _logsCond;

static bool _init = false;


static void* loggerThreadMain(void* arg);

static void writeLogsCsv(const LogEntry* const logEntries, unsigned int lCount, const CelestialSightEntry* const csEntries, unsigned int csCount);
static void writeLogsSql(const LogEntry* const logEntries, unsigned int lCount, const CelestialSightEntry* const csEntries, unsigned int csCount);

static void writeLogsSqlBoatLogs(const LogEntry* const logEntries, unsigned int lCount);
static void writeLogsSqlCelestialSights(const CelestialSightEntry* const csEntries, unsigned int csCount);


static const char* _csvLoggerDir = 0;

static sqlite3* _sql = 0;
static sqlite3_stmt* _sqlInsertStmtBoatLog;
static sqlite3_stmt* _sqlInsertStmtCelestialSight;

static int setupSql(const char* sqliteDbFilename);


int Logger_init(const char* csvLoggerDir, const char* sqliteDbFilename)
{
	if (!csvLoggerDir && !sqliteDbFilename)
	{
		ERRLOG("No logger output paths provided, so not logging to anywhere.");
		return 0;
	}

	if (csvLoggerDir && (strlen(csvLoggerDir) >= CSV_LOGGER_DIR_PATH_MAXLEN))
	{
		ERRLOG("CSV logger directory path name is too long!");
		return 0;
	}

	_csvLoggerDir = strdup(csvLoggerDir);

	if (0 != pthread_mutex_init(&_logsLock, 0))
	{
		ERRLOG("Failed to init logs mutex!");
		return -4;
	}

	if (0 != pthread_cond_init(&_logsCond, 0))
	{
		ERRLOG("Failed to init logs condvar!");
		return -4;
	}

	int rc;

	if (0 != (rc = setupSql(sqliteDbFilename)))
	{
		if (1 != rc)
		{
			ERRLOG("Failed to perform SQLite setup!");
			return -2;
		}
	}

	if (0 != pthread_create(&_loggerThread, 0, &loggerThreadMain, 0))
	{
		ERRLOG("Failed to start boat logging thread!");
		return -1;
	}

#if defined(_GNU_SOURCE) && defined(__GLIBC__)
	if (0 != pthread_setname_np(_loggerThread, THREAD_NAME))
	{
		ERRLOG1("Couldn't set thread name to %s. Continuing anyway.", THREAD_NAME);
	}
#endif

	_init = true;
	return 0;
}

static char* LOGGER_DEFAULT_LOG_BOAT_NAME = "__default__";

void Logger_fillLogEntry(Boat* boat, const char* name, time_t t, bool reportVisible, LogEntry* log)
{
	if (!_init)
	{
		return;
	}

	proteus_Weather wx;
	proteus_Weather_get(&boat->pos, &wx, false);

	proteus_OceanData od;
	const bool odValid = proteus_Ocean_get(&boat->pos, &od);

	proteus_WaveData wd;
	const bool wdValid = proteus_Wave_get(&boat->pos, &wd);

	const bool isWater = proteus_GeoInfo_isWater(&boat->pos);

	const double compassMagDec = proteus_Compass_magdec(&boat->pos, t);

	log->time = t;
	log->boatName = strdup(name);
	if (!log->boatName)
	{
		// Failed to allocate memory, so use a default boat name to continue.
		ERRLOG1("Failed to strdup boat name (%s)! Using default boat name instead.", name);
		log->boatName = LOGGER_DEFAULT_LOG_BOAT_NAME;
	}
	log->boatPos = boat->pos;
	log->boatVecWater = boat->v;
	log->boatVecGround = boat->vGround;
	log->compassMagDec = compassMagDec;
	log->distanceTravelled = boat->distanceTravelled;
	log->damage = boat->damage;
	log->wx = wx;
	log->oceanData = od;
	log->oceanDataValid = odValid;
	log->waveData = wd;
	log->waveDataValid = wdValid;
	log->boatState = (boat->stop ? 0 : (boat->sailsDown ? 2 : 1));
	log->locState = (isWater ? 0 : 1);
	log->reportVisible = reportVisible;
}

void Logger_writeLogs(LogEntry* logEntries, unsigned int lCount, CelestialSightEntry* csEntries, unsigned int csCount)
{
	if (!_init)
	{
		return;
	}

	LogEntries* l = malloc(sizeof(LogEntries));
	if (!l)
	{
		ERRLOG("writeLogs: Alloc failed for LogEntries!");
		return;
	}

	l->logs = logEntries;
	l->lCount = lCount;
	l->cs = csEntries;
	l->csCount = csCount;

	l->next = 0;

	if (0 != pthread_mutex_lock(&_logsLock))
	{
		ERRLOG("writeLogs: Failed to lock logs mutex!");
		free(l);
		return;
	}

	if (_logs == 0)
	{
		_logs = l;
	}
	else
	{
		_logsLast->next = l;
	}

	_logsLast = l;

	if (0 != pthread_cond_signal(&_logsCond))
	{
		ERRLOG("writeLogs: Failed to signal condvar!");
	}

	if (0 != pthread_mutex_unlock(&_logsLock))
	{
		ERRLOG("writeLogs: Failed to unlock logs mutex!");
	}
}


static void* loggerThreadMain(void* arg)
{
	for (;;)
	{
		if (0 != pthread_mutex_lock(&_logsLock))
		{
			ERRLOG("loggerThreadMain: Failed to lock logs mutex!");
			continue;
		}

		while (_logs == 0)
		{
			if (0 != pthread_cond_wait(&_logsCond, &_logsLock))
			{
				ERRLOG("loggerThreadMain: Failed to wait on condvar!");
			}
		}

		while (_logs != 0)
		{
			LogEntries* l = _logs;

			LogEntry* entries = l->logs;
			unsigned int lCount = l->lCount;

			CelestialSightEntry* cs = l->cs;
			unsigned int csCount = l->csCount;

			_logs = l->next;

			if (0 != pthread_mutex_unlock(&_logsLock))
			{
				ERRLOG("loggerThreadMain: Failed to unlock logs mutex!");
			}

			writeLogsSql(entries, lCount, cs, csCount);
			writeLogsCsv(entries, lCount, cs, csCount);

			for (int i = 0; i < lCount; i++)
			{
				if (entries[i].boatName != LOGGER_DEFAULT_LOG_BOAT_NAME)
				{
					free(entries[i].boatName);
				}
			}

			free(entries);
			free(cs);
			free(l);

			if (0 != pthread_mutex_lock(&_logsLock))
			{
				ERRLOG("loggerThreadMain: Failed to lock logs mutex!");
			}
		}

		if (0 != pthread_mutex_unlock(&_logsLock))
		{
			ERRLOG("loggerThreadMain: Failed to unlock logs mutex!");
		}
	}

	return 0;
}

static void writeLogsCsv(const LogEntry* const logEntries, unsigned int lCount, const CelestialSightEntry* const csEntries, unsigned int csCount)
{
	if (!_csvLoggerDir)
	{
		return;
	}

	DIR* dir = opendir(_csvLoggerDir);
	if (dir)
	{
		// Directory exists.
		closedir(dir);
	}
	else if (ENOENT == errno)
	{
		// Directory doesn't exist, so don't write CSV logs.
		return;
	}
	else
	{
		// opendir failed for some other reason.
		ERRLOG1("opendir failed! errno=%d", errno);
		return;
	}

	// Boat logs
	for (unsigned int i = 0; i < lCount; i++)
	{
		const LogEntry* const log = logEntries + i;

		char filepath[CSV_LOGGER_DIR_PATH_MAXLEN + 512];
		snprintf(filepath, CSV_LOGGER_DIR_PATH_MAXLEN + 512, "%s/%s.csv", _csvLoggerDir, log->boatName);

		FILE* f = fopen(filepath, "a");
		if (f == 0)
		{
			ERRLOG1("Failed to open log output file: %s", filepath);
			continue;
		}

		char logLine[CSV_LOGGER_LINE_BUF_SIZE];

		// Log:
		//  - time
		//  - boat lat
		//  - boat lon
		//  - boat course (water)
		//  - boat speed (water)
		//  - boat track (ground)
		//  - boat speed (ground)
		//  - wind direction
		//  - wind speed
		//  - ocean current direction
		//  - ocean current speed
		//  - water temperature
		//  - air temperature
		//  - dewpoint
		//  - pressure
		//  - cloud
		//  - visibility
		//  - precip rate
		//  - precip type
		//  - boat status (0: stopped; 1: moving - sailing; 2: moving - sails down)
		//  - boat location (0: water; 1: landed)
		//  - water salinity
		//  - ocean ice
		//  - distance travelled
		//  - boat damage
		//  - wind gust
		//  - wave height
		//  - compass magnetic declination
		//  - report visibility (0: visible; 1: invisible)

		char waveHeightStr[16];
		if (log->waveDataValid)
		{
			snprintf(waveHeightStr, 16, "%.2f", log->waveData.waveHeight);
		}
		else
		{
			waveHeightStr[0] = 0;
		}

		if (log->oceanDataValid)
		{
			snprintf(logLine, CSV_LOGGER_LINE_BUF_SIZE, "%lu,%.6f,%.6f,%.1f,%.3f,%.1f,%.3f,%.1f,%.3f,%.1f,%.3f,%.1f,%.1f,%.1f,%.1f,%.0f,%.0f,%.2f,%d,%d,%d,%.3f,%.0f,%.1f,%.3f,%.3f,%s,%.3f,%d\n",
				log->time,
				log->boatPos.lat,
				log->boatPos.lon,
				log->boatVecWater.angle,
				log->boatVecWater.mag,
				log->boatVecGround.angle,
				log->boatVecGround.mag,
				log->wx.wind.angle,
				log->wx.wind.mag,
				log->oceanData.current.angle,
				log->oceanData.current.mag,
				log->oceanData.surfaceTemp,
				log->wx.temp,
				log->wx.dewpoint,
				log->wx.pressure,
				log->wx.cloud,
				log->wx.visibility,
				log->wx.prate,
				log->wx.cond,
				log->boatState,
				log->locState,
				log->oceanData.salinity,
				log->oceanData.ice,
				log->distanceTravelled,
				log->damage,
				log->wx.windGust,
				waveHeightStr,
				log->compassMagDec,
				(log->reportVisible ? 0 : 1)
				);
		}
		else
		{
			snprintf(logLine, CSV_LOGGER_LINE_BUF_SIZE, "%lu,%.6f,%.6f,%.1f,%.3f,%.1f,%.3f,%.1f,%.3f,,,,%.1f,%.1f,%.1f,%.0f,%.0f,%.2f,%d,%d,%d,,,%.1f,%.3f,%.3f,%s,%.3f,%d\n",
				log->time,
				log->boatPos.lat,
				log->boatPos.lon,
				log->boatVecWater.angle,
				log->boatVecWater.mag,
				log->boatVecGround.angle,
				log->boatVecGround.mag,
				log->wx.wind.angle,
				log->wx.wind.mag,
				log->wx.temp,
				log->wx.dewpoint,
				log->wx.pressure,
				log->wx.cloud,
				log->wx.visibility,
				log->wx.prate,
				log->wx.cond,
				log->boatState,
				log->locState,
				log->distanceTravelled,
				log->damage,
				log->wx.windGust,
				waveHeightStr,
				log->compassMagDec,
				(log->reportVisible ? 0 : 1)
				);
		}

		size_t l = strlen(logLine);
		size_t w;
		if (l != (w = fwrite(logLine, 1, l, f)))
		{
			ERRLOG2("Failed to write log entry of %ld bytes for %s!", l, log->boatName);
		}

		fclose(f);
	}

	// Celestial sights
	for (unsigned int i = 0; i < csCount; i++)
	{
		const CelestialSightEntry* const cse = csEntries + i;

		char filepath[CSV_LOGGER_DIR_PATH_MAXLEN + 512];
		snprintf(filepath, CSV_LOGGER_DIR_PATH_MAXLEN + 512, "%s/%s-cs.csv", _csvLoggerDir, cse->boatName);

		FILE* f = fopen(filepath, "a");
		if (f == 0)
		{
			ERRLOG1("Failed to open log output file: %s", filepath);
			continue;
		}

		char logLine[CSV_LOGGER_LINE_BUF_SIZE];

		// Log:
		//  - time
		//  - object ID
		//  - azimuth
		//  - altitude
		//  - compass magnetic declination

		snprintf(logLine, CSV_LOGGER_LINE_BUF_SIZE, "%lu,%d,%.6f,%.6f,%.3f\n",
			cse->time,
			cse->obj,
			cse->az,
			cse->alt,
			cse->compassMagDec
			);

		size_t l = strlen(logLine);
		size_t w;
		if (l != (w = fwrite(logLine, 1, l, f)))
		{
			ERRLOG2("Failed to write celestial sight entry of %ld bytes for %s!", l, cse->boatName);
		}

		fclose(f);
	}
}

static void writeLogsSql(const LogEntry* const logEntries, unsigned int lCount, const CelestialSightEntry* const csEntries, unsigned int csCount)
{
	if (!_sql)
	{
		return;
	}

	writeLogsSqlBoatLogs(logEntries, lCount);
	writeLogsSqlCelestialSights(csEntries, csCount);
}

static void writeLogsSqlBoatLogs(const LogEntry* const logEntries, unsigned int lCount)
{
	int src;

	ERRLOG("About to begin BoatLogs DB transaction...");

	while (SQLITE_OK != (src = sqlite3_exec(_sql, "BEGIN IMMEDIATE TRANSACTION;", 0, 0, 0)))
	{
		if (SQLITE_BUSY == src)
		{
			ERRLOG("Got BUSY trying to start transaction. Trying again in 1 second...");
			sleep(1);
		}
		else
		{
			ERRLOG1("Failed to begin SQL transaction! sqlite rc=%d", src);
			return;
		}
	}

	ERRLOG("DB transaction started.");

	for (unsigned int i = 0; i < lCount; i++)
	{
		const LogEntry* const log = logEntries + i;

		if (SQLITE_OK != (src = sqlite3_reset(_sqlInsertStmtBoatLog)))
		{
			ERRLOG1("Failed to reset stmt! sqlite rc=%d", src);
			continue;
		}


		int n = 0;

		if (SQLITE_OK != (src = sqlite3_bind_text(_sqlInsertStmtBoatLog, ++n, log->boatName, -1, 0)))
		{
			ERRLOG1("Failed to bind boatName! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_int64(_sqlInsertStmtBoatLog, ++n, log->time)))
		{
			ERRLOG1("Failed to bind time! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmtBoatLog, ++n, log->boatPos.lat)))
		{
			ERRLOG1("Failed to bind boat lat! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmtBoatLog, ++n, log->boatPos.lon)))
		{
			ERRLOG1("Failed to bind boat lon! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmtBoatLog, ++n, log->boatVecWater.angle)))
		{
			ERRLOG1("Failed to bind boat vec water angle! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmtBoatLog, ++n, log->boatVecWater.mag)))
		{
			ERRLOG1("Failed to bind boat vec water mag! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmtBoatLog, ++n, log->boatVecGround.angle)))
		{
			ERRLOG1("Failed to bind boat vec ground angle! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmtBoatLog, ++n, log->boatVecGround.mag)))
		{
			ERRLOG1("Failed to bind boat vec ground mag! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmtBoatLog, ++n, log->wx.wind.angle)))
		{
			ERRLOG1("Failed to bind wind angle! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmtBoatLog, ++n, log->wx.wind.mag)))
		{
			ERRLOG1("Failed to bind wind mag! sqlite rc=%d", src);
			continue;
		}

		if (log->oceanDataValid)
		{
			if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmtBoatLog, ++n, log->oceanData.current.angle)))
			{
				ERRLOG1("Failed to bind ocean data current angle! sqlite rc=%d", src);
				continue;
			}

			if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmtBoatLog, ++n, log->oceanData.current.mag)))
			{
				ERRLOG1("Failed to bind ocean data current mag! sqlite rc=%d", src);
				continue;
			}

			if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmtBoatLog, ++n, log->oceanData.surfaceTemp)))
			{
				ERRLOG1("Failed to bind ocean data temp! sqlite rc=%d", src);
				continue;
			}
		}
		else
		{
			if (SQLITE_OK != (src = sqlite3_bind_null(_sqlInsertStmtBoatLog, ++n)))
			{
				ERRLOG1("Failed to bind null ocean data current angle! sqlite rc=%d", src);
				continue;
			}

			if (SQLITE_OK != (src = sqlite3_bind_null(_sqlInsertStmtBoatLog, ++n)))
			{
				ERRLOG1("Failed to bind null ocean data current mag! sqlite rc=%d", src);
				continue;
			}

			if (SQLITE_OK != (src = sqlite3_bind_null(_sqlInsertStmtBoatLog, ++n)))
			{
				ERRLOG1("Failed to bind null ocean data temp! sqlite rc=%d", src);
				continue;
			}
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmtBoatLog, ++n, log->wx.temp)))
		{
			ERRLOG1("Failed to bind air temp! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmtBoatLog, ++n, log->wx.dewpoint)))
		{
			ERRLOG1("Failed to bind air dewpoint! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmtBoatLog, ++n, log->wx.pressure)))
		{
			ERRLOG1("Failed to bind air pressure! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_int(_sqlInsertStmtBoatLog, ++n, (int) round(log->wx.cloud))))
		{
			ERRLOG1("Failed to bind cloud! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_int(_sqlInsertStmtBoatLog, ++n, (int) round(log->wx.visibility))))
		{
			ERRLOG1("Failed to bind visibility! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmtBoatLog, ++n, log->wx.prate)))
		{
			ERRLOG1("Failed to bind precip rate! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_int(_sqlInsertStmtBoatLog, ++n, log->wx.cond)))
		{
			ERRLOG1("Failed to bind precip type! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_int(_sqlInsertStmtBoatLog, ++n, log->boatState)))
		{
			ERRLOG1("Failed to bind boat state! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_int(_sqlInsertStmtBoatLog, ++n, log->locState)))
		{
			ERRLOG1("Failed to bind boat location state! sqlite rc=%d", src);
			continue;
		}

		if (log->oceanDataValid)
		{
			if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmtBoatLog, ++n, log->oceanData.salinity)))
			{
				ERRLOG1("Failed to bind ocean data salinity! sqlite rc=%d", src);
				continue;
			}

			if (SQLITE_OK != (src = sqlite3_bind_int(_sqlInsertStmtBoatLog, ++n, (int) round(log->oceanData.ice))))
			{
				ERRLOG1("Failed to bind ocean data ice! sqlite rc=%d", src);
				continue;
			}
		}
		else
		{
			if (SQLITE_OK != (src = sqlite3_bind_null(_sqlInsertStmtBoatLog, ++n)))
			{
				ERRLOG1("Failed to bind null ocean data salinity! sqlite rc=%d", src);
				continue;
			}

			if (SQLITE_OK != (src = sqlite3_bind_null(_sqlInsertStmtBoatLog, ++n)))
			{
				ERRLOG1("Failed to bind null ocean data ice! sqlite rc=%d", src);
				continue;
			}
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmtBoatLog, ++n, log->distanceTravelled)))
		{
			ERRLOG1("Failed to bind distance travelled! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmtBoatLog, ++n, log->damage)))
		{
			ERRLOG1("Failed to bind damage! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmtBoatLog, ++n, log->wx.windGust)))
		{
			ERRLOG1("Failed to bind wind gust! sqlite rc=%d", src);
			continue;
		}

		if (log->waveDataValid)
		{
			if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmtBoatLog, ++n, log->waveData.waveHeight)))
			{
				ERRLOG1("Failed to bind wave data wave height! sqlite rc=%d", src);
				continue;
			}
		}
		else
		{
			if (SQLITE_OK != (src = sqlite3_bind_null(_sqlInsertStmtBoatLog, ++n)))
			{
				ERRLOG1("Failed to bind null wave data wave height! sqlite rc=%d", src);
				continue;
			}
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmtBoatLog, ++n, log->compassMagDec)))
		{
			ERRLOG1("Failed to bind compass mag dec! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_int(_sqlInsertStmtBoatLog, ++n, log->reportVisible ? 0 : 1)))
		{
			ERRLOG1("Failed to bind report invisibility boolean! sqlite rc=%d", src);
			continue;
		}


		if (SQLITE_DONE != (src = sqlite3_step(_sqlInsertStmtBoatLog)))
		{
			ERRLOG1("Failed to step insert! sqlite rc=%d", src);
			continue;
		}
	}

	src = sqlite3_exec(_sql, "END TRANSACTION;", 0, 0, 0);
	if (SQLITE_OK != src)
	{
		ERRLOG1("Failed to end SQL transaction! sqlite rc=%d", src);

		src = sqlite3_exec(_sql, "ROLLBACK;", 0, 0, 0);
		if (SQLITE_OK != src)
		{
			ERRLOG1("Failed to rollback after failed end transaction! sqlite rc=%d", src);
		}
	}
	else
	{
		ERRLOG("Committed boat logs to DB.");
	}
}

static void writeLogsSqlCelestialSights(const CelestialSightEntry* const csEntries, unsigned int csCount)
{
	if (csCount == 0)
	{
		ERRLOG("No CelestialSights to write to DB.");
		return;
	}

	int src;

	ERRLOG("About to begin CelestialSights DB transaction...");

	while (SQLITE_OK != (src = sqlite3_exec(_sql, "BEGIN IMMEDIATE TRANSACTION;", 0, 0, 0)))
	{
		if (SQLITE_BUSY == src)
		{
			ERRLOG("Got BUSY trying to start transaction. Trying again in 1 second...");
			sleep(1);
		}
		else
		{
			ERRLOG1("Failed to begin SQL transaction! sqlite rc=%d", src);
			return;
		}
	}

	ERRLOG("DB transaction started.");

	for (unsigned int i = 0; i < csCount; i++)
	{
		const CelestialSightEntry* const cse = csEntries + i;

		if (SQLITE_OK != (src = sqlite3_reset(_sqlInsertStmtCelestialSight)))
		{
			ERRLOG1("Failed to reset stmt! sqlite rc=%d", src);
			continue;
		}


		int n = 0;

		if (SQLITE_OK != (src = sqlite3_bind_text(_sqlInsertStmtCelestialSight, ++n, cse->boatName, -1, 0)))
		{
			ERRLOG1("Failed to bind boatName! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_int64(_sqlInsertStmtCelestialSight, ++n, cse->time)))
		{
			ERRLOG1("Failed to bind time! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_int64(_sqlInsertStmtCelestialSight, ++n, cse->obj)))
		{
			ERRLOG1("Failed to bind object ID! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmtCelestialSight, ++n, cse->az)))
		{
			ERRLOG1("Failed to bind object azimuth! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmtCelestialSight, ++n, cse->alt)))
		{
			ERRLOG1("Failed to bind object altitude! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmtCelestialSight, ++n, cse->compassMagDec)))
		{
			ERRLOG1("Failed to bind compass mag dec! sqlite rc=%d", src);
			continue;
		}


		if (SQLITE_DONE != (src = sqlite3_step(_sqlInsertStmtCelestialSight)))
		{
			ERRLOG1("Failed to step insert! sqlite rc=%d", src);
			continue;
		}
	}

	src = sqlite3_exec(_sql, "END TRANSACTION;", 0, 0, 0);
	if (SQLITE_OK != src)
	{
		ERRLOG1("Failed to end SQL transaction! sqlite rc=%d", src);

		src = sqlite3_exec(_sql, "ROLLBACK;", 0, 0, 0);
		if (SQLITE_OK != src)
		{
			ERRLOG1("Failed to rollback after failed end transaction! sqlite rc=%d", src);
		}
	}
	else
	{
		ERRLOG("Committed celestial sights to DB.");
	}
}

static int setupSql(const char* sqliteDbFilename)
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
			ERRLOG("No SQLite DB file found, so not logging there.");
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

	int src;

	if (SQLITE_OK != (src = sqlite3_open(sqliteDbFilename, &_sql)))
	{
		ERRLOG1("Failed to open SQLite DB. sqlite rc=%d", src);
		return -1;
	}


	static const char* BOAT_LOG_INSERT_STMT_STR = "INSERT INTO BoatLog VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
	if (SQLITE_OK != (src = sqlite3_prepare_v2(_sql, BOAT_LOG_INSERT_STMT_STR, strlen(BOAT_LOG_INSERT_STMT_STR) + 1, &_sqlInsertStmtBoatLog, 0)))
	{
		ERRLOG1("Failed to prepare BoatLog insert statement. sqlite rc=%d", src);
		return -1;
	}

	static const char* CELESTIAL_SIGHT_INSERT_STMT_STR = "INSERT INTO CelestialSight VALUES (?,?,?,?,?,?);";
	if (SQLITE_OK != (src = sqlite3_prepare_v2(_sql, CELESTIAL_SIGHT_INSERT_STMT_STR, strlen(CELESTIAL_SIGHT_INSERT_STMT_STR) + 1, &_sqlInsertStmtCelestialSight, 0)))
	{
		ERRLOG1("Failed to prepare CelestialSight insert statement. sqlite rc=%d", src);
		return -1;
	}


	return 0;
}
