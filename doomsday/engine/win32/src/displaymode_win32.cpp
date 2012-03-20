/**
 * @file displaymode_win32.cpp
 * Win32 implementation of the DisplayMode class. @ingroup gl
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <qDebug>

#include "de_platform.h"
#include "displaymode_native.h"

#include <assert.h>
#include <vector>

#if 0
static std::vector<DisplayMode> displayModes;
static std::vector<CFDictionaryRef> displayDicts;
static CFDictionaryRef currentDisplayDict;
#endif

void DisplayMode_Native_Init(void)
{
#if 0
    // Let's see which modes are available.
    CFArrayRef modes = CGDisplayAvailableModes(kCGDirectMainDisplay);
    CFIndex count = CFArrayGetCount(modes);
    for(CFIndex i = 0; i < count; ++i)
    {
        CFDictionaryRef dict = (CFDictionaryRef) CFArrayGetValueAtIndex(modes, i);
        displayModes.push_back(modeFromDict(dict));
        displayDicts.push_back(dict);
    }
    currentDisplayDict = (CFDictionaryRef) CGDisplayCurrentMode(kCGDirectMainDisplay);
#endif
}

void DisplayMode_Native_Shutdown(void)
{
#if 0
    displayModes.clear();
    releaseDisplays();
#endif
}

int DisplayMode_Native_Count(void)
{
#if 0
    return displayModes.size();
#endif
    return 0;
}

void DisplayMode_Native_GetMode(int index, DisplayMode* mode)
{
#if 0
    assert(index >= 0 && index < (int)displayModes.size());
    *mode = displayModes[index];
#endif
}

void DisplayMode_Native_GetCurrentMode(DisplayMode* mode)
{
#if 0
    *mode = modeFromDict(currentDisplayDict);
#endif
}

int DisplayMode_Native_Change(const DisplayMode* mode, boolean shouldCapture)
{
#if 0
    const CGDisplayFadeInterval fadeTime = .5f;

    assert(mode);
    assert(findIndex(mode) >= 0); // mode must be an enumerated one

    // Fade all displays to black (blocks until faded).
    CGDisplayFadeReservationToken token;
    CGAcquireDisplayFadeReservation(kCGMaxDisplayReservationInterval, &token);
    CGDisplayFade(token, fadeTime, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0, 0, 0, true /* wait */);

    // Capture the displays now if haven't yet done so.
    bool wasPreviouslyCaptured = CGDisplayIsCaptured(kCGDirectMainDisplay);
    CGDisplayErr result = kCGErrorSuccess;

    CFDictionaryRef newModeDict = displayDicts[findIndex(mode)];

    // Capture displays if instructed to do so.
    if(shouldCapture && !captureDisplays(true))
    {
        result = kCGErrorFailure;
    }

    if(result == kCGErrorSuccess && currentDisplayDict != newModeDict)
    {
        qDebug() << "Changing to native mode" << findIndex(mode);

        // Try to change.
        result = CGDisplaySwitchToMode(kCGDirectMainDisplay, newModeDict);
        if(result != kCGErrorSuccess)
        {
            // Oh no!
            CGDisplaySwitchToMode(kCGDirectMainDisplay, currentDisplayDict);
            if(!wasPreviouslyCaptured) releaseDisplays();
        }
        else
        {
            currentDisplayDict = displayDicts[findIndex(mode)];
        }
    }

    // Fade back to normal.
    CGDisplayFade(token, 2*fadeTime, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0, 0, 0, false);
    CGReleaseDisplayFadeReservation(token);

    // Release display capture if instructed to do so.
    if(!shouldCapture)
    {
        captureDisplays(false);
    }

    return result == kCGErrorSuccess;
#endif
    return 0;
}
