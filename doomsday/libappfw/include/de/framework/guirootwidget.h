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

#ifndef LIBAPPFW_GUIROOTWIDGET_H
#define LIBAPPFW_GUIROOTWIDGET_H

#include <de/RootWidget>
#include <de/AtlasTexture>
#include <de/GLShaderBank>
#include <de/GLUniform>
#include <de/Matrix>
#include <de/CanvasWindow>

#include "../libappfw.h"

namespace de {

class GuiWidget;

/**
 * Graphical root widget.
 *
 * @ingroup gui
 */
class LIBAPPFW_PUBLIC GuiRootWidget : public RootWidget
{
public:
    GuiRootWidget(CanvasWindow *window = 0);

    /**
     * Sets the window in which the root widget resides.
     *
     * @param window  Client window instance.
     */
    void setWindow(CanvasWindow *window);

    /**
     * Returns the window in which the root widget resides.
     */
    CanvasWindow &window();

    AtlasTexture &atlas();
    GLUniform &uAtlas();
    Id solidWhitePixel() const;
    Id roundCorners() const;
    Id gradientFrame() const;
    Id borderGlow() const;
    Id toggleOnOff() const;
    Id tinyDot() const;

    static GLShaderBank &shaders();

    /**
     * Returns the default projection for 2D graphics.
     */
    Matrix4f projMatrix2D() const;

    void routeMouse(Widget *routeTo);

    bool processEvent(Event const &event);

    /**
     * Finds the widget that occupies the given point, looking through the entire tree. The
     * returned widget is the one that will first handle a received event associated with this
     * position.
     *
     * @param pos  Position in the view.
     *
     * @return  Widget, or @c NULL if none were found.
     */
    GuiWidget const *globalHitTest(Vector2i const &pos) const;

    // Events.
    void update();
    void draw();

    /**
     * Draws until the widget @a until is encountered during tree notification.
     *
     * @param until  Widget to stop drawing at. @a until is not drawn.
     */
    void drawUntil(Widget &until);

    /**
     * Adds a widget over all others.
     *
     * @param widget  Widget to add on top.
     */
    virtual void addOnTop(GuiWidget *widget);

    /**
     * Sends the current mouse position as a mouse event, just like the mouse would've
     * been moved.
     */
    virtual void dispatchLatestMousePosition();

    /**
     * If the event is not used by any widget, this will be called so the application may
     * still handle the event for other, non-widget-related purposes. No widget will be
     * offered the event after this is called.
     *
     * @param event  Event.
     */
    virtual void handleEventAsFallback(Event const &event);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_GUIROOTWIDGET_H
