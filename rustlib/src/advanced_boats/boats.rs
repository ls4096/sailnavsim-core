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

use super::types::Vec2;


// Some global constants

const WATER_DENSITY: f64 = 1_000.0; // kg/m^3
const AIR_DENSITY: f64 = 1.204; // kg/m^3


// Some of our "advanced boat" constants

const BOAT_AHEAD_WATER_AREA: f64 = 2.5; // m^2
const BOAT_AHEAD_WATER_DRAG_COEFFICIENT: f64 = 0.3;

const BOAT_ABEAM_WATER_AREA: f64 = 7.0; // m^2
const BOAT_ABEAM_WATER_DRAG_COEFFICIENT: f64 = 1.25;

const BOAT_AHEAD_AIR_AREA: f64 = 3.5; // m^2
const BOAT_AHEAD_AIR_DRAG_COEFFICIENT: f64 = 0.5;

const BOAT_ABEAM_AIR_AREA: f64 = 9.0; // m^2
const BOAT_ABEAM_AIR_DRAG_COEFFICIENT: f64 = 0.7;

// Extra hull exposed to the wind for every additional degree of heeling
const BOAT_ABEAM_AIR_AREA_EXTRA_PER_DEG_HEEL: f64 = 0.12; // m^2/deg

const BOAT_HEEL_RIGHTING_FORCE: f64 = 10_000.0;


pub fn calculate_boat_response(wind_vec: &Vec2, boat_vec: &Vec2, sail_area: f64) -> (Vec2, f64) {
    // Apparent wind vector
    let wind_vec_apparent = wind_vec.add(&boat_vec);

    // Sail force lookup
    let f_sail = get_f_sail(&wind_vec_apparent, sail_area);

    let heeling_angle = get_heeling_angle(&f_sail, sail_area);

    // Scale sail force based on heeling angle.
    // We need to scale by the square of the cosine:
    // - one factor for the sail being at an angle to the (horizontal) wind
    // - one factor for the sail's sideways force vector being angled downward from the horizon
    let ha_cos = heeling_angle.to_radians().cos();
    let f_sail = f_sail.scale(ha_cos * ha_cos);

    // Windage force calculations (through air)
    let wind_vec_force = wind_vec_apparent.rev();
    let f_air = Vec2::from_components(
        get_f(AIR_DENSITY, wind_vec_force.x(), BOAT_ABEAM_AIR_DRAG_COEFFICIENT, BOAT_ABEAM_AIR_AREA + BOAT_ABEAM_AIR_AREA_EXTRA_PER_DEG_HEEL * heeling_angle),
        get_f(AIR_DENSITY, wind_vec_force.y(), BOAT_AHEAD_AIR_DRAG_COEFFICIENT, BOAT_AHEAD_AIR_AREA));

    // Total aerodynamic force
    let f_aero = f_sail.add(&f_air);

    // Velocity is computed at the point where the aerodynamic forces and hydrodynamic forces balance each other.
    let v_x = get_v(f_aero.x(), WATER_DENSITY, BOAT_ABEAM_WATER_DRAG_COEFFICIENT, BOAT_ABEAM_WATER_AREA * heeling_angle.to_radians().cos());
    let v_y = get_v(f_aero.y(), WATER_DENSITY, BOAT_AHEAD_WATER_DRAG_COEFFICIENT, BOAT_AHEAD_WATER_AREA);

    // Take the average of old boat vector and new computed vector to make the transition "smoother".
    (Vec2::from_components((boat_vec.x() + v_x) / 2.0, (boat_vec.y() + v_y) / 2.0), heeling_angle)
}

// The relative force that the sail provides (F_lat: abeam, F_r: ahead) in ideal trim at certain apparent wind angles
const SAIL_RESPONSE_TABLE: [(f64, f64); 20] = [
    (0.0, -20.0),      // 0 deg
    (40.0, -10.0),     // 10
    (180.0, 40.0),     // 20
    (200.0, 120.0),    // 30
    (180.0, 160.0),    // 40
    (140.0, 180.0),    // 50
    (120.0, 200.0),    // 60
    (100.0, 210.0),    // 70
    (80.0, 220.0),     // 80
    (70.0, 230.0),     // 90
    (60.0, 240.0),     // 100
    (55.0, 250.0),     // 110
    (50.0, 255.0),     // 120
    (45.0, 260.0),     // 130
    (40.0, 260.0),     // 140
    (40.0, 255.0),     // 150
    (45.0, 230.0),     // 160
    (50.0, 200.0),     // 170
    (0.0, 150.0),      // 180
    (0.0, 0.0),        // ---
];

fn get_f_sail(wind_vec_apparent: &Vec2, sail_area: f64) -> Vec2 {
    let mut wind_angle = wind_vec_apparent.angle();
    let wind_mag = wind_vec_apparent.mag();
    let mut neg_x: bool = true;

    while wind_angle > 360.0 {
        wind_angle -= 360.0;
    }

    if wind_angle > 180.0 {
        wind_angle = 360.0 - wind_angle;
        neg_x = false;
    }

    let mut wind_angle_i = (wind_angle / 10.0) as i32;
    let frac: f64;
    if wind_angle_i < 0 {
        wind_angle_i = 0;
        frac = 0.0;
    } else if wind_angle_i >= 18 {
        wind_angle_i = 18;
        frac = 0.0;
    } else {
        frac = (wind_angle / 10.0) - (wind_angle_i as f64);
    }

    let (x0, y0) = SAIL_RESPONSE_TABLE[wind_angle_i as usize];
    let (x1, y1) = SAIL_RESPONSE_TABLE[(wind_angle_i + 1) as usize];

    let x = x0 * (1.0 - frac) + x1 * frac;
    let y = y0 * (1.0 - frac) + y1 * frac;

    let mut f_sail = Vec2::from_components(x, y);

    if neg_x {
        f_sail = f_sail.flip_x();
    }

    f_sail.scale(sail_area * wind_mag * wind_mag)
}

fn get_heeling_angle(f_sail: &Vec2, sail_area: f64) -> f64 {
    // Heeling angle is a function of the sail force component abeam and
    // the height of the center of sail force (sqrt of sail area as we are assuming a triangular sail).
    let f = f_sail.x().abs() * sail_area.sqrt();

    // A very rough approximation...
    // SailForce*cos(heel) = RightingForce*sin(heel) ==> heel = atan(SailForce / RightingForce)
    (f / BOAT_HEEL_RIGHTING_FORCE).atan().to_degrees()
}

fn get_f(d: f64, v: f64, c: f64, a: f64) -> f64 {
    match v >= 0.0 {
        true => 0.5 * d * v * v * c * a,
        false => -0.5 * d * v * v * c * a,
    }
}

fn get_v(f: f64, d: f64, c: f64, a: f64) -> f64 {
    match f >= 0.0 {
        true => (2.0 * f / (d * c * a)).sqrt(),
        false => -(-2.0 * f / (d * c * a)).sqrt(),
    }
}
