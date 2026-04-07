/** @file guiwidget.h  Base class for graphical widgets.
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

#ifndef LIBAPPFW_GUIWIDGET_H
#define LIBAPPFW_GUIWIDGET_H

#include <de/widget.h>
#include <de/rulerectangle.h>
#include <de/mouseevent.h>
#include <de/glbuffer.h>
#include <de/painter.h>

#include "ui/defs.h"
#include "ui/margins.h"
#include "ui/style.h"
#include "guiwidgetprivate.h"

namespace de {

class GuiRootWidget;
class BlurWidget;
class PopupWidget;

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
 * - Automatically saving and restoring persistent state. Classes that implement
 *   IPersistent will automatically be saved and restored when the widget is
 *   (de)initialized.
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
 *   click (press then release inside or outside), and passing received events
 *   to separately registered event handler objects.
 *
 * QObject is a base class for the signals and slots capabilities.
 *
 * @note Always use GuiWidget::destroy() to delete any GUI widget. It will
 * ensure that the widget is properly deinitialized before destruction.
 *
 * @ingroup appfw
 */
class LIBGUI_PUBLIC GuiWidget : public Widget
{
public:
    /**
     * Properties of the widget's background's apperance.
     * GuiWidget::glMakeGeometry() uses this to construct the background
     * geometry of the widget.
     *
     * @todo Refactor: it should be possible to apply any combination of these
     * in a single widget; use a dynamic array of effects. Base it on ProceduralImage.
     */
    struct Background {
        enum Type {
            None,                         ///< No background, no solid fill.
            GradientFrame,                ///< Bold round corners, square background.
            GradientFrameWithRoundedFill, ///< Bold round corners with solid rounded background.
            GradientFrameWithThinBorder,  ///< Bold round corners, black thin secondary border.
            BorderGlow,                   ///< Border glow with specified color/thickness.
            Blurred,                      ///< Blurs whatever is showing behind the widget.
            BlurredWithBorderGlow,
            BlurredWithSolidFill,
            SharedBlur,                   ///< Use the blur background from a BlurWidget.
            SharedBlurWithBorderGlow,
            Rounded
        };
        Vec4f      solidFill; ///< Always applied if opacity > 0.
        Type       type;
        Vec4f      color;     ///< Secondary color.
        float      thickness; ///< Frame border thickenss.
        GuiWidget *blur = nullptr;

        Background()
            : type(None), thickness(0) {}

        Background(GuiWidget &blurred, const Vec4f &blurColor)
            : solidFill(blurColor), type(SharedBlur), thickness(0), blur(&blurred) {}

        Background(const Vec4f &solid, Type t = None)
            : solidFill(solid), type(t), thickness(0) {}

        Background(Type t, const Vec4f &borderColor, float borderThickness = 0)
            : type(t), color(borderColor), thickness(borderThickness) {}

        Background(const Vec4f &solid, Type t,
                   const Vec4f &borderColor,
                   float borderThickness = 0)
            : solidFill(solid), type(t), color(borderColor), thickness(borderThickness) {}

        inline Background withSolidFill(const Vec4f &newSolidFill) const {
            Background bg = *this;
            bg.solidFill = newSolidFill;
            return bg;
        }

        inline Background withSolidFillOpacity(float opacity) const {
            Background bg = *this;
            bg.solidFill.w = opacity;
            return bg;
        }
    };

    typedef Vertex2TexRgba DefaultVertex;
    typedef GLBufferT<DefaultVertex> DefaultVertexBuf;
    typedef List<GuiWidget *> Children;

    /**
     * Handles events.
     */
    class IEventHandler
    {
    public:
        virtual ~IEventHandler() = default;

        /**
         * Handle an event.
         *
         * @param widget  Widget that received the event.
         * @param event   Event.
         *
         * @return @c true, if the event was eaten. @c false otherwise.
         */
        virtual bool handleEvent(GuiWidget &widget, const Event &event) = 0;
    };

