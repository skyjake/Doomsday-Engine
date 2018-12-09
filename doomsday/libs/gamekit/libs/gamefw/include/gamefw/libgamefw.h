/** @file libgamefw.h  Common framework for games.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGAMEFW_H
#define LIBGAMEFW_H

#include <de/legacy/types.h>

/*
 * The LIBGAMEFW_PUBLIC macro is used for declaring exported symbols. It must be
 * applied in all exported classes and functions. DEF files are not used for
 * exporting symbols on Windows.
 */
#if defined(_WIN32) && defined(_MSC_VER)
#  ifdef __LIBGAMEFW__
// This is defined when compiling the library.
#    define LIBGAMEFW_PUBLIC __declspec(dllexport)
#  else
#    define LIBGAMEFW_PUBLIC __declspec(dllimport)
#  endif
#else
#  define LIBGAMEFW_PUBLIC  DE_PUBLIC
#endif

typedef enum gfw_game_id_e {
    GFW_DOOM,
    GFW_HERETIC,
    GFW_HEXEN,
    GFW_DOOM64,
    GFW_STRIFE,
    GFW_GAME_ID_COUNT
} gfw_game_id_t;

// Color indices.
enum { CR, CG, CB, CA };

// The Base API is required when using these defines:
#define GAMETIC             (*((timespan_t*) DD_GetVariable(DD_GAMETIC)))
#define IS_SERVER           (DD_GetInteger(DD_SERVER))
#define IS_CLIENT           (DD_GetInteger(DD_CLIENT))
#define IS_NETGAME          (DD_GetInteger(DD_NETGAME))
#define IS_DEDICATED        (DD_GetInteger(DD_NOVIDEO))
#define CONSOLEPLAYER       (DD_GetInteger(DD_CONSOLEPLAYER))
#define DISPLAYPLAYER       (DD_GetInteger(DD_DISPLAYPLAYER))

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initializes the GameKit library. Must be called before any other function
 * in the library is called.
 */
//LIBGAMEFW_PUBLIC void gfw_Initialize();

/**
 * Sets the current game.
 *
 * The current game setting can affect the behavior of some operations. This is
 * particularly useful when vanilla-compatible game-specific behavior is needed.
 *
 * @note When refactoring code and moving it into libgamefw, this game enum
 * should be used to replace the old __JDOOM__ etc. defines.
 *
 * @param game  Current game.
 */
LIBGAMEFW_PUBLIC void gfw_SetCurrentGame(gfw_game_id_t game);

LIBGAMEFW_PUBLIC gfw_game_id_t gfw_CurrentGame();

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

/// libgamefw uses the @c gfw namespace for all its C++ symbols.
namespace gfw {

typedef gfw_game_id_t GameId;

} // namespace gfw

#endif // __cplusplus

#endif // LIBGAMEFW_H
