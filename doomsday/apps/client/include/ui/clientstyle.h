/** @file clientstyle.h  Client UI style.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_UI_CLIENTSTYLE_H
#define DE_CLIENT_UI_CLIENTSTYLE_H

#include <de/image.h>
#include <de/ui/style.h>
#include <de/guiwidget.h>
#include <de/ui/stylist.h>
#include <doomsday/game.h>
#include <doomsday/res/lumpcatalog.h>

class ClientStyle : public de::Style
{
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
    using LogoFlags = de::Flags;

public:
    ClientStyle();

    de::GuiWidget *sharedBlurWidget() const override;

    void performUpdate() override;

    /**
     * Prepares a game logo image to be used in items. The image is based on the
     * game's title screen image in its WAD file(s).
     *
     * @param game     Game.
     * @param catalog  Catalog of selected lumps.
     *
     * @return Lgoo image.
     */
    static de::Image makeGameLogo(const Game &            game,
                                  const res::LumpCatalog &catalog,
                                  LogoFlags               flags = DefaultLogoFlags);

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_UI_CLIENTSTYLE_H