    enum Attribute
    {
        /**
         * Enables or disables automatic state serialization for widgets
         * derived from IPersistent. State serialization occurs when the widget
         * is gl(De)Init'd.
         */
        RetainStatePersistently = 0x1,

        AnimateOpacityWhenEnabledOrDisabled = 0x2,

        /**
         * Widget will not automatically change opacity depending on state
         * (e.g., when disabled).
         */
        ManualOpacity = 0x10,

        /**
         * Widget will automatically change opacity depending on state. This overrides
         * ManualOpacity has family behavior.
         */
        AutomaticOpacity = 0x200,

        /**
         * Prevents the drawing of the widget contents even if it visible. The
         * texture containing the blurred background is updated regardless.
         */
        DontDrawContent = 0x4,

        /**
         * Visible opacity determined solely by the widget itself, not affected
         * by ancestors.
         */
        IndependentOpacity = 0x8,

        /**
         * When focused, don't show the normal focus indicator. The assumption
         * is that the widget will indicate focused state on its own.
         */
        FocusHidden = 0x20,

        /**
         * All received mouse events are eaten. Derived classes may handle the
         * events beforehand, though.
         */
        EatAllMouseEvents = 0x40,

        /**
         * When the widget is in focus, this will prevent cycling focus away with Tab.
         */
        FocusCyclingDisabled = 0x80,

        /**
         * When the widget is in focus, this will prevent moving the focus with
         * arrow keys.
         */
        FocusMoveWithArrowKeysDisabled = 0x100,

        /// Set of attributes that apply to all descendants.
        FamilyAttributes = ManualOpacity | AnimateOpacityWhenEnabledOrDisabled,

        /// Default set of attributes.
        DefaultAttributes = RetainStatePersistently | AnimateOpacityWhenEnabledOrDisabled
    };
    using Attributes = Flags;

    enum ColorTheme { Normal, Inverted };

public:
    GuiWidget(const String &name = {});

    /**
     * Deletes a widget. The widget is first deinitialized.
     *
     * @param widget  Widget to destroy.
     */
    static void destroy(GuiWidget *widget);

    /**
     * Deletes a widget at a later point in time. However, the widget is
     * immediately deinitialized.
     *
     * @param widget  Widget to deinitialize now and destroy layer.
     */
    static void destroyLater(GuiWidget *widget);

    GuiRootWidget &root() const;
    Children childWidgets() const;
    GuiWidget *parentGuiWidget() const;
    const Style &style() const;

    /**
     * Shortcut for accessing individual rules in the active UI style.
     * @param path  Identifier of the rule.
     * @return Rule from the Style.
     */
    const Rule &rule(const DotPath &path) const;

    /**
     * Returns the rule rectangle that defines the placement of the widget on
     * the target canvas.
     */
    RuleRectangle &rule();

    Rectanglei contentRect() const;

    /**
     * Returns the rule rectangle that defines the placement of the widget on
     * the target canvas.
     */
    const RuleRectangle &rule() const;

    /**
     * Calculates an estimate of the height of the widget. Widgets used in virtualized
     * lists should implement this and return an accurate value.
     *
     * By default returns the current height of the widget rectangle.
     */
    virtual float estimatedHeight() const;

    ui::Margins &margins();
    const ui::Margins &margins() const;

    Rectanglef normalizedRect() const;
    Rectanglef normalizedRect(const Rectanglei &viewSpaceRect) const;

    /**
     * Normalized content rectangle. Same as normalizedRect() except margins
     * are applied to all sides.
     */
    Rectanglef normalizedContentRect() const;

    void setFont(const DotPath &id);
    virtual void setTextColor(const DotPath &id);
    void set(const Background &bg);
    void setSaturation(float saturation);

    const Font &font() const;
    const DotPath &fontId() const;
    const DotPath &textColorId() const;
    ColorBank::Color textColor() const;
    ColorBank::Colorf textColorf() const;

