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
	unsigned int count;

	LogEntries* next;
};


static pthread_t _loggerThread = 0;
static LogEntries* _logs = 0;
static LogEntries* _logsLast = 0;
static pthread_mutex_t _logsLock;
static pthread_cond_t _logsCond;

static bool _init = false;


static void* loggerThreadMain(void* arg);

static void writeLogsCsv(LogEntry* logEntries, unsigned int count);
static void writeLogsSql(LogEntry* logEntries, unsigned int count);


static const char* _csvLoggerDir = 0;

static sqlite3* _sql = 0;
static sqlite3_stmt* _sqlInsertStmt;

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

void Logger_fillLogEntry(Boat* boat, const char* name, time_t t, LogEntry* log)
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

	log->time = t;
	log->boatName = name;
	log->boatPos = boat->pos;
	log->boatVecWater = boat->v;
	log->boatVecGround = boat->vGround;
	log->distanceTravelled = boat->distanceTravelled;
	log->damage = boat->damage;
	log->wx = wx;
	log->oceanData = od;
	log->oceanDataValid = odValid;
	log->waveData = wd;
	log->waveDataValid = wdValid;
	log->boatState = (boat->stop ? 0 : (boat->sailsDown ? 2 : 1));
	log->locState = (isWater ? 0 : 1);
}

