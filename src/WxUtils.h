/**
 * Copyright (C) 2023 ls4096 <ls4096@8bitbyte.ca>
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

#ifndef _WxUtils_h_
#define _WxUtils_h_

#include <proteus/GeoVec.h>
#include <proteus/Weather.h>


double WxUtils_adjustWindForCurrent(proteus_Weather* wx, const proteus_GeoVec* current);


#endif // _WxUtils_h_
