/** @file packageiconbank.h  Bank for package icons.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_PACKAGEICONBANK_H
#define LIBGUI_PACKAGEICONBANK_H

#include "de/texturebank.h"
#include <de/vector.h>

namespace de {

/**
 * Bank for package icons.
 *
 * Before use, PackageIconBank must be told which atlas the icons are stored.
 *
 * Loaded icons are resized to a size suitable for use in package lists before they
 * are allocated on the UI atlas.
 */
class LIBGUI_PUBLIC PackageIconBank : public TextureBank
{
public:
    typedef Vec2ui Size;

    PackageIconBank();

    /**
     * Sets the display size for icons (in pixels). Icon images are resized to this
     * size before allocating on the atlas.
     *
     * @param iconDisplaySize  Display size for icons.
     */
    void setDisplaySize(const Size &displaySize);

    bool packageContainsIcon(const File &packageFile) const;

    /**
     * Returns the ID of a package icon, if one is loaded and available in the atlas.
     * If the icon is not yet available, loading it is requested. The caller can use
     * the Bank's Load notification to observe when the icon is available.
     *
     * @param packageFile  Package whose icon to fetch.
     *
     * @return Allocation ID, or Id::None.
     */
    Id packageIcon(const File &packageFile);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_PACKAGEICONBANK_H
