/**
 * Copyright (C) 2021-2022 ls4096 <ls4096@8bitbyte.ca>
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

use std::collections::{ HashMap, hash_map };

use std::os::raw::c_void;

pub struct BoatRegistry {
    boats: HashMap<String, *mut c_void>,
    boat_groups: HashMap<String, HashMap<String, Option<String>>>,
}

pub struct BoatRegistryIter<'a> {
    iter: hash_map::Iter<'a, String, *mut c_void>,
    count: usize,
    at: usize,
}

impl BoatRegistryIter<'_> {
    pub fn has_next(&self) -> bool {
        self.at < self.count
    }

    pub fn next(&mut self) -> *mut c_void {
        self.at += 1;
        match self.iter.next() {
            Some(b) => *(b.1),
            None => 0 as *mut c_void,
        }
    }

    pub fn count(&self) -> usize {
        self.count
    }
}

impl BoatRegistry {
    pub fn new() -> BoatRegistry {
        BoatRegistry {
            boats: HashMap::new(),
            boat_groups: HashMap::new(),
        }
    }

    pub fn add_boat(&mut self, boat_entry: *mut c_void, boat_name: String) -> bool {
        match self.boats.get(&boat_name) {
            Some(_) => false,
            None => {
                self.boats.insert(boat_name, boat_entry);
                true
            }
        }
    }

    pub fn get_boat(&mut self, boat_name: &String) -> *mut c_void {
        match self.boats.get_mut(boat_name) {
            Some(b) => *b,
            None => 0 as *mut c_void,
        }
    }

    pub fn remove_boat(&mut self, boat_name: &String) -> *mut c_void {
        match self.boats.remove(boat_name) {
            Some(b) => b,
            None => 0 as *mut c_void,
        }
    }

    pub fn get_boats_iterator(&self) -> BoatRegistryIter {
        BoatRegistryIter {
            iter: self.boats.iter(),
            count: self.boats.len(),
            at: 0,
        }
    }


    pub fn add_boat_to_group(&mut self, group: String, boat: String, boat_altname: Option<String>) -> bool {
        match self.boat_groups.get_mut(&group) {
            Some(boat_group) => {
                match boat_group.insert(boat, boat_altname) {
                    Some(_) => false,
                    None => true,
                }
            }
            None => {
                let mut boat_group = HashMap::new();
                boat_group.insert(boat, boat_altname);
                self.boat_groups.insert(group, boat_group);
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
                for (boat, altname) in boats.iter() {
                    resp.push_str(boat);
                    resp.push_str(",");
                    resp.push_str(match altname {
                        Some(an) => an,
                        None => "!",
                    });
                    resp.push_str("\n");
                }
            },
            None => {
                // Group not found, so nothing to do.
            }
        }
        resp
    }
}
