/** @file facewidget.h  GUI widget for -.
 *
 * @authors Copyright © 2005-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOM_UI_FACEWIDGET_H
#define LIBDOOM_UI_FACEWIDGET_H

#include "hud/hudwidget.h"
#include "doomdef.h"

/**
 * @ingroup ui
 */
class guidata_face_t : public HudWidget
{
public:
    guidata_face_t(void (*updateGeometry) (HudWidget *wi),
                   void (*drawer) (HudWidget *wi, Point2Raw const *offset),
                   de::dint player);
    virtual ~guidata_face_t();

    void reset();

    void tick(timespan_t elapsed);
    //void updateGeometry();
    //void draw(de::Vector2i const &offset = de::Vector2i()) const;

public:
    static void prepareAssets();

//private:
    DENG2_PRIVATE(d)
};

void Face_Drawer    (guidata_face_t *face, Point2Raw const *offset);
void SBarFace_Drawer(guidata_face_t *face, Point2Raw const *offset);

void Face_UpdateGeometry    (guidata_face_t *face);
void SBarFace_UpdateGeometry(guidata_face_t *face);

#endif  // LIBDOOM_UI_FACEWIDGET_H
