/**
 * Copyright (C) 2020-2024 ls4096 <ls4096@8bitbyte.ca>
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

#ifndef _BoatWindResponse_h_
#define _BoatWindResponse_h_

#include <stdbool.h>

int BoatWindResponse_init();

bool BoatWindResponse_isBoatTypeBasic(int boatType);

bool BoatWindResponse_isBoatTypeAdvanced(int boatType);

int BoatWindResponse_adjustBoatTypeForAdvanced(int boatType);

double BoatWindResponse_getBoatSpeed(double windSpd, double angleFromWind, int boatType);

double BoatWindResponse_getCourseChangeRate(int boatType);

double BoatWindResponse_getSpeedChangeResponse(int boatType);

double BoatWindResponse_getWaveEffectResistance(int boatType);

double BoatWindResponse_getDamageWindGustThreshold(int boatType);

#endif // _BoatWindResponse_h_
