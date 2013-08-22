/** @file progresswidget.h  Progress indicator.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef DENG_CLIENT_PROGRESSWIDGET_H
#define DENG_CLIENT_PROGRESSWIDGET_H

#include "labelwidget.h"

#include <de/Range>

/**
 * Progress indicator.
 *
 * Implemented as a specialized LabelWidget. The label is used for drawing the
 * static part of the indicator wheel and the status text. The ProgressWidget
 * draws the dynamic, animating part.
 *
 * @par Thread-Safety
 *
 * The status of a ProgressWidget can be updated from any thread. This allows
 * background tasks to update the status during their operations.
 */
class ProgressWidget : public LabelWidget
{
public:
    enum Mode {
        Ranged,
        Indefinite
    };

public:
    ProgressWidget(de::String const &name = "");

    void useMiniStyle();

    Mode mode() const;
    de::Rangei range() const;
    bool isAnimating() const;

    void setColor(de::DotPath const &styleId);
    void setShadowColor(de::DotPath const &styleId);

    /**
     * Sets the text displayed in the widget. Thread-safe.
     *
     * @param text  New text for the progress.
     */
    void setText(de::String const &text);

    void setMode(Mode progressMode);

    /**
     * Sets the range of values that can be given in setProgress().
     * Automatically switches the widget to Ranged mode.
     *
     * @param range        Range of valid values for setProgress().
     * @param visualRange  Range to which @a range maps to (within 0...1).
     */
    void setRange(de::Rangei const &range, de::Rangef const &visualRange = de::Rangef(0.f, 1.f));

    void setProgress(int currentProgress, de::TimeDelta const &transitionSpan = 0.5);

    // Events.
    void update();

protected:
    void glInit();
    void glDeinit();
    void glMakeGeometry(DefaultVertexBuf::Builder &verts);
    void updateStyle();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_PROGRESSWIDGET_H
