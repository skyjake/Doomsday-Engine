/** @file style.h  User interface style.
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

#ifndef LIBAPPFW_STYLE_H
#define LIBAPPFW_STYLE_H

#include "../libgui.h"
#include <de/rulebank.h>
#include <de/fontbank.h>
#include <de/colorbank.h>
#include <de/imagebank.h>

namespace de {

class GuiWidget;
class Package;
namespace ui { class Stylist; }

/**
 * User interface style.
 *
 * @ingroup appfw
 */
class LIBGUI_PUBLIC Style
{
public:
    DE_AUDIENCE(Change, void styleChanged(Style &))

public:
    Style();
    virtual ~Style();

    DE_CAST_METHODS()

    /**
     * Loads a style from a resource pack.
     *
     * @param pack  Package containing the style.
     */
    void load(const Package &pack);

    const RuleBank &rules() const;
    const FontBank &fonts() const;
    const ColorBank &colors() const;
    const ImageBank &images() const;

    RuleBank &rules();
    FontBank &fonts();
    ColorBank &colors();
    ImageBank &images();

    // Partial implementation for Font::RichFormat::IStyle.
    void richStyleFormat(int contentStyle,
                         float &sizeFactor,
                         Font::RichFormat::Weight &fontWeight,
                         Font::RichFormat::Style &fontStyle,
                         int &colorIndex) const;

    const Font *richStyleFont(Font::RichFormat::Style fontStyle) const;

    virtual const ui::Stylist &emptyContentLabelStylist() const;

    /**
     * Determines if blurred widget backgrounds are allowed.
     */
    virtual bool isBlurringAllowed() const;

    virtual GuiWidget *sharedBlurWidget() const;

    virtual void performUpdate();

public:
    /**
     * Returns the current global application UI style.
     * @return
     */
    static Style &get();

    /**
     * Sets the current global application UI style.
     *
     * @param appStyle  New application style.
     */
    static void setAppStyle(Style &appStyle);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_STYLE_H
