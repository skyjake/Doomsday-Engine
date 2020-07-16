/** @file finaletextwidget.h  InFine animation system, FinaleTextWidget.
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

#ifndef DE_UI_INFINE_FINALETEXTWIDGET_H
#define DE_UI_INFINE_FINALETEXTWIDGET_H

#include <de/string.h>
#include <de/vector.h>
#include "ui/infine/finalewidget.h"

/**
 * Finale text widget.
 *
 * @ingroup infine
 */
class FinaleTextWidget : public FinaleWidget
{
public:
    FinaleTextWidget(const de::String &name);
    virtual ~FinaleTextWidget();

    void accelerate();
    FinaleTextWidget &setCursorPos(int newPos);

    bool animationComplete() const;

    /**
     * Returns the total number of @em currently-visible characters, excluding control/escape
     * sequence characters.
     */
    int visLength();

    const char *text() const;
    FinaleTextWidget &setText(const char *newText);

    fontid_t font() const;
    FinaleTextWidget &setFont(fontid_t newFont);

    /// @return  @ref alignmentFlags
    int alignment() const;
    /// @param newAlignment  @ref alignmentFlags
    FinaleTextWidget &setAlignment(int newAlignment);

    float lineHeight() const;
    FinaleTextWidget &setLineHeight(float newLineHeight);

    int scrollRate() const;
    FinaleTextWidget &setScrollRate(int newRateInTics);

    int typeInRate() const;
    FinaleTextWidget &setTypeInRate(int newRateInTics);

    FinaleTextWidget &setPageColor(uint id);
    FinaleTextWidget &setPageFont(uint id);

    FinaleTextWidget &setColorAndAlpha(const de::Vec4f &newColorAndAlpha, int steps = 0);
    FinaleTextWidget &setColor(const de::Vec3f &newColor, int steps = 0);
    FinaleTextWidget &setAlpha(float alpha, int steps = 0);

protected:
#ifdef __CLIENT__
    void draw(const de::Vec3f &offset);
#endif
    void runTicks();

public:
    DE_PRIVATE(d)
};

#endif // DE_UI_INFINE_FINALETEXTWIDGET_H
