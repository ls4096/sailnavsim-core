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

#ifndef _sailnavsim_rustlib_h_
#define _sailnavsim_rustlib_h_

#include <stdint.h>


// Lifecycle

void* sailnavsim_rustlib_boatregistry_new(void);
void sailnavsim_rustlib_boatregistry_free(void* boat_registry);


// General boat registry operations

int32_t sailnavsim_rustlib_boatregistry_add_boat_entry(void* boat_registry, void* boat_entry, const char* name);
void* sailnavsim_rustlib_boatregistry_get_boat_entry(void* boat_registry, const char* name);
void* sailnavsim_rustlib_boatregistry_remove_boat_entry(void* boat_registry, const char* name);

void* sailnavsim_rustlib_boatregistry_get_boats_iterator(void* boat_registry, uint32_t* boat_count);
void* sailnavsim_rustlib_boatregistry_boats_iterator_get_next(void* iterator);
int32_t sailnavsim_rustlib_boatregistry_boats_iterator_has_next(void* iterator);
void* sailnavsim_rustlib_boatregistry_free_boats_iterator(void* iterator);


// Boat groups

int32_t sailnavsim_rustlib_boatregistry_group_add_boat(void* boat_registry, const char* group, const char* boat, const char* boat_altname);
void sailnavsim_rustlib_boatregistry_group_remove_boat(void* boat_registry, const char* group, const char* boat);

char* sailnavsim_rustlib_boatregistry_produce_group_membership_response(void* boat_registry, const char* group);
void sailnavsim_rustlib_boatregistry_free_group_membership_response(char* resp);


#endif // _sailnavsim_rustlib_h_
