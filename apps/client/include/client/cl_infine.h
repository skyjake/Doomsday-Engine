/** @file cl_infine.h  Clientside InFine.
 *
 * @authors Copyright © 2010-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_INFINE
#define CLIENT_INFINE

#include "api_infine.h"
#include <de/legacy/reader.h>

finaleid_t Cl_CurrentFinale();

/**
 * This is where clients start their InFine sequences.
 */
void Cl_Finale(Reader1 *msg);

/**
 * Client sends a request to skip the finale.
 */
void Cl_RequestFinaleSkip();

#endif  // CLIENT_INFINE
