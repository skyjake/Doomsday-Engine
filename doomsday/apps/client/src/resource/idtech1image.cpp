/** @file idtech1image.cpp
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "resource/idtech1image.h"

#include <doomsday/resource/patch.h>
#include <doomsday/Game>

#include <de/Style>

using namespace de;
using namespace res;

DENG2_PIMPL_NOREF(IdTech1Image)
{
    Vector2i origin;
    Size nominalSize;
};

IdTech1Image::IdTech1Image(IByteArray const &data, IByteArray const &palette, Format format)
    : d(new Impl)
{
    Size const rawSize(320, 200);

    if(format == Automatic)
    {
        // Try to guess which format the data uses.
        if(data.size() == rawSize.x * rawSize.y)
        {
            format = RawVGAScreen;
        }
        else
        {
            format = Patch;
        }
    }

    if(format == RawVGAScreen)
    {
        d->nominalSize = rawSize;
        Image::operator = (fromIndexedData(rawSize, data, palette));
    }
    else
    {
        auto const metadata = Patch::loadMetadata(data);
        d->nominalSize = metadata.logicalDimensions;
        d->origin      = metadata.origin;

        Image::operator = (Image::fromMaskedIndexedData(metadata.dimensions,
                                                        Patch::load(data), palette));
    }
}

Vector2i IdTech1Image::origin() const
{
    return d->origin;
}

Image::Size IdTech1Image::nominalSize() const
{
    return d->nominalSize;
}

Image IdTech1Image::makeGameLogo(Game const &game,
                                 res::LumpCatalog const &catalog,
                                 LogoFlags flags)
{
    try
    {
        if (flags.testFlag(AlwaysTryLoad) || game.isPlayableWithDefaultPackages())
        {
            const Block playPal  = catalog.read("PLAYPAL");
            const Block title    = catalog.read("TITLE");
            const Block titlePic = catalog.read("TITLEPIC");
            const Block interPic = catalog.read("INTERPIC");

            const Block *imageToUse = &(title ? title : (titlePic ? titlePic : interPic));

            // Maybe it's a modern image format?
            Image logoImage = Image::fromData(*imageToUse);

            if (logoImage.isNull())
            {
                // Try a raw image or graphic patch instead.
                logoImage = IdTech1Image(*imageToUse, playPal);
            }

            const float scaleFactor = flags.testFlag(Downscale50Percent)? .5f : 1.f;
            const Image::Size finalSize(logoImage.width()  * scaleFactor,
                                        logoImage.height() * scaleFactor * 1.2f); // VGA aspect
            logoImage = logoImage.toQImage().scaled(
                finalSize.x, finalSize.y, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

            if (flags & ColorizedByFamily)
            {
                String const colorId = "home.icon." +
                        (game.family().isEmpty()? "other" : game.family());
                return logoImage.colorized(Style::get().colors().color(colorId));
            }
            return logoImage;
        }
    }
    catch (Error const &er)
    {
        if (flags & NullImageIfFails) return Image();

        LOG_RES_WARNING("Failed to load title picture for game \"%s\" using {%s}: %s")
                << game.title()
                << String::join(catalog.packages(), " ")
                << er.asText();
    }
    if (flags & NullImageIfFails)
    {
        return Image();
    }
    // Use a generic logo, some files are missing.
    QImage img(64, 64, QImage::Format_ARGB32);
    img.fill(Qt::black);
    return img;
}
