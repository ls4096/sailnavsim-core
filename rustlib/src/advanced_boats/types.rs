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

use std::fmt;


const EPSILON: f64 = 0.00000001f64;


#[derive(Copy, Clone)]
pub struct Vec2 {
    x: f64,
    y: f64,
}


impl Vec2 {
    pub fn x(&self) -> f64 {
        self.x
    }

    pub fn y(&self) -> f64 {
        self.y
    }

    pub fn angle(&self) -> f64 {
        if self.y.abs() < EPSILON {
            // For very small y values, just return the angle of either west or east
            // (depending on the value of x).
            if self.x < -EPSILON {
                270.0
            } else if self.x > EPSILON {
                90.0
            } else {
                // For very small y and x values, just return the angle of 0.
                0.0
            }
        } else {
            // Normal angle calculation
            let a = (self.x / self.y).atan().to_degrees();
            if self.y < 0.0 {
                a + 180.0
            } else if self.x < 0.0 {
                a + 360.0
            } else {
                a
            }
        }
    }

    pub fn mag(&self) -> f64 {
        ((self.x * self.x) + (self.y * self.y)).sqrt()
    }

    pub fn add(mut self, other: &Vec2) -> Vec2 {
        self.x = self.x + other.x;
        self.y = self.y + other.y;
        self
    }

    pub fn scale(mut self, scalar: f64) -> Vec2 {
        self.x = self.x * scalar;
        self.y = self.y * scalar;
        self
    }

    pub fn rev(mut self) -> Vec2 {
        self.x = -self.x;
        self.y = -self.y;
        self
    }

    pub fn flip_x(mut self) -> Vec2 {
        self.x = -self.x;
        self
    }

    #[allow(dead_code)]
    pub fn flip_y(mut self) -> Vec2 {
        self.y = -self.y;
        self
    }

    pub fn from_angle_mag(angle: f64, mag: f64) -> Vec2 {
        let (angle, mag) = Vec2::normalize_angle_mag(angle, mag);

        Vec2 {
            x: mag * angle.to_radians().sin(),
            y: mag * angle.to_radians().cos(),
        }
    }

    pub fn from_components(x: f64, y: f64) -> Vec2 {
        Vec2 {
            x,
            y,
        }
    }

    fn normalize_angle_mag(angle: f64, mag: f64) -> (f64, f64) {
        let mut angle = angle;
        let mut mag = mag;

        if mag < 0.0 {
            angle = angle + 180.0;
            mag = -mag;
        }

        while angle < 0.0 {
            angle = angle + 360.0;
        }

        while angle > 360.0 {
            angle = angle - 360.0;
        }

        (angle, mag)
    }
}

impl PartialEq for Vec2 {
    fn eq(&self, other: &Self) -> bool {
        (self.x - other.x).abs() < EPSILON && (self.y - other.y).abs() < EPSILON
    }
}

impl fmt::Debug for Vec2 {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Vec2")
            .field("x", &self.x)
            .field("y", &self.y)
            .field("angle()", &self.angle())
            .field("mag()", &self.mag())
            .finish()
    }
}


#[cfg(test)]
mod tests {
    use super::*;

    fn eq_f64(a: f64, b: f64) -> bool {
        (a - b).abs() < EPSILON
    }

    #[test]
    fn basic() {
        let default_mag = 2.0f64;

        let zero_n = Vec2::from_angle_mag(0.0, 0.0);
        let zero_e = Vec2::from_angle_mag(90.0, 0.0);
        let zero_s = Vec2::from_angle_mag(180.0, 0.0);
        let zero_w = Vec2::from_angle_mag(270.0, 0.0);

        assert_eq!(zero_n, zero_n);
        assert_eq!(zero_e, zero_n);
        assert_eq!(zero_s, zero_n);
        assert_eq!(zero_w, zero_n);

        let north = Vec2::from_angle_mag(0.0, default_mag);
        let east = Vec2::from_angle_mag(90.0, default_mag);
        let south = Vec2::from_angle_mag(180.0, default_mag);
        let west = Vec2::from_angle_mag(270.0, default_mag);

        assert!(eq_f64(north.x(), 0.0));
        assert!(eq_f64(north.y(), default_mag));

        assert!(eq_f64(east.x(), default_mag));
        assert!(eq_f64(east.y(), 0.0));

        assert!(eq_f64(south.x(), 0.0));
        assert!(eq_f64(south.y(), -default_mag));

        assert!(eq_f64(west.x(), -default_mag));
        assert!(eq_f64(west.y(), 0.0));

        assert_eq!(north, north);
        assert_eq!(east, east);
        assert_eq!(south, south);
        assert_eq!(west, west);

        assert_eq!(north.rev(), south);
        assert_eq!(east.rev(), west);
        assert_eq!(south.rev(), north);
        assert_eq!(west.rev(), east);

        assert_eq!(north.flip_x(), north);
        assert_eq!(north.flip_y(), south);

        assert_eq!(east.flip_x(), west);
        assert_eq!(east.flip_y(), east);

        assert_eq!(south.flip_x(), south);
        assert_eq!(south.flip_y(), north);

        assert_eq!(west.flip_x(), east);
        assert_eq!(west.flip_y(), west);
    }

