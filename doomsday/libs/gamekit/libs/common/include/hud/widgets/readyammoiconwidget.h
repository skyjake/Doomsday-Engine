/** @file readyammoiconwidget.h  GUI widget for -.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_UI_READUAMMOICONWIDGET_H
#define LIBCOMMON_UI_READUAMMOICONWIDGET_H

#include "hud/hudwidget.h"

/**
 * @ingroup ui
 */
class guidata_readyammoicon_t : public HudWidget
{
public:
    guidata_readyammoicon_t(void (*updateGeometry) (HudWidget *wi),
                            void (*drawer) (HudWidget *wi, const Point2Raw *offset),
                            int player);
    virtual ~guidata_readyammoicon_t();

    void reset();

    void tick(timespan_t elapsed);
    //void updateGeometry();
    //void draw(const de::Vec2i &offset = de::Vec2i()) const;

public:
    static void prepareAssets();

//private:
#if __JDOOM__
    int _sprite = -1;
#else
    patchid_t _patchId = 0;
#endif
};

void ReadyAmmoIconWidget_Drawer    (guidata_readyammoicon_t *icon, const Point2Raw *offset);
#if __JHERETIC__
void SBarReadyAmmoIconWidget_Drawer(guidata_readyammoicon_t *icon, const Point2Raw *offset);
#endif  // __JHERETIC__

void ReadyAmmoIconWidget_UpdateGeometry    (guidata_readyammoicon_t *icon);
#if __JHERETIC__
void SBarReadyAmmoIconWidget_UpdateGeometry(guidata_readyammoicon_t *icon);
#endif  // __JHERETIC__

#endif  // LIBCOMMON_UI_READUAMMOICONWIDGET_H
