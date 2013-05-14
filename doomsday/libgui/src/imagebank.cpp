/** @file imagebank.h  Bank containing Image instances loaded from files.
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

#include "de/ImageBank"
#include "de/App"

#include <de/ScriptedInfo>

namespace de {

DENG2_PIMPL_NOREF(ImageBank)
{
    struct ImageSource : public ISource
    {
        String filePath;

        ImageSource(String const &path) : filePath(path) {}

        Time modifiedAt() const
        {
            return App::rootFolder().locate<File>(filePath).status().modifiedAt;
        }

        Image load() const
        {
            Block imageData;
            App::rootFolder().locate<File const>(filePath) >> imageData;
            return QImage::fromData(imageData);
        }
    };

    struct ImageData : public IData
    {
        Image image;

        ImageData() {}
        ImageData(Image const &img) : image(img) {}

        ISerializable *asSerializable()
        {
            return &image;
        }

        duint sizeInMemory() const
        {
            return image.byteCount();
        }
    };

    ScriptedInfo info;
};

ImageBank::ImageBank(Flags const &flags)
    : Bank(flags), d(new Instance)
{}

void ImageBank::add(Path const &path, String const &imageFilePath)
{
    Bank::add(path, new Instance::ImageSource(imageFilePath));
}

void ImageBank::addFromInfo(String const &source, String const &relativeToPath)
{
    LOG_AS("ImageBank");
    try
    {
        d->info.parse(source);

        foreach(String id, d->info.allBlocksOfType("image"))
        {
            Record const &def = d->info[id];
            add(id, relativeToPath / def["path"]);
        }
    }
    catch(Error const &er)
    {
        LOG_WARNING("Failed to read Info source:\n") << er.asText();
    }
}

void ImageBank::addFromInfo(File const &file)
{
    // Relative paths in the defs are assumed relative to this file.
    addFromInfo(String::fromUtf8(Block(file)), file.path().fileNamePath());
}

Image &ImageBank::image(Path const &path) const
{
    return static_cast<Instance::ImageData &>(data(path)).image;
}

Bank::IData *ImageBank::loadFromSource(ISource &source)
{
    return new Instance::ImageData(static_cast<Instance::ImageSource &>(source).load());
}

Bank::IData *ImageBank::newData()
{
    return new Instance::ImageData();
}

} // namespace de
