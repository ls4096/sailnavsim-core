/**
 * Copyright (C) 2021 ls4096 <ls4096@8bitbyte.ca>
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

use std::collections::HashMap;
use std::collections::HashSet;

pub struct BoatRegistry {
    boat_groups: HashMap<String, HashSet<String>>
}

impl BoatRegistry {
    pub fn new() -> BoatRegistry {
        BoatRegistry {
            boat_groups: HashMap::new()
        }
    }

    pub fn add_boat_to_group(&mut self, group: String, boat: String) -> bool {
        match self.boat_groups.get_mut(&group) {
            Some(boat_set) => {
                boat_set.insert(boat)
            }
            None => {
                let mut boat_set = HashSet::new();
                boat_set.insert(boat);
                self.boat_groups.insert(group, boat_set);
                true
            }
        }
    }

    pub fn remove_boat_from_group(&mut self, group: &String, boat: &String) {
        match self.boat_groups.get_mut(group) {
            Some(boat_set) => {
                boat_set.remove(boat);
                if boat_set.len() == 0 {
                    self.boat_groups.remove(group);
                }
            }
            None => {
                // Group not found, so nothing to do.
            }
        }
    }

    pub fn produce_group_membership_response(&self, group: &String) -> String {
        let mut resp = String::from("");
        match self.boat_groups.get(group) {
            Some(boats) => {
                boats.iter().for_each(|b| {
                    resp.push_str(b);
                    resp.push_str("\n");
                });
            },
            None => {
                // Group not found, so nothing to do.
            }
        }
        resp
    }
}
