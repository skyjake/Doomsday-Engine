/** @file idtech1image.h
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

#ifndef DENG_CLIENT_RESOURCE_IDTECH1IMAGE_H
#define DENG_CLIENT_RESOURCE_IDTECH1IMAGE_H

#include <de/Image>
#include <doomsday/LumpCatalog>

class Game;

/**
 * Image that imports its content using Id Tech 1 graphics formats.
 */
class IdTech1Image : public de::Image
{
public:
    enum Format {
        Automatic,
        RawVGAScreen,
        Patch
    };

public:
    /**
     * Constructs a new Id Tech 1 image. The Image object gets initialized with the
     * RGBA_8888 contents of the image.
     *
     * @param data     Image data. Format detected automatically.
     * @param palette  RGB palette containing color triplets. The size of the palette
     *                 must be big enough to contain all the color indices used in the
     *                 image data.
     */
    IdTech1Image(de::IByteArray const &data, de::IByteArray const &palette,
                 Format format = Automatic);

    de::Vec2i origin() const;

    /**
     * Size of the image data as declared in its metadata. May not match the actual
     * image size.
     */
    Size nominalSize() const;

public:
    enum LogoFlag
    {
        UnmodifiedAppearance = 0,
        ColorizedByFamily    = 0x1,
        Downscale50Percent   = 0x2,
        NullImageIfFails     = 0x4, // by default returns a small fallback image
        AlwaysTryLoad        = 0x8,

        DefaultLogoFlags     = ColorizedByFamily | Downscale50Percent,
    };
    Q_DECLARE_FLAGS(LogoFlags, LogoFlag)

    /**
     * Prepares a game logo image to be used in items. The image is based on the
     * game's title screen image in its WAD file(s).
     *
     * @param game     Game.
     * @param catalog  Catalog of selected lumps.
     *
     * @return Image.
     *
     * @todo This could be moved to a better location / other class. -jk
     */
    static de::Image makeGameLogo(Game const &game, res::LumpCatalog const &catalog,
                                  LogoFlags flags = DefaultLogoFlags);

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(IdTech1Image::LogoFlags)

#endif // DENG_CLIENT_RESOURCE_IDTECH1IMAGE_H