    /**
     * Determines whether the contents of the widget are supposed to be clipped
     * to its boundaries. The Widget::ContentClipping behavior flag is used for
     * storing this information.
     */
    bool isClipped() const;

    const Background &background() const;

    /**
     * Sets the opacity of the widget. Child widgets' opacity is also affected.
     *
     * @param opacity     Opacity.
     * @param span        Animation transition span.
     * @param startDelay  Starting delay.
     */
    void setOpacity(float opacity, TimeSpan span = 0.0, TimeSpan startDelay = 0.0);

    /**
     * Determines the widget's opacity animation.
     */
    Animation opacity() const;

    /**
     * Determines the widget's opacity, factoring in all ancestor opacities.
     */
    float visibleOpacity() const;

    /**
     * Sets an object that will be offered events received by this widget. The
     * handler may eat the event. Any number of event handlers can be added;
     * they are called in the order of addition.
     *
     * @param handler  Event handler. Ownership given to GuiWidget.
     */
    void addEventHandler(IEventHandler *handler);

    void removeEventHandler(IEventHandler *handler);

    /**
     * Sets, unsets, or replaces one or more widget attributes.
     *
     * @param attr  Attribute(s) to modify.
     * @param op    Flag operation.
     */
    void setAttribute(const Attributes &attr, FlagOpArg op = SetFlags);

    /**
     * Returns this widget's attributes.
     */
    Attributes attributes() const;

    /**
     * Returns the attributes that apply to this widget a
     * @return
     */
    Attributes familyAttributes() const;

    /**
     * Save the state of the widget and all its children (those who support state
     * serialization).
     */
    void saveState();

    /**
     * Restore the state of the widget and all its children (those who support state
     * serialization).
     */
    void restoreState();

    // Events.
    void initialize() override;
    void deinitialize() override;
    void viewResized() override;
    void update() override;
    void draw() override final;
    void preDrawChildren() override;
    void postDrawChildren() override;
    bool handleEvent(const Event &event) override;

    /**
     * Determines if the widget occupies on-screen position @a pos.
     *
     * @param pos  Coordinates.
     *
     * @return @c true, if hit.
     */
    virtual bool hitTest(const Vec2i &pos) const;

    bool hitTest(const Event &event) const;

    /**
     * Checks if the position is on any of the children of this widget.
     *
     * @param pos  Coordinates.
     *
     * @return  The child that occupied the position in the view.
     */
    const GuiWidget *treeHitTest(const Vec2i &pos) const;

    /**
     * Returns the rule rectangle used for hit testing. Defaults to a rectangle
     * equivalent to GuiWidget::rule(). Modify the hit test rule to allow
     * widgets to be hittable outside their default boundaries.
     *
     * @return Hit test rule.
     */
    RuleRectangle &hitRule();

    const RuleRectangle &hitRule() const;

    enum MouseClickStatus {
        MouseClickUnrelated, ///< Event was not related to mouse clicks.
        MouseClickStarted,
        MouseClickFinished,
        MouseClickAborted
    };

    MouseClickStatus handleMouseClick(const Event &event,
                                      MouseEvent::Button button = MouseEvent::Left);
    
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

    bool isInitialized() const;

    bool canBeFocused() const override;

    GuiWidget *guiFind(const String &name);
    const GuiWidget *guiFind(const String &name) const;

    /**
     * Finds the popup widget that this widget resides in.
     * @return Popup, or @c nullptr if the widget is not inside a popup.
     */
    PopupWidget *findParentPopup() const;

    void collectUnreadyAssets(AssetGroup &collected,
                              CollectMode = CollectMode::OnlyVisible) override;

    /**
     * Puts the widget in garbage to be deleted at the next recycling.
     */
    void guiDeleteLater();

public:
    /**
     * Normalize a rectangle within a container rectangle.
     *
     * @param rect            Rectangle to normalize.
     * @param containerRect   Container rectangle to normalize in.
     *
     * @return Normalized rectangle.
     */
    static Rectanglef normalizedRect(const Rectanglei &rect,
                                     const Rectanglei &containerRect);

