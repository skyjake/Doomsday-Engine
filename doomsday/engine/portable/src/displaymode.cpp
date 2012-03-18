/**
 * @file displaymode.cpp
 * Platform-independent display mode management. @ingroup gl
 *
 * @authors Copyright (c) 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "displaymode.h"

#ifdef MACOSX
#  include "displaymode_macx.h"
#endif

#include <vector>
#include <set>
#include <algorithm>

#include <qDebug>

static bool inited = false;

typedef std::set<DisplayMode> DisplayModes;
static DisplayModes displayModes;

static bool operator < (const DisplayMode& a, const DisplayMode& b)
{
    if(a.height == b.height)
    {
        if(a.width == b.width)
        {
            if(a.depth == b.depth)
            {
                return a.refreshRate < b.refreshRate;
            }
            return a.depth < b.depth;
        }
        return a.width < b.width;
    }
    return a.height < b.height;
}

static void calculateRatio(int width, int height, int* x, int* y)
{
    Q_ASSERT(x && y);

    *x = width;
    *y = height;

    // Reduce until we must resort to fractions.
    forever
    {
        bool divved = false;
        for(int div = 2; div <= qMin(*x/2, *y/2); div++)
        {
            int dx = *x / div;
            if(dx * div != *x) continue;
            int dy = *y / div;
            if(dy * div != *y) continue;
            divved = true;
            *x = dx;
            *y = dy;
            break;
        }
        if(!divved) break;
    }

    if(*x == 8 && *y == 5)
    {
        // This is commonly referred to as 16:10.
        *x *= 2;
        *y *= 2;
    }
}

int DisplayMode_Init(void)
{
    if(inited) return true;

    DisplayMode_Native_Init();

    for(int i = 0; i < DisplayMode_Native_Count(); ++i)
    {
        DisplayMode m;
        DisplayMode_Native_GetMode(i, &m);
        calculateRatio(m.width, m.height, &m.ratioX, &m.ratioY);
        displayModes.insert(m);
    }

    for(DisplayModes::iterator i = displayModes.begin(); i != displayModes.end(); ++i)
    {
        qDebug() << "size" << i->width << "x" << i->height << "depth" << i->depth << "rate"
                 << i->refreshRate << "ratio" << i->ratioX << ":" << i->ratioY;
    }

    inited = true;
    return true;
}

void DisplayMode_Shutdown(void)
{
    displayModes.clear();

    DisplayMode_Native_Shutdown();
}
