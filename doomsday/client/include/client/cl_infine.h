/** @file
 *
 * @authors Copyright © 2010 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

/**
 * Client-side InFine.
 */

#ifndef LIBDENG_CLIENT_INFINE
#define LIBDENG_CLIENT_INFINE

#include <de/reader.h>

#ifdef __cplusplus
extern "C" {
#endif

finaleid_t Cl_CurrentFinale(void);

/**
 * This is where clients start their InFine sequences.
 */
void Cl_Finale(Reader* msg);

/**
 * Client sends a request to skip the finale.
 */
void Cl_RequestFinaleSkip(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_CLIENT_INFINE */
