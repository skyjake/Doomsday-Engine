/** @file imagefile.cpp  Image file.
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

#include "de/ImageFile"
#include "de/HeightMap"
#include <de/App>
#include <de/Folder>
#include <de/LogBuffer>

#include <QHash>

namespace de {

static String const MULTIPLY            ("Multiply:");
static String const HEIGHTMAP_TO_NORMALS("HeightMap.toNormals");
static String const COLOR_DESATURATE    ("Color.desaturate");

DENG2_PIMPL(ImageFile)
{
    BuiltInFilter filter = NoFilter;
    QHash<BuiltInFilter, ImageFile *> filtered; // owned
    String filterParameter;

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        qDeleteAll(filtered);
    }

    ImageFile *makeOrGetFiltered(BuiltInFilter filter)
    {
        // Already got it?
        auto found = filtered.constFind(filter);
        if (found != filtered.constEnd())
        {
            return found.value();
        }
        if (filter != NoFilter) //filter == HeightMapToNormals || filter == Multiply)
        {
            ImageFile *sub = new ImageFile(filter, self());
            filtered.insert(filter, sub);
            return sub;
        }
        return nullptr;
    }

    ImageFile &filterSource()
    {
        return self().Node::parent()->as<ImageFile>();
    }

    static String filterTypeToText(BuiltInFilter filter)
    {
        switch (filter)
        {
        case HeightMapToNormals:
            return HEIGHTMAP_TO_NORMALS;

        case Multiply:
            return MULTIPLY;

        case ColorDesaturate:
            return COLOR_DESATURATE;

        default:
            break;
        }
        return "";
    }
};

ImageFile::ImageFile(File *source)
    : File(source->name())
    , d(new Impl(this))
{
    setSource(source);
}

ImageFile::ImageFile(BuiltInFilter filterType, ImageFile &filterSource)
    : File(Impl::filterTypeToText(filterType))
    , d(new Impl(this))
{
    d->filter = filterType;
    setParent(&filterSource);
}

ImageFile::~ImageFile()
{
    DENG2_GUARD(this);

    DENG2_FOR_AUDIENCE2(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion().clear();
    deindex();
}

String ImageFile::describe() const
{
    String desc = String("image \"%1\"").arg(d->filter == NoFilter?
                                             name() : d->filterSource().name());
    switch (d->filter)
    {
    case HeightMapToNormals:
        desc += " (filter: heightfield to normals)";
        break;

    case Multiply:
        desc += " (filter: multiplied with " + d->filterParameter + ")";
        break;

    case ColorDesaturate:
        desc += " (filter: desaturate)";
        break;

    default:
        break;
    }
    return desc;
}

Image ImageFile::image() const
{
    if (d->filter != NoFilter)
    {
        // Node parent is the source for the filter.
        Image img = d->filterSource().image();
        if (d->filter == HeightMapToNormals)
        {
            HeightMap heightMap;
            heightMap.loadGrayscale(img);
            img = heightMap.makeNormalMap();
        }
        else if (d->filter == Multiply)
        {
            String const refPath = d->filterSource().path().fileNamePath();
            Image factorImg = App::rootFolder().locate<ImageFile const>
                    (refPath / d->filterParameter).image();

            if (img.size() != factorImg.size())
            {
                throw FilterError("ImageFile::image",
                                  String("Cannot multiply %1 and %2 due to different sizes")
                                  .arg(d->filterSource().path())
                                  .arg(refPath / d->filterParameter));
            }

            img = img.multiplied(factorImg);
        }
        else if (d->filter == ColorDesaturate)
        {
            img = img.colorized(Image::Color(255, 255, 255, 255));
        }
        return img;
    }
    else
    {
        Image img = Image::fromData(*source(), extension());
        if (source()->name().contains("@2x.", Qt::CaseInsensitive))
        {
            img.setPointRatio(.5f);
        }
        return img;
    }
}

IIStream const &ImageFile::operator >> (IByteArray &bytes) const
{
    // The source file likely supports streaming the raw data.
    *source() >> bytes;
    return *this;
}

filesys::Node const *ImageFile::tryGetChild(String const &name) const
{
    if (!name.compareWithoutCase(HEIGHTMAP_TO_NORMALS))
    {
        return d->makeOrGetFiltered(HeightMapToNormals);
    }
    else if (name.startsWith(MULTIPLY, Qt::CaseInsensitive))
    {
        /// @bug Different filter parameters should be saved as unique ImageFiles,
        /// or otherwise the latest accessed parameter is in effect for all multiplied
        /// instances. -jk
        ImageFile *filtered = d->makeOrGetFiltered(Multiply);
        filtered->d->filterParameter = name.substr(MULTIPLY.size());
        return filtered;
    }
    else if (!name.compareWithoutCase(COLOR_DESATURATE))
    {
        return d->makeOrGetFiltered(ColorDesaturate);
    }
    else if (d->filter == Multiply)
    {
        // Append to filter parameter path.
        d->filterParameter = d->filterParameter / name;
        return this;
    }
    return nullptr;
}

File *ImageFile::Interpreter::interpretFile(File *sourceData) const
{
    if (Image::recognize(*sourceData))
    {
        LOG_RES_XVERBOSE("Interpreted %s as an image", sourceData->description());
        return new ImageFile(sourceData);
    }
    return nullptr;
}

} // namespace de