    #[test]
    fn param() {
        let zero_vec = Vec2::from_angle_mag(0.0, 0.0);

        // Zero vector added to zero vector should not change.
        let zero_vec_copy1 = zero_vec.clone();
        assert_eq!(zero_vec_copy1.add(&zero_vec), zero_vec);
        let zero_vec_copy2 = zero_vec;
        assert_eq!(zero_vec_copy2.add(&zero_vec), zero_vec);

        let mut a = -1080.0f64;
        let mut m = 0.001f64;
        while a < 1080.0 {
            while m < 1000000.0 {
                let v = Vec2::from_angle_mag(a, m);

                let (a_norm, m_norm) = Vec2::normalize_angle_mag(a, m);
                assert!(a_norm >= 0.0);
                assert!(a_norm <= 360.0);
                assert!(m_norm >= 0.0);

                assert!(eq_f64(a_norm, v.angle()));
                assert!(eq_f64(m_norm, v.mag()));

                // Vector added to its reverse should result in zero vector.
                assert_eq!(v.rev().add(&v), zero_vec);

                // Zero vector added to non-zero vector should not change non-zero vector.
                let v_copy1 = v.clone();
                assert_eq!(v_copy1.add(&zero_vec), v);
                let v_copy2 = v;
                assert_eq!(v_copy2.add(&zero_vec), v);

                m = m * 1.37;
            }

            a = a + 0.11;
        }


        // Now do the same but with negative magnitudes provided.
        a = -1080.0f64;
        m = -0.001f64;
        while a < 1080.0 {
            while m > -1000000.0 {
                let v = Vec2::from_angle_mag(a, m);

                let (a_norm, m_norm) = Vec2::normalize_angle_mag(a, m);
                assert!(a_norm >= 0.0);
                assert!(a_norm <= 360.0);
                assert!(m_norm >= 0.0);

                assert!(eq_f64(a_norm, v.angle()));
                assert!(eq_f64(m_norm, v.mag()));

                // Vector added to its reverse should result in zero vector.
                assert_eq!(v.rev().add(&v), zero_vec);

                // Zero vector added to non-zero vector should not change non-zero vector.
                let v_copy1 = v.clone();
                assert_eq!(v_copy1.add(&zero_vec), v);
                let v_copy2 = v;
                assert_eq!(v_copy2.add(&zero_vec), v);

                m = m * 1.37;
            }

            a = a + 0.11;
        }
    }

