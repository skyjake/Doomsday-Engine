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

#include "de/imagebank.h"
#include "de/app.h"
#include "de/filesystem.h"
#include "de/imagefile.h"

#include <de/folder.h>
#include <de/filesystem.h>
#include <de/scripting/scriptedinfo.h>

namespace de {

DE_PIMPL_NOREF(ImageBank)
{
    struct ImageSource : public ISource
    {
        String filePath;
        float  pointRatio;

        ImageSource(const String &path, float pointRatio)
            : filePath(path)
            , pointRatio(pointRatio)
        {}

        Time modifiedAt() const
        {
            return FS::locate<File const>(filePath).status().modifiedAt;
        }

        Image load() const
        {
            Image img = FS::locate<const ImageFile>(filePath).image();
            if (pointRatio > 0)
            {
                img.setPointRatio(pointRatio);
            }
            return img;
        }
    };

    struct ImageData : public IData
    {
        Image image;

        ImageData() {}
        ImageData(const Image &img) : image(img) {}

        ISerializable *asSerializable()
        {
            return &image;
        }

        duint sizeInMemory() const
        {
            return duint(image.byteCount());
        }
    };
};

ImageBank::ImageBank(const Flags &flags) : InfoBank("ImageBank", flags), d(new Impl)
{}

void ImageBank::add(const DotPath &path, const String &imageFilePath)
{
    Bank::add(path, new Impl::ImageSource(imageFilePath, 0.f));
}

void ImageBank::addFromInfo(const File &file)
{
    LOG_AS("ImageBank");
    parse(file);
    addFromInfoBlocks("image");
}

const Image &ImageBank::image(const DotPath &path) const
{
    return data(path).as<Impl::ImageData>().image;
}

Bank::ISource *ImageBank::newSourceFromInfo(const String &id)
{
    const Record &def = info()[id];
    return new Impl::ImageSource(absolutePathInContext(def, def["path"]),
                                 def.getf("pointRatio", 0.f));
}

Bank::IData *ImageBank::loadFromSource(ISource &source)
{
    return new Impl::ImageData(source.as<Impl::ImageSource>().load());
}

Bank::IData *ImageBank::newData()
{
    return new Impl::ImageData();
}

} // namespace de
