/** @file style.h  User interface style.
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

#ifndef DENG_CLIENT_STYLE_H
#define DENG_CLIENT_STYLE_H

#include <de/RuleBank>
#include <de/FontBank>
#include <de/ColorBank>
#include <de/ImageBank>

/**
 * User interface style.
 */
class Style
{
public:
    Style();

    /**
     * Loads a style from a resource pack.
     *
     * @param pack  Path of a resource pack containing the style.
     */
    void load(de::String const &pack);

    de::RuleBank const &rules() const;
    de::FontBank const &fonts() const;
    de::ColorBank const &colors() const;
    de::ImageBank const &images() const;

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_STYLE_H
