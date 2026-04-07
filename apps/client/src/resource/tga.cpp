/** @file tga.cpp  Truevision TGA (a.k.a Targa) image reader
 *
 * @authors Copyright © 2003-2019 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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

#include "resource/tga.h"
#include "dd_share.h"

#include <de/legacy/memory.h>
#include <de/image.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace de;

uint8_t *TGA_Load(res::FileHandle &file, Vec2ui &outSize, int &pixelSize)
{
    const size_t initPos = file.tell();

    // Read the file into a memory buffer.
    Block fileContents(file.length() - initPos);
    file.read(fileContents.data(), fileContents.size());
    file.seek(initPos, res::SeekSet);

    Image img = Image::fromData(fileContents, ".tga");
    if (img.isNull())
    {
        return nullptr;
    }

    outSize   = img.size();
    pixelSize = img.depth() / 8;

    // Return a copy of the pixel data.
    return (uint8_t *) M_MemDup(img.bits(), img.byteCount());
}
