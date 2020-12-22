/** @file chainwidget.h  GUI widget for -.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_UI_CHAINWIDGET_H
#define LIBCOMMON_UI_CHAINWIDGET_H

#include "hud/hudwidget.h"

/**
 * @ingroup ui
 */
class guidata_chain_t : public HudWidget
{
public:
    guidata_chain_t(int player);
    virtual ~guidata_chain_t();

    void reset();

    void tick(timespan_t elapsed);
    void updateGeometry();
    void draw(const de::Vec2i &offset = de::Vec2i()) const;

public:
    static void prepareAssets();

private:
    int _healthMarker = 0;
    int _wiggle = 0;
};

#endif  // LIBCOMMON_UI_CHAINWIDGET_H
