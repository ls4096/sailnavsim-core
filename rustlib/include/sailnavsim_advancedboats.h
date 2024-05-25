/**
 * Copyright (C) 2023-2024 ls4096 <ls4096@8bitbyte.ca>
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

#ifndef _sailnavsim_advancedboats_h_
#define _sailnavsim_advancedboats_h_

#include <stdint.h>


typedef struct {
	double wind_angle;
	double wind_speed;
	double boat_speed_ahead;
	double boat_speed_abeam;
	double sail_area;
} AdvancedBoatInputData;

typedef struct {
	double boat_speed_ahead;
	double boat_speed_abeam;
	double heeling_angle;
} AdvancedBoatOutputData;


int32_t sailnavsim_advancedboats_get_boat_type_count(void);

int32_t sailnavsim_advancedboats_boat_update_v(int32_t boat_type, const AdvancedBoatInputData* in_data, AdvancedBoatOutputData* out_data);

double sailnavsim_advancedboats_boat_course_change_rate(int32_t boat_type);

double sailnavsim_advancedboats_boat_wave_effect_resistance(int32_t boat_type);

double sailnavsim_advancedboats_boat_damage_wind_gust_threshold(int32_t boat_type);


#endif // _sailnavsim_advancedboats_h_
