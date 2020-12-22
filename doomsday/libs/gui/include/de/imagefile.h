/** @file imagefile.h  Image file.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_IMAGEFILE_H
#define LIBGUI_IMAGEFILE_H

#include <de/file.h>
#include <de/filesys/iinterpreter.h>
#include "de/image.h"

namespace de {

/**
 * Image file. @ingroup fs
 *
 * Represents images in the file system and allows performing image filtering
 * via special file names.
 *
 * An image processing filter can be applied using a subfolder notation:
 *
 * <pre>example.png/HeightMap.toNormals</pre>
 *
 * The name of the filter is added to the path as if the image was a folder.
 * (It isn't actually a folder.) Internally, the filtered images are separate
 * ImageFile objects owned by the original ImageFile. This means they get
 * deleted if the original is deleted, and they can only be accessed via the
 * subfolder notation -- only the original image resides in the parent folder.
 */
class LIBGUI_PUBLIC ImageFile : public File
{
public:
    enum BuiltInFilter {
        NoFilter,
        Multiply,
        HeightMapToNormals,
        ColorDesaturate,
        ColorSolid,
        ColorMultiply,
    };

    DE_ERROR(FilterError);

public:
    /**
     * Constructs a new ImageFile instance.
     *
     * @param source  Image data. Ownership transferred to ImageFile.
     */
    ImageFile(File *source);

    ~ImageFile();

    String describe() const;

    /**
     * Returns the image contents. The image is loaded from the source file.
     */
    Image image() const;

    // File overrides:
    const IIStream &operator >> (IByteArray &bytes) const;

    // filesys::Node overrides:
    const Node *tryGetChild(const String &name) const;

public:
    struct LIBGUI_PUBLIC Interpreter : public filesys::IInterpreter {
        File *interpretFile(File *sourceData) const override;
    };

protected:
    ImageFile(BuiltInFilter filterType, ImageFile &filterSource);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_IMAGEFILE_H

