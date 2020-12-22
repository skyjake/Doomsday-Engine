/** @file clientstyle.cpp  Client UI style.
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

#include "ui/clientstyle.h"
#include "ui/clientwindow.h"

#include <doomsday/res/idtech1image.h>
#include <de/async.h>

using namespace de;

DE_PIMPL_NOREF(ClientStyle)
{};

ClientStyle::ClientStyle() : d(new Impl)
{}

GuiWidget *ClientStyle::sharedBlurWidget() const
{
    if (!ClientWindow::mainExists()) return nullptr;
    return &ClientWindow::main().taskBarBlur();
}

Image ClientStyle::makeGameLogo(const Game &game, const res::LumpCatalog &catalog, LogoFlags flags)
{
    try
    {
        if (game.isPlayableWithDefaultPackages())
        {
            const Block playPal  = catalog.read("PLAYPAL");
            const Block title    = catalog.read("TITLE");
            const Block titlePic = catalog.read("TITLEPIC");
            const Block interPic = catalog.read("INTERPIC");

            const Block *dataToUse = &(title ? title : (titlePic ? titlePic : interPic));

            // Maybe it's a modern image format?
            Image logoImage = Image::fromData(*dataToUse);

            if (logoImage.isNull())
            {
                // Try a raw image or graphic patch instead.
                res::IdTech1Image img(*dataToUse, playPal);
                logoImage = Image::fromRgbaData(img.pixelSize(), img.pixels());
            }

            const int sizeDiv = flags & Downscale50Percent ? 2 : 1;
            const Image::Size finalSize(    logoImage.size().x        / sizeDiv,
                                        int(logoImage.size().y * 1.2f / sizeDiv)); // VGA aspect

            logoImage.resize(finalSize);

            if (flags & ColorizedByFamily)
            {
                const String colorId = "home.icon." +
                        (game.family().isEmpty()? "other" : game.family());
                return logoImage.colorized(Style::get().colors().color(colorId));
            }
            return logoImage;
        }
    }
    catch (const Error &er)
    {
        if (flags & NullImageIfFails) return Image();

        LOG_RES_WARNING("Failed to load title picture for game \"%s\": %s")
                << game.title()
                << er.asText();
    }
    if (flags & NullImageIfFails)
    {
        return Image();
    }
    // Use a generic logo, some files are missing.
    Image img(Image::Size(64, 64), Image::RGBA_8888);
    img.fill(Image::Color(0, 0, 0, 255));
    return img;
}

void ClientStyle::performUpdate()
{
    // We'll use de::async since the thread will just be sleeping.
    async([]() {
        // Wait until all UI assets are finished, and thus we can sure that no background
        // operations are accessing style assets.
        LOG_MSG("Waiting for UI assets to be ready...");
        ClientWindow::main().root().waitForAssetsReady();
        return 0;
    },
    [this](int) {
        LOG_MSG("Updating the UI style");
        Style::performUpdate();
    });
}
