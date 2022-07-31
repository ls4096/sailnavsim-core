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

#ifndef _Perf_h_
#define _Perf_h_

#include "Command.h"


typedef void (*Perf_CommandHandlerFunc)(Command*);

void Perf_addAndStartRandomBoat(int groupNameLen, Perf_CommandHandlerFunc commandHandler);
int Perf_runAdditional(Perf_CommandHandlerFunc commandHandler);


#endif // _Perf_h_