void Logger_writeLogs(LogEntry* logEntries, unsigned int count)
{
	if (!_init)
	{
		return;
	}

	LogEntries* l = (LogEntries*) malloc(sizeof(LogEntries));
	l->logs = logEntries;
	l->count = count;
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
			unsigned int count = l->count;

			_logs = l->next;

			if (0 != pthread_mutex_unlock(&_logsLock))
			{
				ERRLOG("loggerThreadMain: Failed to unlock logs mutex!");
			}

			writeLogsSql(entries, count);
			writeLogsCsv(entries, count);

			free(entries);
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

static void writeLogsCsv(LogEntry* logEntries, unsigned int count)
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

	for (unsigned int i = 0; i < count; i++)
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
		//  - wave height

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
			snprintf(logLine, CSV_LOGGER_LINE_BUF_SIZE, "%lu,%.6f,%.6f,%.1f,%.3f,%.1f,%.3f,%.1f,%.3f,%.1f,%.3f,%.1f,%.1f,%.1f,%.1f,%.0f,%.0f,%.2f,%d,%d,%d,%.3f,%.0f,%.1f,%.3f,%.3f,%s\n",
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
				waveHeightStr
				);
		}
		else
		{
			snprintf(logLine, CSV_LOGGER_LINE_BUF_SIZE, "%lu,%.6f,%.6f,%.1f,%.3f,%.1f,%.3f,%.1f,%.3f,,,,%.1f,%.1f,%.1f,%.0f,%.0f,%.2f,%d,%d,%d,,,%.1f,%.3f,%.3f,%s\n",
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
				waveHeightStr
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
}

static void writeLogsSql(LogEntry* logEntries, unsigned int count)
{
	if (!_sql)
	{
		return;
	}

	int src;

	ERRLOG("About to begin DB transaction...");

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

	for (unsigned int i = 0; i < count; i++)
	{
		const LogEntry* const log = logEntries + i;

		if (SQLITE_OK != (src = sqlite3_reset(_sqlInsertStmt)))
		{
			ERRLOG1("Failed to reset stmt! sqlite rc=%d", src);
			continue;
		}

		int n = 0;

		if (SQLITE_OK != (src = sqlite3_bind_text(_sqlInsertStmt, ++n, log->boatName, -1, 0)))
		{
			ERRLOG1("Failed to bind boatName! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_int64(_sqlInsertStmt, ++n, log->time)))
		{
			ERRLOG1("Failed to bind time! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmt, ++n, log->boatPos.lat)))
		{
			ERRLOG1("Failed to bind boat lat! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmt, ++n, log->boatPos.lon)))
		{
			ERRLOG1("Failed to bind boat lon! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmt, ++n, log->boatVecWater.angle)))
		{
			ERRLOG1("Failed to bind boat vec water angle! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmt, ++n, log->boatVecWater.mag)))
		{
			ERRLOG1("Failed to bind boat vec water mag! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmt, ++n, log->boatVecGround.angle)))
		{
			ERRLOG1("Failed to bind boat vec ground angle! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmt, ++n, log->boatVecGround.mag)))
		{
			ERRLOG1("Failed to bind boat vec ground mag! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmt, ++n, log->wx.wind.angle)))
		{
			ERRLOG1("Failed to bind wind angle! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmt, ++n, log->wx.wind.mag)))
		{
			ERRLOG1("Failed to bind wind mag! sqlite rc=%d", src);
			continue;
		}

		if (log->oceanDataValid)
		{
			if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmt, ++n, log->oceanData.current.angle)))
			{
				ERRLOG1("Failed to bind ocean data current angle! sqlite rc=%d", src);
				continue;
			}

			if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmt, ++n, log->oceanData.current.mag)))
			{
				ERRLOG1("Failed to bind ocean data current mag! sqlite rc=%d", src);
				continue;
			}

			if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmt, ++n, log->oceanData.surfaceTemp)))
			{
				ERRLOG1("Failed to bind ocean data temp! sqlite rc=%d", src);
				continue;
			}
		}
		else
		{
			if (SQLITE_OK != (src = sqlite3_bind_null(_sqlInsertStmt, ++n)))
			{
				ERRLOG1("Failed to bind null ocean data current angle! sqlite rc=%d", src);
				continue;
			}

			if (SQLITE_OK != (src = sqlite3_bind_null(_sqlInsertStmt, ++n)))
			{
				ERRLOG1("Failed to bind null ocean data current mag! sqlite rc=%d", src);
				continue;
			}

			if (SQLITE_OK != (src = sqlite3_bind_null(_sqlInsertStmt, ++n)))
			{
				ERRLOG1("Failed to bind null ocean data temp! sqlite rc=%d", src);
				continue;
			}
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmt, ++n, log->wx.temp)))
		{
			ERRLOG1("Failed to bind air temp! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmt, ++n, log->wx.dewpoint)))
		{
			ERRLOG1("Failed to bind air dewpoint! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmt, ++n, log->wx.pressure)))
		{
			ERRLOG1("Failed to bind air pressure! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_int(_sqlInsertStmt, ++n, (int) round(log->wx.cloud))))
		{
			ERRLOG1("Failed to bind cloud! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_int(_sqlInsertStmt, ++n, (int) round(log->wx.visibility))))
		{
			ERRLOG1("Failed to bind visibility! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmt, ++n, log->wx.prate)))
		{
			ERRLOG1("Failed to bind precip rate! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_int(_sqlInsertStmt, ++n, log->wx.cond)))
		{
			ERRLOG1("Failed to bind precip type! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_int(_sqlInsertStmt, ++n, log->boatState)))
		{
			ERRLOG1("Failed to bind boat state! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_int(_sqlInsertStmt, ++n, log->locState)))
		{
			ERRLOG1("Failed to bind boat location state! sqlite rc=%d", src);
			continue;
		}

		if (log->oceanDataValid)
		{
			if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmt, ++n, log->oceanData.salinity)))
			{
				ERRLOG1("Failed to bind ocean data salinity! sqlite rc=%d", src);
				continue;
			}

			if (SQLITE_OK != (src = sqlite3_bind_int(_sqlInsertStmt, ++n, (int) round(log->oceanData.ice))))
			{
				ERRLOG1("Failed to bind ocean data ice! sqlite rc=%d", src);
				continue;
			}
		}
		else
		{
			if (SQLITE_OK != (src = sqlite3_bind_null(_sqlInsertStmt, ++n)))
			{
				ERRLOG1("Failed to bind null ocean data salinity! sqlite rc=%d", src);
				continue;
			}

			if (SQLITE_OK != (src = sqlite3_bind_null(_sqlInsertStmt, ++n)))
			{
				ERRLOG1("Failed to bind null ocean data ice! sqlite rc=%d", src);
				continue;
			}
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmt, ++n, log->distanceTravelled)))
		{
			ERRLOG1("Failed to bind distance travelled! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmt, ++n, log->damage)))
		{
			ERRLOG1("Failed to bind damage! sqlite rc=%d", src);
			continue;
		}

		if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmt, ++n, log->wx.windGust)))
		{
			ERRLOG1("Failed to bind wind gust! sqlite rc=%d", src);
			continue;
		}

		if (log->waveDataValid)
		{
			if (SQLITE_OK != (src = sqlite3_bind_double(_sqlInsertStmt, ++n, log->waveData.waveHeight)))
			{
				ERRLOG1("Failed to bind wave data wave height! sqlite rc=%d", src);
				continue;
			}
		}
		else
		{
			if (SQLITE_OK != (src = sqlite3_bind_null(_sqlInsertStmt, ++n)))
			{
				ERRLOG1("Failed to bind null wave data wave height! sqlite rc=%d", src);
				continue;
			}
		}


		if (SQLITE_DONE != (src = sqlite3_step(_sqlInsertStmt)))
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

	static const char* INSERT_STMT_STR = "INSERT INTO BoatLog VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

	if (SQLITE_OK != (src = sqlite3_prepare_v2(_sql, INSERT_STMT_STR, strlen(INSERT_STMT_STR) + 1, &_sqlInsertStmt, 0)))
	{
		ERRLOG1("Failed to prepare insert statement. sqlite rc=%d", src);
		return -1;
	}

	return 0;
}
