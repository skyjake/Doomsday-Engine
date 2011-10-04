/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_C_WRAPPER_H
#define LIBDENG2_C_WRAPPER_H

#include "libdeng2.h"

/**
 * @file Defines the C wrapper API for libdeng2 classes. Legacy code can use
 * this wrapper API to access libdeng2 functionality. Note that the identifiers
 * in this file are not in the de namespace.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * LegacyCore
 */
DENG2_OPAQUE(LegacyCore)

DENG2_PUBLIC LegacyCore* LegacyCore_New(int* argc, char** argv);
DENG2_PUBLIC int         LegacyCore_RunEventLoop(LegacyCore* lc, void (*loopFunc)(void));
DENG2_PUBLIC void        LegacyCore_Stop(LegacyCore* lc, int exitCode);
DENG2_PUBLIC void        LegacyCore_Delete(LegacyCore* lc);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG2_C_WRAPPER_H