    static float pointsToPixels(float points);
    static float pixelsToPoints(float pixels);

    inline static int pointsToPixels(int points) {
        return int(pointsToPixels(float(points)));
    }

    inline static duint pointsToPixels(duint points) {
        return duint(pointsToPixels(float(points)));
    }

    template <typename Vector2>
    static Vector2 pointsToPixels(const Vector2 &type) {
        return Vector2(typename Vector2::ValueType(pointsToPixels(type.x)),
                       typename Vector2::ValueType(pointsToPixels(type.y)));
    }

    template <typename Vector2>
    static Vector2 pixelsToPoints(const Vector2 &type) {
        return Vector2(typename Vector2::ValueType(pixelsToPoints(type.x)),
                       typename Vector2::ValueType(pixelsToPoints(type.y)));
    }

    static ColorTheme invertColorTheme(ColorTheme theme);

    /**
     * Immediately deletes all the widgets in the garbage. This is useful to
     * avoid double deletion in case a trashed widget's parent is deleted
     * before recycling occurs.
     */
    static void recycleTrashedWidgets();

protected:
    /**
     * Called by GuiWidget::update() the first time an update is being carried
     * out. Native GL is guaranteed to be available at this time, so the widget
     * must allocate all its GL resources during this method. Note that widgets
     * cannot always allocate GL resources during their constructors because GL
     * may not be initialized yet at that time.
     */
    virtual void glInit();

    /**
     * Called from deinitialize(). Deinitialization must occur before the
     * widget is destroyed. This is the appropriate place for the widget to
     * release its GL resources. If one waits until the widget's destructor to
     * do so, it may already have lost access to some required information
     * (such as the root widget, or derived classes' private instances).
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

    void drawBlurredRect(const Rectanglei &rect, const Vec4f &color, float opacity = 1.0f);

    /**
     * Extensible mechanism for derived widgets to build their geometry. The
     * assumptions with this are 1) the vertex format is Vertex2TexRgba, 2)
     * the shared UI atlas is used, and 3) the background is automatically
     * built by GuiWidget's implementation of the function.
     *
     * @param verts  Vertex builder.
     */
    virtual void glMakeGeometry(GuiVertexBuilder &verts);

    /**
     * Checks if the widget's rectangle has changed.
     *
     * @param currentPlace  The widget's current placement is returned here.
     *
     * @return @c true, if the place of the widget has changed since the
     * last call to hasChangedPlace(); otherwise, @c false.
     */
    bool hasChangedPlace(Rectanglei &currentPlace);

    /**
     * Determines whether update() has been called at least once on the widget.
     * Before this the widget has not been seen on screen, so it can be manipulated
     * without visible artifacts.
     *
     * @return @c true, if the wdiget has been updated.
     */
    bool hasBeenUpdated() const;

    /**
     * Called during GuiWidget::update() whenever the style of the widget has
     * been marked as changed.
     */
    virtual void updateStyle();

    /**
     * Returns the opacity animation of the widget.
     */
    Animation &opacityAnimation();

private:
    DE_PRIVATE(d)
};

template <typename WidgetType>
struct GuiWidgetDeleter {
    void operator () (WidgetType *w) {
        GuiWidget::destroy(w);
    }
};

typedef GuiWidget::Children GuiWidgetList;

template <typename WidgetType>
class UniqueWidgetPtr : public std::unique_ptr<WidgetType, GuiWidgetDeleter<WidgetType>> {
public:
    UniqueWidgetPtr(WidgetType *w = nullptr)
        : std::unique_ptr<WidgetType, GuiWidgetDeleter<WidgetType>>(w) {}
};

} // namespace de

#endif // LIBAPPFW_GUIWIDGET_H
