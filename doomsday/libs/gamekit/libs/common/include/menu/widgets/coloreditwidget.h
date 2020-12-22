/** @file coloreditwidget.h  UI widget for editing a color.
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

#ifndef LIBCOMMON_UI_COLOREDITWIDGET
#define LIBCOMMON_UI_COLOREDITWIDGET

#include <de/vector.h>
#include "widget.h"

namespace common {
namespace menu {

#define MNDATA_COLORBOX_WIDTH       4  ///< Default inner width (in fixed 320x200 space).
#define MNDATA_COLORBOX_HEIGHT      4  ///< Default inner height (in fixed 320x200 space).

/**
 * @defgroup mncolorboxSetColorFlags  MNColorBox Set Color Flags.
 */
///@{
#define MNCOLORBOX_SCF_NO_ACTION    0x1  ///< Do not call any linked action function.
///@}

/**
 * UI widget for editing a color.
 *
 * @ingroup menu
 */
class ColorEditWidget : public Widget
{
public:
    explicit ColorEditWidget(const de::Vec4f &color = de::Vec4f(),
                             bool rgbaMode = false);
    virtual ~ColorEditWidget();

    void draw() const;
    void updateGeometry();
    int handleCommand(menucommand_e command);

    /**
     * Change the dimensions of the preview area (in fixed 320x200 space).
     *
     * @param newDimensions  New dimensions of the preview area.
     *
     * @return  Reference to this ColorEditWidget.
     */
    ColorEditWidget &setPreviewDimensions(const de::Vec2i &newDimensions);

    /**
     * Returns the dimensions of the preview area (in fixed 320x200 space).
     */
    de::Vec2i previewDimensions() const;

    /**
     * Returns @c true if operating in RGBA mode.
     */
    bool rgbaMode() const;

    /**
     * Returns a copy of the current color.
     */
    de::Vec4f color() const;

    inline float red  () const { return color().x; }
    inline float green() const { return color().y; }
    inline float blue () const { return color().z; }
    inline float alpha() const { return color().w; }

    /**
     * Change the current color of the preview widget.
     *
     * @param newColor  New color and alpha.
     * @param flags     @ref mncolorboxSetColorFlags
     *
     * @return  Reference to this ColorEditWidget.
     */
    ColorEditWidget &setColor(const de::Vec4f &newColor, int flags = MNCOLORBOX_SCF_NO_ACTION);

    ColorEditWidget &setRed  (float newRed,   int flags = MNCOLORBOX_SCF_NO_ACTION);
    ColorEditWidget &setGreen(float newGreen, int flags = MNCOLORBOX_SCF_NO_ACTION);
    ColorEditWidget &setBlue (float newBlue,  int flags = MNCOLORBOX_SCF_NO_ACTION);
    ColorEditWidget &setAlpha(float newAlpha, int flags = MNCOLORBOX_SCF_NO_ACTION);

private:
    DE_PRIVATE(d)
};

} // namespace menu
} // namespace common

#endif // LIBCOMMON_UI_COLOREDITWIDGET
