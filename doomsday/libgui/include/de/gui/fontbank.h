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

#include <de/Bank>
#include <de/File>
#include "libgui.h"

namespace de {

/**
 * Bank containing fonts.
 */
class LIBGUI_PUBLIC FontBank : public Bank
{
public:
    FontBank();

    /**
     * Constructs a bank of fonts based on information in Info source.
     *
     * @param source  Info source containing font definitions.
     */
    FontBank(String const &source);

    /**
     * Constructs a bank of fonts based on information in an Info file.
     *
     * @param file  File with Info source.
     */
    FontBank(File const &file);

    void readInfo(String const &source);

    void readInfo(File const &file);

protected:
    virtual IData *loadFromSource(ISource &source);
    virtual IData *newData();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_FONTBANK_H
