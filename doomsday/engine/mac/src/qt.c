/**\file qt.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2011 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * QuickTime implementation for Ext and Mus interfaces
 *
 * This is only used on the Mac OS platform.
 */

#if 0

// HEADER FILES ------------------------------------------------------------

#ifdef MACOS_10_4
#include <stdint.h>
typedef uint64_t io_user_reference_t;
#endif

#include "doomsday.h"
#include "sys_audiod.h"
#include "sys_audiod_mus.h"
#include <Carbon/Carbon.h>
#include <QuickTime/Movies.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

static int DM_Ext_PlayFile(const char *filename, int looped);
static void DM_Ext_Stop(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean qtInited = false;
static boolean inLoopedMode = false;
//static unsigned songSize = 0;
//static char *song;
static Movie movie = NULL;
static short movieVolume = kFullVolume;

// CODE --------------------------------------------------------------------

static void DS_Error(void)
{
    Con_Message("DS_Error: Error playing music with QuickTime.\n");
}

static void ExtMus_Init(void)
{
    if(qtInited)
        return;

    Con_Message("  Initializing QuickTime.\n");

    // Initialize QuickTime.
    EnterMovies();

    qtInited = true;
}

static int DM_Ext_Init(void)
{
    ExtMus_Init();
    return qtInited;
}

static void DM_Ext_Update(void)
{
    if(!qtInited || movie == NULL)
        return;

    MoviesTask(movie, 0);

    if(IsMovieDone(movie) && inLoopedMode)
    {
        GoToBeginningOfMovie(movie);
        StartMovie(movie);
    }
}

static void DM_Ext_Set(int property, float value)
{
    if(!qtInited)
        return;

    switch (property)
    {
    case MUSIP_VOLUME:
        if(value > 1) value = 1;
        if(value < 0) value = 0;
        movieVolume = (short) (0x100 * value);
        if(movie)
        {
            // Update the volume of the running movie.
            SetMovieVolume(movie, movieVolume);
        }
        break;
    }
}

static int DM_Ext_Get(int property, void *value)
{
    if(!qtInited)
        return false;

    switch (property)
    {
    case MUSIP_ID:
        strcpy(value, "QuickTime::Ext");
        break;

    default:
        return false;
    }
    return true;
}

static void DM_Ext_Pause(int pause)
{
    if(!qtInited || movie == NULL)
        return;

    if(pause)
        StopMovie(movie);
    else
        StartMovie(movie);
}

static void DM_Ext_Stop(void)
{
    if(!qtInited || movie == NULL)
        return;

    StopMovie(movie);
}

static int playFile(const char *filename, int looped)
{
    OSErr error = noErr;
    CFStringRef pathStr;
    CFStringRef escapedStr;
    CFURLRef url;
    FSRef fsRef;
    FSSpec fsSpec;
    short refNum;

    if(!qtInited)
        return false;

    // Free any previously loaded music.
    if(movie)
    {
        DisposeMovie(movie);
        movie = NULL;
    }

    // Now we'll open the file using Carbon and QuickTime.
    pathStr = CFStringCreateWithCString(NULL, filename,
        CFStringGetSystemEncoding());
    escapedStr = CFURLCreateStringByAddingPercentEscapes(
        NULL, pathStr, NULL, CFSTR(""), kCFStringEncodingUTF8);
    url = CFURLCreateWithString(NULL, escapedStr, NULL);
    CFRelease(pathStr);
    CFRelease(escapedStr);

    // We've got the URL, get the FSSpec.
    if(!CFURLGetFSRef(url, &fsRef))
    {
        // File does not exist??
        if(url) CFRelease(url);
        DS_Error();
        return false;
    }
    CFRelease(url);
    url = NULL;
    if(FSGetCatalogInfo(&fsRef, kFSCatInfoNone, NULL, NULL, &fsSpec, NULL)
        != noErr)
    {
        DS_Error();
        return false;
    }

    // Open the 'movie' from the specified file.
    if(OpenMovieFile(&fsSpec, &refNum, fsRdPerm) != noErr)
    {
        DS_Error();
        return false;
    }
    error = NewMovieFromFile(&movie, refNum, NULL, NULL,
        newMovieActive & newMovieDontAskUnresolvedDataRefs, NULL);
    CloseMovieFile(refNum);
    if(error != noErr)
    {
        DS_Error();
        return false;
    }

    GoToBeginningOfMovie(movie);
    StartMovie(movie);
    SetMovieVolume(movie, movieVolume);

    inLoopedMode = (looped != 0);
    return true;
}

static int DM_Ext_PlayFile(const char *filename, int looped)
{
    return playFile(filename, looped);
}

// The audio driver struct.
audiointerface_music_t audiodQuickTimeMusic = { {
    DM_Ext_Init,
    DM_Ext_Update,
    DM_Ext_Set,
    DM_Ext_Get,
    DM_Ext_Pause,
    DM_Ext_Stop },
    NULL,
    NULL,
    DM_Ext_PlayFile,
};

#endif
