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

mod boat_registry;

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_void};

use boat_registry::BoatRegistry;

#[no_mangle]
pub extern fn sailnavsim_rustlib_boatregistry_new() -> *mut c_void {
    let br = Box::new(BoatRegistry::new());
    let ptr = Box::into_raw(br);
    ptr as *mut c_void
}

#[no_mangle]
pub unsafe extern fn sailnavsim_rustlib_boatregistry_free(ptr_raw: *mut c_void) {
    let _ptr = Box::from_raw(ptr_raw as *mut BoatRegistry);
}

#[no_mangle]
pub extern fn sailnavsim_rustlib_boatregistry_group_add_boat(boat_registry_raw: *mut c_void, group_raw: *const c_char, boat_raw: *const c_char) -> i32 {
    let mut boat_registry = unsafe {
        Box::from_raw(boat_registry_raw as *mut BoatRegistry)
    };

    let group = unsafe {
        match CStr::from_ptr(group_raw).to_str() {
            Ok(s) => String::from(s),
            Err(_) => {
                return -1;
            }
        }
    };

    let boat = unsafe {
        match CStr::from_ptr(boat_raw).to_str() {
            Ok(s) => String::from(s),
            Err(_) => {
                return -2;
            }
        }
    };

    let result = match boat_registry.add_boat_to_group(group, boat) {
        true => 0,
        false => 1,
    };

    Box::into_raw(boat_registry);
    result
}

#[no_mangle]
pub extern fn sailnavsim_rustlib_boatregistry_group_remove_boat(boat_registry_raw: *mut c_void, group_raw: *const c_char, boat_raw: *const c_char) {
    let mut boat_registry = unsafe {
        Box::from_raw(boat_registry_raw as *mut BoatRegistry)
    };

    let group = unsafe {
        match CStr::from_ptr(group_raw).to_str() {
            Ok(s) => String::from(s),
            Err(_) => {
                return;
            }
        }
    };

    let boat = unsafe {
        match CStr::from_ptr(boat_raw).to_str() {
            Ok(s) => String::from(s),
            Err(_) => {
                return;
            }
        }
    };

    boat_registry.remove_boat_from_group(&group, &boat);

    Box::into_raw(boat_registry);
}

#[no_mangle]
pub extern fn sailnavsim_rustlib_boatregistry_produce_group_membership_response(boat_registry_raw: *mut c_void, group_raw: *const c_char) -> *mut c_char {
    let boat_registry = unsafe {
        Box::from_raw(boat_registry_raw as *mut BoatRegistry)
    };

    let group = unsafe {
        match CStr::from_ptr(group_raw).to_str() {
            Ok(s) => String::from(s),
            Err(_) => String::from(""),
        }
    };

    let resp = match CString::new(boat_registry.produce_group_membership_response(&group)) {
        Ok(cs) => cs.into_raw(),
        Err(_) => 0 as *mut c_char
    };

    Box::into_raw(boat_registry);
    resp
}

#[no_mangle]
pub unsafe extern fn sailnavsim_rustlib_boatregistry_free_group_membership_response(resp: *mut c_char) {
    let _to_free = CString::from_raw(resp);
}
