/** @file fontbank.h  Font bank.
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

#ifndef LIBGUI_FONTBANK_H
#define LIBGUI_FONTBANK_H

#include <de/InfoBank>
#include <de/File>
#include "../Font"
#include "libgui.h"

namespace de {

/**
 * Bank containing fonts.
 */
class LIBGUI_PUBLIC FontBank : public InfoBank
{
public:
    FontBank();

    /**
     * Creates a number of fonts based on information in an Info document.
     * The file is parsed first.
     *
     * @param source  File with Info source containing font definitions.
     */
    void addFromInfo(File const &file);

    /**
     * Finds a specific font.
     *
     * @param path  Identifier of the font.
     *
     * @return  Font instance.
     */
    Font const &font(DotPath const &path) const;

protected:
    virtual ISource *newSourceFromInfo(String const &id);
    virtual IData *loadFromSource(ISource &source);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_FONTBANK_H
