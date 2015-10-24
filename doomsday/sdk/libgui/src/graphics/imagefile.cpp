/** @file imagefile.cpp  Image file.
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <QHash>

namespace de {

static String const HEIGHTMAP_TO_NORMALS("HeightMap.toNormals");

DENG2_PIMPL(ImageFile)
{
    BuiltInFilter filter = NoFilter;
    QHash<BuiltInFilter, ImageFile *> filtered; // owned

    Instance(Public *i) : Base(i) {}

    ~Instance()
    {
        qDeleteAll(filtered.values());
    }

    ImageFile *makeOrGetFiltered(BuiltInFilter filter)
    {
        // Already got it?
        auto found = filtered.constFind(filter);
        if(found != filtered.constEnd())
        {
            return found.value();
        }
        if(filter == HeightMapToNormals)
        {
            ImageFile *sub = new ImageFile(filter, self);
            filtered.insert(filter, sub);
            return sub;
        }
        return nullptr;
    }

    ImageFile &filterSource()
    {
        return self.Node::parent()->as<ImageFile>();
    }

    static String filterTypeToText(BuiltInFilter filter)
    {
        switch(filter)
        {
        case HeightMapToNormals:
            return HEIGHTMAP_TO_NORMALS;

        default:
            break;
        }
        return "";
    }
};

ImageFile::ImageFile(File *source)
    : File(source->name())
    , d(new Instance(this))
{
    setSource(source);
}

ImageFile::ImageFile(BuiltInFilter filterType, ImageFile &filterSource)
    : File(Instance::filterTypeToText(filterType))
    , d(new Instance(this))
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
    switch(d->filter)
    {
    case HeightMapToNormals:
        desc += " (filter: heightfield to normals)";
        break;

    default:
        break;
    }
    return desc;
}

Image ImageFile::image() const
{
    if(d->filter != NoFilter)
    {
        // Node parent is the source for the filter.
        Image img = d->filterSource().image();
        if(d->filter == HeightMapToNormals)
        {
            HeightMap heightMap;
            heightMap.loadGrayscale(img);
            img = heightMap.makeNormalMap();
        }
        return img;
    }
    else
    {
        return Image::fromData(*source(), name().fileNameExtension());
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
    if(!name.compareWithoutCase(HEIGHTMAP_TO_NORMALS))
    {
        return d->makeOrGetFiltered(HeightMapToNormals);
    }
    return nullptr;
}

File *ImageFile::Interpreter::interpretFile(File *sourceData) const
{
    if(Image::recognize(*sourceData))
    {
        LOG_RES_VERBOSE("Interpreted ") << sourceData->description()
                                        << " as an image";
        return new ImageFile(sourceData);
    }
    return nullptr;
}

} // namespace de
