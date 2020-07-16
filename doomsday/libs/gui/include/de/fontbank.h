/** @file fontbank.h  Font bank.
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

#ifndef LIBGUI_FONTBANK_H
#define LIBGUI_FONTBANK_H

#include <de/infobank.h>
#include <de/file.h>
#include "de/font.h"
#include "libgui.h"

namespace de {

/**
 * Bank containing fonts. @ingroup gui
 */
class LIBGUI_PUBLIC FontBank : public InfoBank
{
public:
    FontBank();

    /**
     * Creates a number of fonts based on information in an Info document.
     * The file is parsed first.
     *
     * @param file  File with Info source containing font definitions.
     */
    void addFromInfo(const File &file);

    /**
     * Finds a specific font.
     *
     * @param path  Identifier of the font.
     *
     * @return  Font instance.
     */
    const Font &font(const DotPath &path) const;

    /**
     * Sets a factor applied to all font sizes when loading the back.
     *
     * @param sizeFactor  Size factor.
     */
    void setFontSizeFactor(float sizeFactor);

    void reload();

protected:
    virtual ISource *newSourceFromInfo(const String &id);
    virtual IData *loadFromSource(ISource &source);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_FONTBANK_H
