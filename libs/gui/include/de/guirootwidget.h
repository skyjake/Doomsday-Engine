/** @file guirootwidget.h  Graphical root widget.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_GUIROOTWIDGET_H
#define LIBAPPFW_GUIROOTWIDGET_H

#include <de/rootwidget.h>
#include <de/matrix.h>
#include <de/atlastexture.h>
#include <de/glshaderbank.h>
#include <de/gluniform.h>
#include <de/glwindow.h>

namespace de {

class AnimationVector2;
class FocusWidget;
class GuiWidget;
class Painter;

/**
 * Graphical root widget.
 *
 * @ingroup appfw
 */
class LIBGUI_PUBLIC GuiRootWidget : public RootWidget
{
public:
    GuiRootWidget(GLWindow *window = nullptr);

    /**
     * Sets the window in which the root widget resides.
     *
     * @param window  Client window instance.
     */
    void setWindow(GLWindow *window);

    /**
     * Returns the window in which the root widget resides.
     */
    GLWindow &window();

    AtlasTexture &atlas();
    GLUniform &uAtlas();
    Id solidWhitePixel() const;
    Id solidRoundCorners() const;
    Id roundCorners() const;
    Id boldRoundCorners() const;
    Id borderGlow() const;
    Id tinyDot() const;

    /**
     * Gets the identifier of a style image allocated on the shared UI atlas texture.
     *
     * @param styleImagePath  Path of the style image in the style's image bank.
     *
     * @return Id of the texture allocation.
     */
    Id styleTexture(const DotPath &styleImagePath) const;

    static GLShaderBank &shaders();

    Painter &painter();

    /**
     * Returns the default projection for 2D graphics.
     */
    Mat4f projMatrix2D() const;

    AnimationVector2 &rootOffset();

    void routeMouse(Widget *routeTo);

    bool processEvent(const Event &event);

    /**
     * Finds the widget that occupies the given point, looking through the entire tree. The
     * returned widget is the one that will first handle a received event associated with this
     * position.
     *
     * @param pos  Position in the view.
     *
     * @return  Widget, or @c NULL if none were found.
     */
    const GuiWidget *globalHitTest(const Vec2i &pos) const;

    const GuiWidget *guiFind(const String &name) const;

    FocusWidget &focusIndicator();

    GuiWidget *focus() const;

    /**
     * Pushes a pointer to the current focused widget to a stack. It can later be
     * restored with popFocus().
     */
    void pushFocus();

    void popFocus();

    void clearFocusStack();

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
     * Reorders the children of the root widget so that @a widget is added to the top.
     *
     * @param widget  Widget to move to the top.
     */
    void moveToTop(GuiWidget &widget);

    /**
     * If the event is not used by any widget, this will be called so the application may
     * still handle the event for other, non-widget-related purposes. No widget will be
     * offered the event after this is called.
     *
     * @param event  Event.
     */
    virtual void handleEventAsFallback(const Event &event);

protected:
    virtual void loadCommonTextures();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_GUIROOTWIDGET_H
