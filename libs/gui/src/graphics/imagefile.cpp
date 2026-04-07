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

#include "de/imagefile.h"
#include "de/heightmap.h"
#include <de/app.h>
#include <de/folder.h>
#include <de/logbuffer.h>
#include <de/hash.h>

namespace de {

static const String MULTIPLY            ("Multiply:");
static const String HEIGHTMAP_TO_NORMALS("HeightMap.toNormals");
static const String COLOR_DESATURATE    ("Color.desaturate");
static const String COLOR_SOLID         ("Color.solid:");
static const String COLOR_MULTIPLY      ("Color.multiply:");

DE_PIMPL(ImageFile)
{
    BuiltInFilter            filter = NoFilter;
    Hash<duint, ImageFile *> filtered; // owned
    String                   filterParameter;

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        filtered.deleteAll();
    }

    ImageFile *makeOrGetFiltered(BuiltInFilter filter)
    {
        // Already got it?
        auto found = filtered.find(filter);
        if (found != filtered.end())
        {
            return found->second;
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

        case ColorSolid:
            return COLOR_SOLID;

        case ColorMultiply:
            return COLOR_MULTIPLY;

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
    DE_GUARD(this);

    DE_NOTIFY(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion().clear();
    deindex();
}

String ImageFile::describe() const
{
    String desc = Stringf(
        "image \"%s\"", d->filter == NoFilter ? name().c_str() : d->filterSource().name().c_str());
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

    case ColorSolid:
        desc += " (filter: solid color " + d->filterParameter + ")";
        break;

    case ColorMultiply:
        desc += " (filter: multiply with color " + d->filterParameter + ")";
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
            const String refPath = d->filterSource().path().fileNamePath();
            Image factorImg = App::rootFolder().locate<ImageFile const>
                    (refPath / d->filterParameter).image();

            if (img.size() != factorImg.size())
            {
                throw FilterError("ImageFile::image",
                                  stringf("Cannot multiply %s and %s due to different sizes",
                                          d->filterSource().path().c_str(),
                                          (refPath / d->filterParameter).c_str()));
            }

            img = img.multiplied(factorImg);
        }
        else if (d->filter == ColorDesaturate)
        {
            img = img.colorized(Image::Color(255, 255, 255, 255));
        }
        else if (d->filter == ColorSolid ||
                 d->filter == ColorMultiply)
        {
            // Parse the parameters.
            const auto components = d->filterParameter.split(',');
            duint8 colorValues[4] {0, 0, 0, 255};
            for (dsize i = 0; i < 4; ++i)
            {
                if (i < components.size())
            {
                    colorValues[i] = duint8(components[i].toUInt32());
                }
            }

            const Image::Color paramColor{
                colorValues[0], colorValues[1], colorValues[2], colorValues[3]};

            if (d->filter == ColorSolid)
            {
                img.fill(paramColor);
            }
            else
            {
                img = img.multiplied(paramColor);
            }
        }
        return img;
    }
    else
    {
        Image img = Image::fromData(*source(), extension());
        if (source()->name().contains("@2x.", CaseInsensitive))
        {
            img.setPointRatio(.5f);
        }
        return img;
    }
}

const IIStream &ImageFile::operator >> (IByteArray &bytes) const
{
    // The source file likely supports streaming the raw data.
    *source() >> bytes;
    return *this;
}

const filesys::Node *ImageFile::tryGetChild(const String &name) const
{
    if (!name.compareWithoutCase(HEIGHTMAP_TO_NORMALS))
    {
        return d->makeOrGetFiltered(HeightMapToNormals);
    }
    else if (name.beginsWith(MULTIPLY, CaseInsensitive))
    {
        /// @bug Different filter parameters should be saved as unique ImageFiles,
        /// or otherwise the latest accessed parameter is in effect for all multiplied
        /// instances. -jk
        ImageFile *filtered = d->makeOrGetFiltered(Multiply);
        filtered->d->filterParameter = name.substr(MULTIPLY.sizeb());
        return filtered;
    }
    else if (!name.compareWithoutCase(COLOR_DESATURATE))
    {
        return d->makeOrGetFiltered(ColorDesaturate);
    }
    else if (name.beginsWith(COLOR_SOLID, CaseInsensitive))
    {
        ImageFile *filtered = d->makeOrGetFiltered(ColorSolid);
        filtered->d->filterParameter = name.substr(COLOR_SOLID.sizeb());
        return filtered;
    }
    else if (name.beginsWith(COLOR_MULTIPLY, CaseInsensitive))
    {
        ImageFile *filtered = d->makeOrGetFiltered(ColorMultiply);
        filtered->d->filterParameter = name.substr(COLOR_MULTIPLY.sizeb());
        return filtered;
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
