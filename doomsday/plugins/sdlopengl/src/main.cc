/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009, 2010 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/deng.h>
#include "sdlopenglvideo.h"

extern "C"
{
    LIBDENG2_EXPORT const char* deng_LibraryType(void)
    {
        return "deng-plugin/video";
    }

    LIBDENG2_EXPORT void deng_InitializePlugin(void)
    {}

    LIBDENG2_EXPORT void deng_ShutdownPlugin(void)
    {}

    LIBDENG2_EXPORT de::Video* deng_NewVideo(void)
    {
        return new SDLOpenGLVideo();
    }
}
