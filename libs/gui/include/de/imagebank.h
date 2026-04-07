/** @file imagebank.h  Bank containing Image instances loaded from files.
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

#ifndef LIBGUI_IMAGEBANK_H
#define LIBGUI_IMAGEBANK_H

#include "libgui.h"
#include "de/image.h"

#include <de/infobank.h>

namespace de {

class File;

/**
 * Bank containing Image instances loaded from files.
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC ImageBank : public InfoBank
{
public:
    /**
     * Constructs a new image bank.
     *
     * @param flags  Properties for the bank. By default, the bank uses a
     *               background thread because large images may take some
     *               time to load. Hot storage is disabled because loading
     *               images from their potentially encoded source data is
     *               faster/more efficient than deserializating raw pixel data.
     */
    ImageBank(const Flags &flags = BackgroundThread | DisableHotStorage);

    void add(const DotPath &path, const String &imageFilePath);
    void addFromInfo(const File &file);

    const Image &image(const DotPath &path) const;

protected:
    ISource *newSourceFromInfo(const String &id);
    IData *loadFromSource(ISource &source);
    IData *newData();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_IMAGEBANK_H
