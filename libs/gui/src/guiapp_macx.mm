/** @file guiapp_macx.mm  macOS-specific GUI app initialization.
 *
 * @authors Copyright (c) 2025 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#import <AppKit/AppKit.h>
#include <SDL3/SDL_events.h>
#include <atomic>
#include <string.h>

static std::atomic<bool> s_mouseTrapped{false};
static Uint32            s_trackpadScrollEventType = SDL_EVENT_USER;

extern "C" void GuiApp_DiscardWheelMomentum(bool trapped)
{
    s_mouseTrapped = trapped;
}

extern "C" Uint32 GuiApp_TrackpadScrollEventType(void)
{
    return s_trackpadScrollEventType;
}

static void *floatToVoidPtr(float f)
{
    void *p = NULL;
    memcpy(&p, &f, sizeof(f));
    return p;
}

extern "C" void GuiApp_InitMacOSScrolling()
{
    s_trackpadScrollEventType = SDL_RegisterEvents(1);

    // Enable momentum (inertia) scrolling. SDL apps must opt in explicitly.
    [[NSUserDefaults standardUserDefaults] setBool:YES
                                            forKey:@"AppleMomentumScrollSupported"];

    // Intercept scroll wheel events before SDL sees them. Trackpad events are
    // converted to SDL user events carrying per-pixel deltas. Mouse wheel events
    // are passed through so SDL can accumulate integer step counts normally.
    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskScrollWheel
                                          handler:^NSEvent *(NSEvent *event) {
        if (s_mouseTrapped && event.momentumPhase != NSEventPhaseNone)
        {
            return nil; // Discard momentum events when in game input mode.
        }
        if (event.hasPreciseScrollingDeltas)
        {
            if (!s_mouseTrapped)
            {
                SDL_Event sdlEv;
                SDL_zero(sdlEv);
                sdlEv.type            = s_trackpadScrollEventType;
                sdlEv.user.data1      = floatToVoidPtr(float(event.scrollingDeltaX));
                sdlEv.user.data2      = floatToVoidPtr(float(event.scrollingDeltaY));
                SDL_PushEvent(&sdlEv);
            }
            return nil; // Trackpad events are handled via the user event above.
        }
        return event; // Mouse wheel: let SDL process it for integer step counts.
    }];
}
