/*
 * The Doomsday Engine Project -- Supporting code
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef SUPPORT_DENGMAIN_H
#define SUPPORT_DENGMAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(WIN32)
    // Called from SDL.
#   define deng_Main SDL_main
#endif

#if defined(UNIX) && !defined(MACOSX)
#   define deng_Main main
#endif

/**
 * Main entry point for a libdeng2 application.
 */
int deng_Main(int argc, char** argv);

#ifdef __cplusplus
}
#endif

#endif /* SUPPORT_DENGMAIN_H */
