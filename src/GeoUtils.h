/**
 * Copyright (C) 2022 ls4096 <ls4096@8bitbyte.ca>
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

#ifndef _GeoUtils_h_
#define _GeoUtils_h_

#include <stdbool.h>

#include <proteus/GeoPos.h>


bool GeoUtils_isApproximatelyNearVisibleLand(const proteus_GeoPos* pos, float visibility);


#endif // _GeoUtils_h_
