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

#ifndef _BoatInitParser_h_
#define _BoatInitParser_h_

#include "Boat.h"

typedef struct
{
	Boat* boat;
	char* name;
	char* group;
} BoatInitEntry;


int BoatInitParser_start(const char* boatInitFilename, const char* sqliteDbFilename);
BoatInitEntry* BoatInitParser_getNext();


#endif // _BoatInitParser_h_
