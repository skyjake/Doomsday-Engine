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
#include <QObject>

#include "ui/style.h"

class GuiRootWidget;
class BlurWidget;

/**
 * Base class for graphical widgets.
 *
 * Each GuiWidget has one RuleRectangle that defines the widget's position in
 * the view. However, all widgets are allowed to draw outside this rectangle
 * and react to events occurring outside it. In essence, all widgets thus cover
 * the entire view area and can be thought as a (hierarchical) stack.
 *
 * GuiWidget is the base class for all widgets of a GUI application. However, a
 * GuiWidget does not necessarily need to have a visible portion on the screen:
 * its entire purpose may be to handle events, for example.
 *
 * The common features GuiWidget offers to all widgets are:
 *
 * - Background geometry builder. All widgets may use this to build geometry for
 *   the background of the widget. However, widgets are also allowed to fully
 *   generate all of their geometry from scratch.
 *
 * - Access to the UI Style.
 *
 * - GuiWidget can be told which font and text color to use using a style
 *   definition identifier (e.g., "editor.hint"). These style elements are then
 *   conveniently accessible using methods of GuiWidget.
 *
 * - Opacity property. Opacities respect the hierarchical organization of
 *   widgets: GuiWidget::visibleOpacity() returns the opacity of a particular
 *   widget where all the parent widgets' opacities are factored in.
 *
 * - Hit testing: checking if a view space point should be considered to be
 *   inside the widget. The default implementation simply checks if the point is
 *   inside the widget's rectangle. Derived classes may override this to adapt
 *   hit testing to their particular visual shape.
 *
 * - Logic for handling more complicated interactions such as a mouse pointer
 *   click (press then release inside or outside).
 *
 * QObject is a base class for the signals and slots capabilities.
 *
 * @ingroup gui
 */
class GuiWidget : public QObject, public de::Widget
{
public:
    /**
     * Properties of the widget's background's apperance.
     * GuiWidget::glMakeGeometry() uses this to construct the background
     * geometry of the widget.
     */
    struct Background {
        enum Type {
            None,               ///< No background or solid fill.
            GradientFrame,      ///< Use the "gradient frame" from the UI atlas.
            Blurred,            ///< Blurs whatever is showing behind the widget.
            SharedBlur          ///< Use the blur background from a BlurWidget.
        };
        de::Vector4f solidFill; ///< Always applied if opacity > 0.
        Type type;
        de::Vector4f color;     ///< Secondary color.
        float thickness;        ///< Frame border thickenss.
        BlurWidget *blur;

        Background()
            : type(None), thickness(0), blur(0) {}

        Background(BlurWidget &blurred, de::Vector4f const &blurColor)
            : solidFill(blurColor), type(SharedBlur), thickness(0), blur(&blurred) {}

        Background(de::Vector4f const &solid, Type t = None)
            : solidFill(solid), type(t), thickness(0), blur(0) {}

        Background(Type t, de::Vector4f const &borderColor, float borderThickness = 0)
            : type(t), color(borderColor), thickness(borderThickness), blur(0) {}

        Background(de::Vector4f const &solid, Type t,
                   de::Vector4f const &borderColor,
                   float borderThickness = 0)
            : solidFill(solid), type(t), color(borderColor), thickness(borderThickness),
              blur(0) {}
    };

    typedef de::Vertex2TexRgba DefaultVertex;
    typedef de::GLBufferT<DefaultVertex> DefaultVertexBuf;

public:
    GuiWidget(de::String const &name = "");

    GuiRootWidget &root();
    GuiRootWidget &root() const;
    Style const &style() const;

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

    de::Rectanglef normalizedRect() const;

    void deleteLater();

    void setFont(de::DotPath const &id);
    void setTextColor(de::DotPath const &id);
    void setMargin(de::DotPath const &id);
    void set(Background const &bg);

    de::Font const &font() const;
    de::ColorBank::Color textColor() const;
    de::ColorBank::Colorf textColorf() const;
    de::Rule const &margin() const;

    /**
     * Determines whether the contents of the widget are supposed to be clipped
     * to its boundaries. The Widget::ContentClipping behavior flag is used for
     * storing this information.
     */
    bool clipped() const;

    Background const &background() const;

    /**
     * Sets the opacity of the widget. Child widgets' opacity is also affected.
     *
     * @param opacity     Opacity.
     * @param span        Animation transition span.
     * @param startDelay  Starting delay.
     */
    void setOpacity(float opacity, de::TimeDelta span = 0, de::TimeDelta startDelay = 0);

    /**
     * Determines the widget's current opacity.
     */
    float opacity() const;

    /**
     * Determines the widget's opacity, factoring in all ancestor opacities.
     */
    float visibleOpacity() const;

    // Events.
    void initialize();
    void deinitialize();
    void viewResized();
    void update();
    void draw() /*final*/;

    /**
     * Determines if the widget occupies on-screen position @a pos.
     *
     * @param pos  Coordinates.
     *
     * @return @c true, if hit.
     */
    virtual bool hitTest(de::Vector2i const &pos) const;

    bool hitTest(de::Event const &event) const;

    enum MouseClickStatus {
        MouseClickUnrelated, ///< Event was not related to mouse clicks.
        MouseClickStarted,
        MouseClickFinished,
        MouseClickAborted
    };

    MouseClickStatus handleMouseClick(de::Event const &event);

protected:
    virtual void addedChildWidget(Widget &widget) /*final*/;
    virtual void removedChildWidget(Widget &widget) /*final*/;

    virtual void addedChildWidget(GuiWidget &widget);
    virtual void removedChildWidget(GuiWidget &widget);

    /**
     * Called by GuiWidget::update() the first time an update is being carried
     * out. Native GL is guaranteed to be available at this time, so the widget
     * must allocate all its GL resources during this method. Note that widgets
     * cannot always allocate GL resources during their constructors because GL
     * may not be initialized yet at that time.
     */
    virtual void glInit();

    /**
     * Called by GuiWidget before the widget is destroyed. This is the
     * appropriate place for the widget to release its GL resources. If one
     * waits until the widget's destructor to do so, it may already have lost
     * access to some required information (such as the root widget).
     */
    virtual void glDeinit();

    /**
     * Called by GuiWidget when it is time to draw the widget's content. A
     * clipping scissor is automatically set before this is called. Derived
     * classes should override this instead of the draw() method.
     *
     * This is not called if the widget's visible opacity is zero or the widget
     * is hidden.
     */
    virtual void drawContent();

    void drawBlurredRect(de::Rectanglei const &rect, de::Vector4f const &color);

    /**
     * Requests the widget to refresh its geometry, if it has any static
     * geometry. Normally this does not need to be called. It is provided
     * mostly as a way for subclasses to ensure that geometry is up to date
     * when they need it.
     *
     * @param yes  @c true to request, @c false to cancel the request.
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

    /**
     * Called during GuiWidget::update() whenever the style of the widget has
     * been marked as changed.
     */
    virtual void updateStyle();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_GUIWIDGET_H
