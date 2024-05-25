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

mod types;
mod boats;

use types::Vec2;


const KTS_IN_MPS: f64 = 1.943844;


#[repr(C)]
pub struct AdvancedBoatInputData {
    wind_angle: f64,
    wind_speed: f64,
    boat_speed_ahead: f64,
    boat_speed_abeam: f64,
    sail_area: f64,
}

#[repr(C)]
pub struct AdvancedBoatOutputData {
    boat_speed_ahead: f64,
    boat_speed_abeam: f64,
    heeling_angle: f64,
}


#[no_mangle]
pub extern fn sailnavsim_advancedboats_get_boat_type_count() -> i32 {
    1
}

#[no_mangle]
pub extern fn sailnavsim_advancedboats_boat_update_v(boat_type: i32, in_data_raw: *const AdvancedBoatInputData, out_data_raw: *mut AdvancedBoatOutputData) -> i32 {
    let in_data = unsafe { &(*in_data_raw) };

    let wind_vec = Vec2::from_angle_mag(in_data.wind_angle, in_data.wind_speed);

    let (boat_vec_out, heeling_angle) = match boat_type {
        0 => {
            let bv = Vec2::from_components(in_data.boat_speed_abeam, in_data.boat_speed_ahead);
            boats::calculate_boat_response(&wind_vec, &bv, in_data.sail_area)
        },
        _ => { return -1; }, // Failure
    };

    unsafe {
        (*out_data_raw).boat_speed_ahead = boat_vec_out.y();
        (*out_data_raw).boat_speed_abeam = boat_vec_out.x();
        (*out_data_raw).heeling_angle = heeling_angle;
    }

    0 // Success
}

#[no_mangle]
pub extern fn sailnavsim_advancedboats_boat_course_change_rate(boat_type: i32) -> f64 {
    match boat_type {
        0 => 5.0,
        _ => 0.0, // Any boat type that isn't modeled always just gets a zero rate.
    }
}

#[no_mangle]
pub extern fn sailnavsim_advancedboats_boat_wave_effect_resistance(boat_type: i32) -> f64 {
    match boat_type {
        0 => 75.0,
        _ => 0.001, // Any boat type that isn't modeled just has very low wave resistance.
    }
}

#[no_mangle]
pub extern fn sailnavsim_advancedboats_boat_damage_wind_gust_threshold(boat_type: i32) -> f64 {
    match boat_type {
        0 => 45.0 / KTS_IN_MPS,
        _ => 0.001, // Any boat type that isn't modeled just has very low wind gust damage threshold.
    }
}
