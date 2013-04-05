/** @file displaymode_x11.cpp X11 implementation of the DisplayMode native functionality. 
 * @ingroup gl
 *
 * Uses the XRandR extension to manipulate the display.
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <QDebug>
#include <QX11Info>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/xf86vmode.h>

#include "de/gui/displaymode_native.h"

#include <assert.h>
#include <vector>
#include <de/Log>

typedef std::vector<DisplayMode> DisplayModes;

static int displayDepth;
static Rotation displayRotation;
static DisplayModes availableModes;
static DisplayMode currentMode;

/**
 * Wrapper for the Xrandr configuration info. The config is kept in memory only
 * for the lifetime of an RRInfo instance.
 */
class RRInfo
{
public:
    /**
     * Queries all the available modes in the display configuration.
     */
    RRInfo() : _numSizes(0)
    {
        _conf = XRRGetScreenInfo(QX11Info::display(), QX11Info::appRootWindow());
        if(!_conf) return; // Not available.

        // Let's see which modes are available.
        _sizes = XRRConfigSizes(_conf, &_numSizes);
        for(int i = 0; i < _numSizes; ++i)
        {
            int numRates = 0;
            short* rates = XRRConfigRates(_conf, i, &numRates);
            for(int k = 0; k < numRates; k++)
            {
                DisplayMode mode;
                memset(&mode, 0, sizeof(mode));
                mode.width = _sizes[i].width;
                mode.height = _sizes[i].height;
                mode.depth = displayDepth;
                mode.refreshRate = rates[k];
                _modes.push_back(mode);
            }
        }

        Time prevConfTime;
        _confTime = XRRConfigTimes(_conf, &prevConfTime);
    }

    ~RRInfo()
    {
        if(_conf) XRRFreeScreenConfigInfo(_conf);
    }

    /**
     * Returns the currently active mode as specified in the Xrandr config.
     * Also determines the display's current rotation angle.
     */
    DisplayMode currentMode() const
    {
        DisplayMode mode;
        memset(&mode, 0, sizeof(mode));

        if(!_conf) return mode;

        SizeID currentSize = XRRConfigCurrentConfiguration(_conf, &displayRotation);

        // Update the current mode.
        mode.width = _sizes[currentSize].width;
        mode.height = _sizes[currentSize].height;
        mode.depth = displayDepth;
        mode.refreshRate = XRRConfigCurrentRate(_conf);
        return mode;
    }

    std::vector<DisplayMode>& modes() { return _modes; }

    static short rateFromMode(const DisplayMode* mode)
    {
        return short(qRound(mode->refreshRate));
    }

    int find(const DisplayMode* mode) const
    {
        for(int i = 0; i < _numSizes; ++i)
        {
            int numRates = 0;
            short* rates = XRRConfigRates(_conf, i, &numRates);
            for(int k = 0; k < numRates; k++)
            {
                if(rateFromMode(mode) == rates[k] &&
                   mode->width == _sizes[i].width &&
                   mode->height == _sizes[i].height)
                {
                    // This is the one.
                    return i;
                }
            }
        }
        return -1;
    }

    bool apply(const DisplayMode* mode)
    {
        if(!_conf) return false;

        int sizeIdx = find(mode);
        assert(sizeIdx >= 0);

        //qDebug() << "calling XRRSetScreenConfig" << _confTime;
        Status result = XRRSetScreenConfigAndRate(QX11Info::display(), _conf, QX11Info::appRootWindow(),
                                                  sizeIdx, displayRotation, rateFromMode(mode), _confTime);
        //qDebug() << "result" << result;
        if(result == BadValue)
        {
            qDebug() << "Failed to apply screen config and rate with Xrandr";
            return false;
        }

        // Update the current mode.
        ::currentMode = *mode;
        return true;
    }

private:
    XRRScreenConfiguration* _conf;
    XRRScreenSize* _sizes;
    Time _confTime;
    int _numSizes;
    DisplayModes _modes;
};

void DisplayMode_Native_Init(void)
{
    // We will not be changing the depth at runtime.
    displayDepth = QX11Info::appDepth();

    RRInfo info;
    availableModes = info.modes();
    currentMode = info.currentMode();
}

void DisplayMode_Native_Shutdown(void)
{
    availableModes.clear();
}

int DisplayMode_Native_Count(void)
{
    return availableModes.size();
}

void DisplayMode_Native_GetMode(int index, DisplayMode* mode)
{
    assert(index >= 0 && index < DisplayMode_Native_Count());
    *mode = availableModes[index];
}

void DisplayMode_Native_GetCurrentMode(DisplayMode* mode)
{
    *mode = currentMode;
}

#if 0
static int findMode(const DisplayMode* mode)
{
    for(int i = 0; i < DisplayMode_Native_Count(); ++i)
    {
        DisplayMode d = devToDisplayMode(devModes[i]);
        if(DisplayMode_IsEqual(&d, mode))
        {
            return i;
        }
    }
    return -1;
}
#endif

int DisplayMode_Native_Change(const DisplayMode* mode, boolean shouldCapture)
{
    DENG_UNUSED(shouldCapture);
    return RRInfo().apply(mode);
}

void DisplayMode_Native_GetColorTransfer(displaycolortransfer_t* colors)
{
    Display *dpy = QX11Info::display();
    int screen = QX11Info::appScreen();
    int event = 0, error = 0;

    LOG_AS("GetColorTransfer");

    if(!dpy || !XF86VidModeQueryExtension(dpy, &event, &error))
    {
        LOG_MSG("XFree86-VidModeExtension not available.");
        return;
    }
    LOG_DEBUG("event# %i error# %i") << event << error;

    // Ramp size.
    int rampSize = 0;
    XF86VidModeGetGammaRampSize(dpy, screen, &rampSize);
    LOG_VERBOSE("Gamma ramp size: %i") << rampSize;
    if(!rampSize) return;

    ushort* xRamp = new ushort[3 * rampSize];

    // Get the current ramps.
    XF86VidModeGetGammaRamp(dpy, screen, rampSize, xRamp,
                            xRamp + rampSize, xRamp + 2*rampSize);

    for(uint i = 0; i < 256; ++i)
    {
        const uint tx = qMin(uint(rampSize - 1), i * rampSize / 255);
        colors->table[i]       = xRamp[tx];
        colors->table[i + 256] = xRamp[tx + rampSize];
        colors->table[i + 512] = xRamp[tx + 2*rampSize];
    }

    delete [] xRamp;
}

void DisplayMode_Native_SetColorTransfer(const displaycolortransfer_t* colors)
{
    Display* dpy = QX11Info::display();
    if(!dpy) return;

    // Ramp size.
    int rampSize = 0;
    XF86VidModeGetGammaRampSize(dpy, QX11Info::appScreen(), &rampSize);
    if(!rampSize) return;

    ushort* xRamp = new ushort[3 * rampSize];

    for(int i = 0; i < rampSize; ++i)
    {
        const uint tx = qMin(255, i * 256 / (rampSize - 1));
        xRamp[i]              = colors->table[tx];
        xRamp[i + rampSize]   = colors->table[tx + 256];
        xRamp[i + 2*rampSize] = colors->table[tx + 512];
    }

    XF86VidModeSetGammaRamp(dpy, QX11Info::appScreen(), rampSize,
                            xRamp, xRamp + rampSize, xRamp + 2*rampSize);

    delete [] xRamp;
}
