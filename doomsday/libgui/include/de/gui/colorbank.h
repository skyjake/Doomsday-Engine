/** @file colorbank.h  Bank of colors.
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

#ifndef LIBGUI_COLORBANK_H
#define LIBGUI_COLORBANK_H

#include "libgui.h"
#include <de/InfoBank>
#include <de/File>
#include <de/Vector>

namespace de {

/**
 * Bank of colors where each color is identified by a Path.
 */
class LIBGUI_PUBLIC ColorBank : public InfoBank
{
public:
    typedef Vector4ub Color;
    typedef Vector4f Colorf;

public:
    ColorBank();

    /**
     * Creates a number of colors based on information in an Info document.
     * The file is parsed first.
     *
     * @param source  File with Info source containing color definitions.
     */
    void addFromInfo(File const &file);

    /**
     * Finds a specific color.
     *
     * @param path  Identifier of the color.
     *
     * @return  Vector with the color values (0...255).
     */
    Color color(Path const &path) const;

    /**
     * Finds a specific color.
     *
     * @param path  Identifier of the color.
     *
     * @return  Vector with the floating-point color values (0...1).
     */
    Colorf colorf(Path const &path) const;

protected:
    virtual ISource *newSourceFromInfo(String const &id);
    virtual IData *loadFromSource(ISource &source);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_COLORBANK_H
