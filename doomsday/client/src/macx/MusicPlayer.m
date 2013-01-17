/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#import "MusicPlayer.h"

/**
 * QuickTime music player for Mac OS X 10.4 or newer. Uses the Cocoa QTKit.
 */

static MusicPlayer* musicPlayer;

@implementation MusicPlayer

- (id)init
{
    currentSong = 0;
    songVolume = 1.f;
    return self;
}

- (void)shutdown
{
    if(currentSong)
    {
        [currentSong stop];
        [currentSong release];
        currentSong = 0;
    }
}

- (int)playFile:(const char*)filename looping:(int)loop
{
    // Get rid of the old song?
    if(currentSong)
    {
        [currentSong stop];
        [currentSong release];
        currentSong = 0;
    }

    NSString* path = [[[NSString alloc] initWithCString:filename encoding:[NSString defaultCStringEncoding]] autorelease];
    currentSong = [[QTMovie alloc] initWithFile:path error:0];
    if(!currentSong) return false;

    [currentSong setVolume:songVolume];

    // Set the looping attribute.
    [currentSong setAttribute:[NSNumber numberWithBool:(loop? YES : NO)] forKey:QTMovieLoopsAttribute];

    // Start playing.
    [currentSong play];
    
    return true;
}

- (void)setVolume:(float)volume
{
    songVolume = volume;
    if(currentSong)
    {
        [currentSong setVolume:songVolume];
    }
}

- (void)play
{
    if(currentSong)
    {
        [currentSong play];
    }
}

- (void)stop
{
    if(currentSong)
    {
        [currentSong stop];
    }
}

- (void)rewind
{
    if(currentSong)
    {
        [currentSong gotoBeginning];
    }
}

@end

// --------------------------------------------------------------------------

#include "api_audiod.h"
#include "api_audiod_mus.h"

static int DM_Ext_Init(void)
{
    // Already initialized?
    if(musicPlayer) return true;

    // Create a new QTKit player.
    musicPlayer = [[MusicPlayer alloc] init];

    return true;
}

static void DM_Ext_Shutdown(void)
{
    if(!musicPlayer) return;

    // Release the player.
    [musicPlayer shutdown];
    [musicPlayer release];
    musicPlayer = 0;
}

static void DM_Ext_Update(void)
{
    if(!musicPlayer) return;

    // Gets called on every frame.
}

static void DM_Ext_Set(int property, float value)
{
    if(!musicPlayer)
        return;

    switch (property)
    {
    case MUSIP_VOLUME:
        if(value > 1) value = 1;
        if(value < 0) value = 0;
        [musicPlayer setVolume:value];
        break;
    }
}

static int DM_Ext_Get(int property, void *value)
{
    if(!musicPlayer)
        return false;

    switch (property)
    {
    case MUSIP_ID:
        strcpy(value, "QuickTime(Cocoa)/Ext");
        break;

    default:
        return false;
    }
    return true;
}

static void DM_Ext_Pause(int pause)
{
    if(!musicPlayer) return;

    if(pause)
    {
        [musicPlayer stop];
    }
    else
    {
        [musicPlayer play];
    }
}

static void DM_Ext_Stop(void)
{
    if(!musicPlayer) return;

    [musicPlayer stop];
    [musicPlayer rewind];
}

static int DM_Ext_PlayFile(const char *filename, int looped)
{
    if(!musicPlayer) return 0;

    return [musicPlayer playFile:filename looping:looped];
}

// The audio driver struct.
audiointerface_music_t audiodQuickTimeMusic = { {
    DM_Ext_Init,
    DM_Ext_Shutdown,
    DM_Ext_Update,
    DM_Ext_Set,
    DM_Ext_Get,
    DM_Ext_Pause,
    DM_Ext_Stop },
    NULL,
    NULL,
    DM_Ext_PlayFile,
};

