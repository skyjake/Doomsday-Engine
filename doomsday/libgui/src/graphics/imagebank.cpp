/** @file imagebank.h  Bank containing Image instances loaded from files.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
            return Image::fromData(imageData);
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

    //String relativeToPath;
};

ImageBank::ImageBank(Flags const &flags) : InfoBank(flags), d(new Instance)
{}

void ImageBank::add(DotPath const &path, String const &imageFilePath)
{
    Bank::add(path, new Instance::ImageSource(imageFilePath));
}

void ImageBank::addFromInfo(File const &file)
{
    LOG_AS("ImageBank");
    //d->relativeToPath = file.path().fileNamePath();
    parse(file);
    addFromInfoBlocks("image");
}

Image const &ImageBank::image(DotPath const &path) const
{
    return data(path).as<Instance::ImageData>().image;
}

Bank::ISource *ImageBank::newSourceFromInfo(String const &id)
{
    Record const &def = info()[id];
    return new Instance::ImageSource(relativeToPath(def) / def["path"]);
}

Bank::IData *ImageBank::loadFromSource(ISource &source)
{
    return new Instance::ImageData(source.as<Instance::ImageSource>().load());
}

Bank::IData *ImageBank::newData()
{
    return new Instance::ImageData();
}

} // namespace de
