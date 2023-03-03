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

#include "WxUtils.h"


// Adjusts wind vector and gust value to take into account provided ocean current vector.
double WxUtils_adjustWindForCurrent(proteus_Weather* wx, const proteus_GeoVec* current)
{
	proteus_GeoVec windGust = {
		.angle = wx->wind.angle,
		.mag = wx->windGust
	};

	proteus_GeoVec_add(&wx->wind, current);

	proteus_GeoVec_add(&windGust, current);
	wx->windGust = windGust.mag;

	return windGust.angle;
}
