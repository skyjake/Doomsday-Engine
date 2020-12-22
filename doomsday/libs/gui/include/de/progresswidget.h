/** @file progresswidget.h  Progress indicator.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_PROGRESSWIDGET_H
#define LIBAPPFW_PROGRESSWIDGET_H

#include "de/labelwidget.h"

#include <de/range.h>

namespace de {

/**
 * Progress indicator.
 *
 * Implemented as a specialized LabelWidget. The label is used for drawing the
 * static part of the indicator wheel and the status text. The ProgressWidget
 * draws the dynamic, animating part.
 *
 * @par Thread-safety
 *
 * The status of a ProgressWidget can be updated from any thread. This allows
 * background tasks to update the status during their operations.
 *
 * @todo Needs a bit of cleanup: the visual style (large gear, small gear, dots)
 * and the range setup should be separate concepts.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC ProgressWidget : public LabelWidget
{
public:
    enum Mode {
        Ranged,
        Indefinite,
        Dots        ///< One dot per range unit, no label.
    };

public:
    ProgressWidget(const String &name = String());

    void useMiniStyle(const DotPath &colorId = "text");
    void setRotationSpeed(float anglesPerSecond);

    Mode mode() const;
    Rangei range() const;
    bool isAnimating() const;

    void setColor(const DotPath &styleId);
    void setShadowColor(const DotPath &styleId);

    /**
     * Sets the text displayed in the widget. Thread-safe.
     *
     * @param text  New text for the progress.
     */
    void setText(const String &text);

    void setMode(Mode progressMode);

    /**
     * Sets the range of values that can be given in setProgress().
     * Automatically switches the widget to Ranged mode.
     *
     * @param range        Range of valid values for setProgress().
     * @param visualRange  Range to which @a range maps to (within 0...1).
     */
    void setRange(const Rangei &range, const Rangef &visualRange = Rangef(0.f, 1.f));

    void setProgress(int currentProgress, TimeSpan transitionSpan = 0.5);

    // Events.
    void update();

protected:
    void glInit();
    void glDeinit();
    void glMakeGeometry(GuiVertexBuilder &verts);
    void updateStyle();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_PROGRESSWIDGET_H
