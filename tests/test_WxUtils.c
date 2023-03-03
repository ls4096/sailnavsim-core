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

#include <stdbool.h>

#include <proteus/GeoVec.h>
#include <proteus/Weather.h>

#include "tests.h"
#include "tests_assert.h"

#include "WxUtils.h"


int test_WxUtils()
{
	proteus_Weather wx;
	proteus_GeoVec cur;
	double gustAngle;


	wx.wind.angle = 0.0;
	wx.wind.mag = 0.0;
	wx.windGust = 0.0f;

	cur.angle = 0.0;
	cur.mag = 0.0;

	gustAngle = WxUtils_adjustWindForCurrent(&wx, &cur);
	EQUALS_DBL(0.0, wx.wind.angle);
	EQUALS_DBL(0.0, wx.wind.mag);
	EQUALS_DBL(0.0, gustAngle);
	EQUALS_FLT(0.0f, wx.windGust);


	cur.angle = 90.0;
	cur.mag = 1.0;

	gustAngle = WxUtils_adjustWindForCurrent(&wx, &cur);
	EQUALS_DBL(90.0, wx.wind.angle);
	EQUALS_DBL(1.0, wx.wind.mag);
	EQUALS_DBL(90.0, gustAngle);
	EQUALS_FLT(1.0f, wx.windGust);


	wx.wind.angle = 270.0;
	wx.wind.mag = 1.0;
	wx.windGust = 1.0f;

	gustAngle = WxUtils_adjustWindForCurrent(&wx, &cur);
	EQUALS_DBL(0.0, wx.wind.angle);
	EQUALS_DBL(0.0, wx.wind.mag);
	EQUALS_DBL(0.0, gustAngle);
	EQUALS_FLT(0.0f, wx.windGust);


	wx.wind.angle = 90.0;
	wx.wind.mag = 1.0;
	wx.windGust = 1.0f;

	cur.angle = 180.0;
	cur.mag = 1.0;

	gustAngle = WxUtils_adjustWindForCurrent(&wx, &cur);
	EQUALS_DBL(135.0, wx.wind.angle);
	EQUALS_DBL(1.4142135623730951, wx.wind.mag);
	EQUALS_DBL(135.0, gustAngle);
	EQUALS_FLT(1.4142135623730951f, wx.windGust);


	wx.wind.angle = 225.0;
	wx.wind.mag = 2.0;
	wx.windGust = 2.0f;

	cur.angle = 315.0;
	cur.mag = 2.0;

	gustAngle = WxUtils_adjustWindForCurrent(&wx, &cur);
	EQUALS_DBL(270.0, wx.wind.angle);
	EQUALS_DBL(2.8284271247461903, wx.wind.mag);
	EQUALS_DBL(270.0, gustAngle);
	EQUALS_FLT(2.8284271247461903f, wx.windGust);


	wx.wind.angle = 135.0;
	wx.wind.mag = 2.0;
	wx.windGust = 3.0f;

	cur.angle = 315.0;
	cur.mag = 2.0;

	gustAngle = WxUtils_adjustWindForCurrent(&wx, &cur);
	EQUALS_DBL(0.0, wx.wind.angle);
	EQUALS_DBL(0.0, wx.wind.mag);
	EQUALS_DBL(135.0, gustAngle);
	EQUALS_FLT(1.0f, wx.windGust);


	wx.wind.angle = 135.0;
	wx.wind.mag = 1.0;
	wx.windGust = 3.0f;

	gustAngle = WxUtils_adjustWindForCurrent(&wx, &cur);
	EQUALS_DBL(315.0, wx.wind.angle);
	EQUALS_DBL(1.0, wx.wind.mag);
	EQUALS_DBL(135.0, gustAngle);
	EQUALS_FLT(1.0f, wx.windGust);


	return 0;
}
