/** @file style.h  User interface style.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "../libappfw.h"
#include <de/RuleBank>
#include <de/FontBank>
#include <de/ColorBank>
#include <de/ImageBank>

namespace de {

/**
 * User interface style.
 */
class LIBAPPFW_PUBLIC Style
{
public:
    Style();    
    virtual ~Style();

    /**
     * Loads a style from a resource pack.
     *
     * @param pack  Absolute path of a resource pack containing the style.
     */
    void load(String const &pack);

    RuleBank const &rules() const;
    FontBank const &fonts() const;
    ColorBank const &colors() const;
    ImageBank const &images() const;

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

    Font const *richStyleFont(Font::RichFormat::Style fontStyle) const;

    /**
     * Determines if blurred widget backgrounds are allowed.
     */
    virtual bool isBlurringAllowed() const;

public:
    /**
     * Returns the current global application UI style.
     * @return
     */
    static Style &appStyle();

    /**
     * Sets the current global application UI style.
     *
     * @param appStyle  New application style.
     */
    static void setAppStyle(Style &appStyle);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_STYLE_H