    #[test]
    fn new_dirs_from_add() {
        let default_mag = 1.0f64;

        let north = Vec2::from_angle_mag(0.0, default_mag);
        let east = Vec2::from_angle_mag(90.0, default_mag);
        let south = Vec2::from_angle_mag(180.0, default_mag);
        let west = Vec2::from_angle_mag(270.0, default_mag);

        let ne = north.add(&east);
        let se = south.add(&east);
        let sw = south.add(&west);
        let nw = north.add(&west);

        // Check angles.
        assert!(eq_f64(45.0, ne.angle()));
        assert!(eq_f64(135.0, se.angle()));
        assert!(eq_f64(225.0, sw.angle()));
        assert!(eq_f64(315.0, nw.angle()));

        // Check magnitudes.
        assert!(eq_f64((2.0f64).sqrt(), ne.mag()));
        assert!(eq_f64((2.0f64).sqrt(), se.mag()));
        assert!(eq_f64((2.0f64).sqrt(), sw.mag()));
        assert!(eq_f64((2.0f64).sqrt(), nw.mag()));

        // Add the new direction angles to get cardinal direction-pointing vectors, and check.
        assert_eq!(nw.add(&ne), north.scale(2.0));
        assert_eq!(sw.add(&se), south.scale(2.0));
        assert_eq!(ne.add(&se), east.scale(2.0));
        assert_eq!(nw.add(&sw), west.scale(2.0));

        // Check opposites.
        assert_eq!(ne.rev(), sw);
        assert_eq!(se.rev(), nw);
        assert_eq!(sw.rev(), ne);
        assert_eq!(nw.rev(), se);

        // Check flips.
        assert_eq!(ne.flip_x(), nw);
        assert_eq!(ne.flip_y(), se);
        assert_eq!(se.flip_x(), sw);
        assert_eq!(se.flip_y(), ne);
        assert_eq!(sw.flip_x(), se);
        assert_eq!(sw.flip_y(), nw);
        assert_eq!(nw.flip_x(), ne);
        assert_eq!(nw.flip_y(), sw);
    }

    #[test]
    fn normalize_angle_mag() {
        let mut a = -1080.0f64;
        let mut m = 0.001f64;

        while a < 1080.0 {
            while m < 1000000.0 {
                let (a_norm, m_norm) = Vec2::normalize_angle_mag(a, m);

                let v0 = Vec2::from_angle_mag(a, m);
                let v1 = Vec2::from_angle_mag(a_norm, m_norm);
                let v2 = Vec2::from_angle_mag(a - 360.0, m);
                let v3 = Vec2::from_angle_mag(a + 360.0, m);
                let v4 = Vec2::from_angle_mag(a - 180.0, -m);
                let v5 = Vec2::from_angle_mag(a + 180.0, -m);

                assert_eq!(v0, v1);
                assert_eq!(v0, v2);
                assert_eq!(v0, v3);
                assert_eq!(v0, v4);
                assert_eq!(v0, v5);

                m = m * 1.37;
            }

            a = a + 0.11;
        }
    }

    #[test]
    fn operations() {
        let mut a0 = -1080.0f64;
        let mut m0 = 0.001f64;
        let mut a1 = -360.0f64;
        let mut m1 = 0.1f64;

        while a0 < 1080.0 {
            while m0 < 1000000.0 {
                while a1 < 360.0 {
                    while m1 < 1000000.0 {
                        let v0 = Vec2::from_angle_mag(a0, m0);
                        let v1 = Vec2::from_angle_mag(a1, m1);

                        // Vector addition must be commutative.
                        assert_eq!(v0.add(&v1), v1.add(&v0));

                        // Scaling by x and 1/x must result in original vector.
                        assert_eq!(v0.scale(2.5).scale(1.0 / 2.5), v0);

                        // Double-reversing must result in the original vector.
                        assert_eq!(v0.rev().rev(), v0);

                        // Double-flipping must result in the original vector.
                        assert_eq!(v0.flip_x().flip_x(), v0);
                        assert_eq!(v0.flip_y().flip_y(), v0);

                        m1 = m1 * 1.37;
                    }
                    a1 = a1 + 0.11;
                }
                m0 = m0 * 1.37;
            }
            a0 = a0 + 0.11;
        }
    }

    #[test]
    fn compare_from_angle_mag_with_components() {
        assert_eq!(Vec2::from_angle_mag(0.0, 0.0), Vec2::from_components(0.0, 0.0));
        assert_eq!(Vec2::from_angle_mag(123.0, 0.0), Vec2::from_components(0.0, 0.0));

        assert_eq!(Vec2::from_angle_mag(0.0, 1.0), Vec2::from_components(0.0, 1.0));
        assert_eq!(Vec2::from_angle_mag(360.0, 1.0), Vec2::from_components(0.0, 1.0));
        assert_eq!(Vec2::from_angle_mag(90.0, 1.0), Vec2::from_components(1.0, 0.0));
        assert_eq!(Vec2::from_angle_mag(180.0, 1.5), Vec2::from_components(0.0, -1.5));
        assert_eq!(Vec2::from_angle_mag(270.0, 2.5), Vec2::from_components(-2.5, 0.0));
    }
}
