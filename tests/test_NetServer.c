/**
 * Copyright (C) 2026 ls4096 <ls4096@8bitbyte.ca>
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

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "tests.h"
#include "tests_assert.h"

#include "BoatRegistry.h"
#include "NetServer.h"


typedef struct {
	int fd;
} ThreadArgs;

typedef struct {
	int rc;
	int clientFd;
	pthread_t thread;
	ThreadArgs* threadArgs;
} RequestProcessorCtx;

typedef enum {
	WPT_NORMAL,
	WPT_ONE_BYTE,
	WPT_RANDOM,
	WPT_RANDOM_WITH_SLEEP
} WritePatternType;


static RequestProcessorCtx startRequestProcessor();
static int finishRequestProcessor(RequestProcessorCtx* ctx);

static int performAllRequests(WritePatternType wpt);
static int requestAndCheckResponse(const char* req, const char* resp, int fd, WritePatternType wpt);

static void* requestProcessorThreadMain(void* arg);

static int getRandInt(int max);
static unsigned int _initRandSeed = 0;
static unsigned int _randSeed = 0;


int test_NetServer_processRequests()
{
	EQUALS(0, BoatRegistry_init(60));

	EQUALS(0, performAllRequests(WPT_NORMAL));
	EQUALS(0, performAllRequests(WPT_ONE_BYTE));
	EQUALS(0, performAllRequests(WPT_RANDOM));
	EQUALS(0, performAllRequests(WPT_RANDOM_WITH_SLEEP));

	BoatRegistry_destroy();

	return 0;
}


static RequestProcessorCtx startRequestProcessor()
{
	RequestProcessorCtx ctx;
	int fds[2];

	ctx.rc = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
	if (ctx.rc)
	{
		return ctx;
	}

	ctx.threadArgs = malloc(sizeof(ThreadArgs));
	ctx.clientFd = fds[0];
	ctx.threadArgs->fd = fds[1];
	ctx.rc = pthread_create(&ctx.thread, 0, &requestProcessorThreadMain, ctx.threadArgs);

	return ctx;
}

static int finishRequestProcessor(RequestProcessorCtx* ctx)
{
	close(ctx->clientFd);

	int rc = pthread_join(ctx->thread, 0);
	EQUALS(0, rc);

	free(ctx->threadArgs);

	return 0;
}

int performAllRequests(WritePatternType wpt)
{
	RequestProcessorCtx ctx;


	// Start and finish without sending anything.
	ctx = startRequestProcessor();
	EQUALS(0, ctx.rc);
	EQUALS(0, finishRequestProcessor(&ctx));


	// Send one invalid request.
	ctx = startRequestProcessor();
	EQUALS(0, ctx.rc);

	EQUALS(0, requestAndCheckResponse("invalid_request,\r\n", "error\n", ctx.clientFd, wpt));

	EQUALS(0, finishRequestProcessor(&ctx));


	// Send one valid request.
	ctx = startRequestProcessor();
	EQUALS(0, ctx.rc);

	EQUALS(0, requestAndCheckResponse("wind,0,0\r\n", "wind,0.000000,0.000000,-999.000000,-999.000000\n", ctx.clientFd, wpt));

	EQUALS(0, finishRequestProcessor(&ctx));


	// Send multiple valid requests.
	ctx = startRequestProcessor();
	EQUALS(0, ctx.rc);

	EQUALS(0, requestAndCheckResponse("wind,0,0\r\n", "wind,0.000000,0.000000,-999.000000,-999.000000\n", ctx.clientFd, wpt));
	EQUALS(0, requestAndCheckResponse("wind,-0.1,-0.1\r\n", "wind,-0.100000,-0.100000,-999.000000,-999.000000\n", ctx.clientFd, wpt));
	EQUALS(0, requestAndCheckResponse("wind,10.9,-11.8\r\n", "wind,10.900000,-11.800000,-999.000000,-999.000000\n", ctx.clientFd, wpt));

	EQUALS(0, requestAndCheckResponse("wind,0,0\n", "wind,0.000000,0.000000,-999.000000,-999.000000\n", ctx.clientFd, wpt));
	EQUALS(0, requestAndCheckResponse("wind,-0.1,-0.1\n", "wind,-0.100000,-0.100000,-999.000000,-999.000000\n", ctx.clientFd, wpt));
	EQUALS(0, requestAndCheckResponse("wind,10.9,-11.8\n", "wind,10.900000,-11.800000,-999.000000,-999.000000\n", ctx.clientFd, wpt));

	EQUALS(0, requestAndCheckResponse("bd,fakename,\n", "bd,fakename,noboat\n", ctx.clientFd, wpt));
	EQUALS(0, requestAndCheckResponse("bd_nc,fakename2,\n", "bd_nc,fakename2,noboat\n", ctx.clientFd, wpt));

	EQUALS(0, requestAndCheckResponse("boatgroupmembers,fakename,\n", "boatgroupmembers,fakename,noboat\n", ctx.clientFd, wpt));
	EQUALS(0, requestAndCheckResponse("boatgroupmembers,reallylongfakename_reallylongfakename_reallylongfakename_,\n", "boatgroupmembers,reallylongfakename_reallylongfakename_reallylongfakename_,noboat\n", ctx.clientFd, wpt));

	EQUALS(0, finishRequestProcessor(&ctx));


	// Send multiple valid requests, concatenated together.
	ctx = startRequestProcessor();
	EQUALS(0, ctx.rc);

	EQUALS(0, requestAndCheckResponse("wind,0,0\r\nwind,1,1\r\n", "wind,0.000000,0.000000,-999.000000,-999.000000\nwind,1.000000,1.000000,-999.000000,-999.000000\n", ctx.clientFd, wpt));

	EQUALS(0, requestAndCheckResponse("bd,name1,\nbd_nc,name2,\nbd,name3,\nbd,name4,\nbd,name5,\nbd,name6,\nbd,name7,\nbd_nc,name8,\n", "bd,name1,noboat\nbd_nc,name2,noboat\nbd,name3,noboat\nbd,name4,noboat\nbd,name5,noboat\nbd,name6,noboat\nbd,name7,noboat\nbd_nc,name8,noboat\n", ctx.clientFd, wpt));

	EQUALS(0, finishRequestProcessor(&ctx));


	return 0;
}

#define READ_BUF_SIZE (64 * 1024)
#define SLEEP_US (5000)

static int requestAndCheckResponse(const char* req, const char* resp, int fd, WritePatternType wpt)
{
	const ssize_t toWrite = strlen(req);
	const ssize_t toRead = strlen(resp);

	ssize_t written = 0;
	while (written < toWrite)
	{
		size_t thisWrite;
		bool doSleep = false;
		switch (wpt)
		{
		case WPT_NORMAL:
			thisWrite = toWrite - written;
			break;
		case WPT_ONE_BYTE:
			thisWrite = 1;
			break;
		case WPT_RANDOM:
			thisWrite = getRandInt(toWrite - written - 1) + 1;
			break;
		case WPT_RANDOM_WITH_SLEEP:
			doSleep = true;
			thisWrite = getRandInt(toWrite - written - 1) + 1;
			break;
		default:
			thisWrite = toWrite - written;
			break;
		}

		ssize_t wb = write(fd, req + written, thisWrite);
		IS_TRUE(wb > 0);
		written += wb;

		if (doSleep)
		{
			usleep(SLEEP_US);
		}
	}

	char buf[READ_BUF_SIZE] = { 0 };

	ssize_t readBytes = 0;
	for (;;)
	{
		ssize_t rb = read(fd, buf + readBytes, READ_BUF_SIZE - readBytes);
		IS_TRUE(rb >= 0);
		readBytes += rb;

		if (readBytes >= toRead)
		{
			IS_TRUE(0 == memcmp(resp, buf, toRead));
			return 0;
		}
	}

	return 1;
}

static void* requestProcessorThreadMain(void* arg)
{
	ThreadArgs* ta = (ThreadArgs*)arg;
	NetServer_processRequests(gettid(), ta->fd);
	close(ta->fd);
	return 0;
}

static int getRandInt(int max)
{
	if (_initRandSeed == 0)
	{
		_initRandSeed = time(0);
		_randSeed = _initRandSeed;
	}

	return (rand_r(&_randSeed) % (max + 1));
}
