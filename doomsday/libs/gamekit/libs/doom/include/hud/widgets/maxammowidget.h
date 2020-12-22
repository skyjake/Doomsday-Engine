/** @file maxammowidget.h  GUI widget for visualizing max player ammo ownership.
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

#ifndef LIBDOOM_UI_MAXAMMOWIDGET_H
#define LIBDOOM_UI_MAXAMMOWIDGET_H

#include "hud/hudwidget.h"
#include "doomdef.h"

/**
 * @ingroup ui
 */
class guidata_maxammo_t : public HudWidget
{
public:
    guidata_maxammo_t(int player);
    virtual ~guidata_maxammo_t();

    void reset();

    guidata_maxammo_t &setAmmoType(ammotype_t newAmmoType);

    void tick(timespan_t elapsed);
    void updateGeometry();
    void draw(const de::Vec2i &offset = de::Vec2i()) const;

private:
    int _value = 1994;
    ammotype_t _ammotype = ammotype_t(0);
};

#endif  // LIBDOOM_UI_MAXAMMOWIDGET_H
