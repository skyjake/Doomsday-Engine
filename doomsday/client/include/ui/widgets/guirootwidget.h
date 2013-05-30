/** @file guirootwidget.h  Graphical root widget.
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

#ifndef CLIENT_GUIROOTWIDGET_H
#define CLIENT_GUIROOTWIDGET_H

#include <de/RootWidget>
#include <de/AtlasTexture>
#include <de/GLShaderBank>
#include <de/GLUniform>
#include <de/Matrix>

class ClientWindow;

/**
 * Graphical root widget.
 * @ingroup gui
 */
class GuiRootWidget : public de::RootWidget
{
public:
    GuiRootWidget(ClientWindow *window = 0);

    /**
     * Sets the window in which the root widget resides.
     *
     * @param window  Client window instance.
     */
    void setWindow(ClientWindow *window);

    /**
     * Returns the window in which the root widget resides.
     */
    ClientWindow &window();

    de::AtlasTexture &atlas();
    de::GLUniform &uAtlas();
    de::Id solidWhitePixel() const;
    de::Id roundCorners() const;
    de::Id gradientFrame() const;

    static de::GLShaderBank &shaders();

    /**
     * Returns the default projection for 2D graphics.
     */
    de::Matrix4f projMatrix2D() const;

    // Events.
    void update();

private:
    DENG2_PRIVATE(d)
};

#endif // CLIENT_GUIROOTWIDGET_H
