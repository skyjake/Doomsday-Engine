/** @file finaleanimwidget.h  InFine animation system, FinaleAnimWidget.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_UI_INFINE_FINALEANIMWIDGET_H
#define DE_UI_INFINE_FINALEANIMWIDGET_H

#include <doomsday/world/material.h>
#include "finalewidget.h"

/**
 * Finale animation widget. Colored rectangles or image sequence animations.
 *
 * @ingroup infine
 */
class FinaleAnimWidget : public FinaleWidget
{
public:
    /**
     * Describes a frame in the animation sequence.
     */
    struct Frame
    {
        enum Type
        {
            PFT_MATERIAL,
            PFT_PATCH,
            PFT_RAW, /// "Raw" graphic or PCX lump.
            PFT_XIMAGE /// External graphics resource.
        };

        int tics;
        Type type;
        struct Flags {
            char flip:1;
        } flags;
        union {
            world::Material *material;
            patchid_t patch;
            lumpnum_t lumpNum;
            DGLuint tex;
        } texRef;
        short sound;

        Frame();
        ~Frame();
    };
    typedef de::List<Frame *> Frames;

public:
    FinaleAnimWidget(const de::String &name);
    virtual ~FinaleAnimWidget();

    /// @todo Observe instead.
    bool animationComplete() const;

    FinaleAnimWidget &setLooping(bool yes = true);

    int newFrame(Frame::Type type, int tics, void *texRef, short sound, bool flagFlipH);

    const Frames &allFrames() const;
    FinaleAnimWidget &clearAllFrames();

    inline int frameCount() const { return allFrames().count(); }

    FinaleAnimWidget &resetAllColors();

    const animator_t *color() const;
    FinaleAnimWidget &setColorAndAlpha(const de::Vec4f &newColorAndAlpha, int steps = 0);
    FinaleAnimWidget &setColor(const de::Vec3f &newColor, int steps = 0);
    FinaleAnimWidget &setAlpha(float newAlpha, int steps = 0);

    const animator_t *edgeColor() const;
    FinaleAnimWidget &setEdgeColorAndAlpha(const de::Vec4f &newColorAndAlpha, int steps = 0);
    FinaleAnimWidget &setEdgeColor(const de::Vec3f &newColor, int steps = 0);
    FinaleAnimWidget &setEdgeAlpha(float newAlpha, int steps = 0);

    const animator_t *otherColor() const;
    FinaleAnimWidget &setOtherColorAndAlpha(const de::Vec4f &newColorAndAlpha, int steps = 0);
    FinaleAnimWidget &setOtherColor(const de::Vec3f &newColor, int steps = 0);
    FinaleAnimWidget &setOtherAlpha(float newAlpha, int steps = 0);

    const animator_t *otherEdgeColor() const;
    FinaleAnimWidget &setOtherEdgeColorAndAlpha(const de::Vec4f &newColorAndAlpha, int steps = 0);
    FinaleAnimWidget &setOtherEdgeColor(const de::Vec3f &newColor, int steps = 0);
    FinaleAnimWidget &setOtherEdgeAlpha(float newAlpha, int steps = 0);

protected:
#ifdef __CLIENT__
    void draw(const de::Vec3f &offset);
#endif
    void runTicks(/*timespan_t timeDelta*/);

private:
    DE_PRIVATE(d)
};

typedef FinaleAnimWidget::Frame FinaleAnimWidgetFrame;

#endif // DE_UI_INFINE_FINALEANIMWIDGET_H
