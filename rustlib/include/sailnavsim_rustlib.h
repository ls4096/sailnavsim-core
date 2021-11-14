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

#ifndef _sailnavsim_rustlib_h_
#define _sailnavsim_rustlib_h_

#include <stdint.h>


void* sailnavsim_rustlib_boatregistry_new(void);
void sailnavsim_rustlib_boatregistry_free(void* boat_registry);

int32_t sailnavsim_rustlib_boatregistry_group_add_boat(void* boat_registry, const char* group, const char* boat, const char* boat_altname);
void sailnavsim_rustlib_boatregistry_group_remove_boat(void* boat_registry, const char* group, const char* boat);

char* sailnavsim_rustlib_boatregistry_produce_group_membership_response(void* boat_registry, const char* group);
void sailnavsim_rustlib_boatregistry_free_group_membership_response(char* resp);


#endif // _sailnavsim_rustlib_h_
