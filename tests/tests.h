/**
 * Copyright (C) 2020-2023 ls4096 <ls4096@8bitbyte.ca>
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

#ifndef _tests_h_
#define _tests_h_

int test_BoatRegistry_runBasic();
int test_BoatRegistry_runBasicWithGroups();
int test_BoatRegistry_runLoad();
int test_BoatRegistry_runLoadWithBigGroups();

int test_WxUtils();

#endif // _tests_h_
