/** @file guiwidget.h  Base class for graphical widgets.
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

#ifndef DENG_CLIENT_GUIWIDGET_H
#define DENG_CLIENT_GUIWIDGET_H

#include <de/Widget>
#include <de/RuleRectangle>
#include <de/GLBuffer>

#include "ui/style.h"

class GuiRootWidget;

/**
 * Base class for graphical widgets.
 * @ingroup gui
 */
class GuiWidget : public de::Widget
{
public:
    struct Background {
        enum Type {
            None,
            GradientFrame
        };
        de::Vector4f solidFill; ///< Always applied if opacity > 0.
        Type type;
        de::Vector4f color;
        float thickness;

        Background() : type(None), thickness(0) {}
        Background(de::Vector4f const &solid) : solidFill(solid), type(None), thickness(0) {}
        Background(Type t, de::Vector4f const &borderColor, float borderThickness = 0)
            : type(t), color(borderColor), thickness(borderThickness) {}
        Background(de::Vector4f const &solid, Type t,
                   de::Vector4f const &borderColor, float borderThickness = 0)
            : solidFill(solid), type(t), color(borderColor), thickness(borderThickness) {}
    };

    typedef de::Vertex2TexRgba DefaultVertex;
    typedef de::GLBufferT<DefaultVertex> DefaultVertexBuf;

public:
    GuiWidget(de::String const &name = "");

    GuiRootWidget &root();
    Style const &style();

    /**
     * Returns the rule rectangle that defines the placement of the widget on
     * the target canvas.
     */
    de::RuleRectangle &rule();

    /**
     * Returns the rule rectangle that defines the placement of the widget on
     * the target canvas.
     */
    de::RuleRectangle const &rule() const;

    void deleteLater();

    void set(Background const &bg);

    Background const &background() const;

    // Events.
    void initialize();
    void deinitialize();
    void update();

    /**
     * Determines if the widget occupies on-screen position @a pos.
     *
     * @param pos  Coordinates.
     *
     * @return @c true, if hit.
     */
    virtual bool hitTest(de::Vector2i const &pos) const;

    enum MouseClickStatus {
        MouseClickUnrelated, ///< Event was not related to mouse clicks.
        MouseClickStarted,
        MouseClickFinished,
        MouseClickAborted
    };

    MouseClickStatus handleMouseClick(de::Event const &event);

protected:
    virtual void glInit();
    virtual void glDeinit();

    /**
     * Requests the widget to refresh its geometry, if it has any static
     * geometry. Normally this does not need to be called. It is provided
     * mostly as a way for subclasses to ensure that geometry is up to date
     * when they need it.
     *
     * @param yes
     */
    void requestGeometry(bool yes = true);

    bool geometryRequested() const;

    /**
     * Extensible mechanism for derived widgets to build their geometry. The
     * assumptions with this are 1) the vertex format is de::Vertex2TexRgba, 2)
     * the shared UI atlas is used, and 3) the background is automatically
     * built by GuiWidget's implementation of the function.
     *
     * @param verts  Vertex builder.
     */
    virtual void glMakeGeometry(DefaultVertexBuf::Builder &verts);

    /**
     * Checks if the widget's rectangle has changed.
     *
     * @param currentPlace  The widget's current placement is returned here.
     *
     * @return @c true, if the place of the widget has changed since the
     * last call to hasChangedPlace(); otherwise, @c false.
     */
    bool hasChangedPlace(de::Rectanglei &currentPlace);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_GUIWIDGET_H
