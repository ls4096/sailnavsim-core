/**
 * Copyright (C) 2020-2026 ls4096 <ls4096@8bitbyte.ca>
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

#include <signal.h>
#include <stdio.h>

#include "tests.h"

typedef int (*test_func)(void);

static const char* TEST_NAMES[] = {
	"BoatRegistry_basic",
	"BoatRegistry_basicWithGroups",
	"BoatRegistry_load",
	"BoatRegistry_loadWithBigGroups",
	"NetServer_processRequests",
	"WxUtils"
};

static const test_func TEST_FUNCS[] = {
	&test_BoatRegistry_runBasic,
	&test_BoatRegistry_runBasicWithGroups,
	&test_BoatRegistry_runLoad,
	&test_BoatRegistry_runLoadWithBigGroups,
	&test_NetServer_processRequests,
	&test_WxUtils
};

int main()
{
	int testres;
	int sum = 0;

	// We might get SIGPIPE when calling write() in the NetServer tests, so just ignore it.
	signal(SIGPIPE, SIG_IGN);

	printf("Running tests for sailnavsim...\n");

	for (size_t i = 0; i < (sizeof(TEST_NAMES) / sizeof(const char*)); i++)
	{
		printf("%s...\n", TEST_NAMES[i]);

		if (0 != (testres = TEST_FUNCS[i]()))
		{
			printf("\t\033[31mFAILED!\033[0m\n");
		}
		else
		{
			printf("\tOK\n");
		}

		sum += testres;
	}

	if (sum == 0)
	{
		printf("\033[32mAll tests passed!\033[0m\n");
	}
	else
	{
		printf("\033[31mThere were test failures!\033[0m\n");
	}

	return sum;
}
