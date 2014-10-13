/** @file dd_ui.h  InFine animation system widgets.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DENG_UI_INFINE_WIDGETS_H
#define DENG_UI_INFINE_WIDGETS_H

#include <QList>
#include <de/animator.h>
#include <de/Error>
#include <de/Id>
#include <de/Observers>
#include "api_fontrender.h"
#include "api_svg.h"

#include "Material"
#include "ui/finaleinterpreter.h" // fi_objectid_t

class FinalePageWidget;

/**
 * Base class for Finale widgets.
 *
 * @ingroup infine
 */
class FinaleWidget
{
public:
    /// Notified when the InFine object is about to be deleted.
    DENG2_DEFINE_AUDIENCE(Deletion, void finaleWidgetBeingDeleted(FinaleWidget const &widget))

public:
    explicit FinaleWidget(de::String const &name = "");
    virtual ~FinaleWidget();

    DENG2_AS_IS_METHODS()

#ifdef __CLIENT__
    virtual void draw(de::Vector3f const &offset) = 0;
#endif
    virtual void runTicks(/*timespan_t timeDelta*/);

    /**
     * Returns the unique identifier of the widget.
     */
    de::Id id() const;

    /**
     * Returns the symbolic name of the widget.
     */
    de::String name() const;
    FinaleWidget &setName(de::String const &newName);

    animatorvector3_t const &origin() const;
    FinaleWidget &setOrigin(de::Vector3f const &newOrigin, int steps = 0);
    FinaleWidget &setOriginX(float newX, int steps = 0);
    FinaleWidget &setOriginY(float newY, int steps = 0);
    FinaleWidget &setOriginZ(float newZ, int steps = 0);

    animator_t const &angle() const;
    FinaleWidget &setAngle(float newAngle, int steps = 0);

    animatorvector3_t const &scale() const;
    FinaleWidget &setScale(de::Vector3f const &newScale, int steps = 0);
    FinaleWidget &setScaleX(float newScaleX, int steps = 0);
    FinaleWidget &setScaleY(float newScaleY, int steps = 0);
    FinaleWidget &setScaleZ(float newScaleZ, int steps = 0);

    /**
     * Returns the FinalePageWidget to which the widget is attributed (if any).
     */
    FinalePageWidget *page() const;

    /**
     * Change/setup a reverse link between this object and it's owning page.
     * @note Changing this relationship here does not complete the task of
     * linking an object with a page (not enough information). It is therefore
     * the page's responsibility to call this when adding/removing objects.
     */
    FinaleWidget &setPage(FinalePageWidget *newPage);

private:
    DENG2_PRIVATE(d)
};

/**
 * Finale animation widget. Colored rectangles or image sequence animations.
 *
 * @ingroup infine
 */
class FinaleAnimWidget : public FinaleWidget
{
public:
    /**
     * Describes a frame in the animation sequence.
     */
    struct Frame
    {
        enum Type
        {
            PFT_MATERIAL,
            PFT_PATCH,
            PFT_RAW, /// "Raw" graphic or PCX lump.
            PFT_XIMAGE /// External graphics resource.
        };

        int tics;
        Type type;
        struct Flags {
            char flip:1;
        } flags;
        union {
            Material *material;
            patchid_t patch;
            lumpnum_t lumpNum;
            DGLuint tex;
        } texRef;
        short sound;

        Frame();
        ~Frame();
    };
    typedef QList<Frame *> Frames;

public:
    FinaleAnimWidget(de::String const &name);
    virtual ~FinaleAnimWidget();

    /// @todo Observe instead.
    bool animationComplete() const;

    FinaleAnimWidget &setLooping(bool yes = true);

    int newFrame(Frame::Type type, int tics, void *texRef, short sound, bool flagFlipH);

    Frames const &allFrames() const;
    FinaleAnimWidget &clearAllFrames();

    inline int frameCount() const { return allFrames().count(); }

    FinaleAnimWidget &resetAllColors();

    animator_t const *color() const;
    FinaleAnimWidget &setColorAndAlpha(de::Vector4f const &newColorAndAlpha, int steps = 0);
    FinaleAnimWidget &setColor(de::Vector3f const &newColor, int steps = 0);
    FinaleAnimWidget &setAlpha(float newAlpha, int steps = 0);

    animator_t const *edgeColor() const;
    FinaleAnimWidget &setEdgeColorAndAlpha(de::Vector4f const &newColorAndAlpha, int steps = 0);
    FinaleAnimWidget &setEdgeColor(de::Vector3f const &newColor, int steps = 0);
    FinaleAnimWidget &setEdgeAlpha(float newAlpha, int steps = 0);

    animator_t const *otherColor() const;
    FinaleAnimWidget &setOtherColorAndAlpha(de::Vector4f const &newColorAndAlpha, int steps = 0);
    FinaleAnimWidget &setOtherColor(de::Vector3f const &newColor, int steps = 0);
    FinaleAnimWidget &setOtherAlpha(float newAlpha, int steps = 0);

    animator_t const *otherEdgeColor() const;
    FinaleAnimWidget &setOtherEdgeColorAndAlpha(de::Vector4f const &newColorAndAlpha, int steps = 0);
    FinaleAnimWidget &setOtherEdgeColor(de::Vector3f const &newColor, int steps = 0);
    FinaleAnimWidget &setOtherEdgeAlpha(float newAlpha, int steps = 0);

protected:
#ifdef __CLIENT__
    void draw(de::Vector3f const &offset);
#endif
    void runTicks();

private:
    DENG2_PRIVATE(d)
};

typedef FinaleAnimWidget::Frame FinaleAnimWidgetFrame;

/**
 * Finale text widget.
 *
 * @ingroup infine
 */
class FinaleTextWidget : public FinaleWidget
{
public:
    FinaleTextWidget(de::String const &name);
    virtual ~FinaleTextWidget();

    void accelerate();
    FinaleTextWidget &setCursorPos(int newPos);

    bool animationComplete() const;

    /**
     * Returns the total number of @em currently-visible characters, excluding control/escape
     * sequence characters.
     */
    int visLength();

    char const *text() const;
    FinaleTextWidget &setText(char const *newText);

    fontid_t font() const;
    FinaleTextWidget &setFont(fontid_t newFont);

    /// @return  @ref alignmentFlags
    int alignment() const;
    /// @param newAlignment  @ref alignmentFlags
    FinaleTextWidget &setAlignment(int newAlignment);

    float lineHeight() const;
    FinaleTextWidget &setLineHeight(float newLineHeight);

    int scrollRate() const;
    FinaleTextWidget &setScrollRate(int newRateInTics);

    int typeInRate() const;
    FinaleTextWidget &setTypeInRate(int newRateInTics);

    FinaleTextWidget &setPageColor(uint id);
    FinaleTextWidget &setPageFont(uint id);

    FinaleTextWidget &setColorAndAlpha(de::Vector4f const &newColorAndAlpha, int steps = 0);
    FinaleTextWidget &setColor(de::Vector3f const &newColor, int steps = 0);
    FinaleTextWidget &setAlpha(float alpha, int steps = 0);

protected:
#ifdef __CLIENT__
    void draw(de::Vector3f const &offset);
#endif
    void runTicks();

public:
    DENG2_PRIVATE(d)
};

// ---------------------------------------------------------------------------------------

/**
 * Finale page widget (layer).
 *
 * @ingroup infine
 */
class FinalePageWidget
{
public:
    /// An invalid color index was specified. @ingroup errors
    DENG2_ERROR(InvalidColorError);

    /// An invalid font index was specified. @ingroup errors
    DENG2_ERROR(InvalidFontError);

    /// @note Unlike de::Visual the children are not owned by the page.
    typedef QList<FinaleWidget *> Widgets;

public:
    FinalePageWidget();
    virtual ~FinalePageWidget();

#ifdef __CLIENT__
    virtual void draw();
#endif
    virtual void runTicks(timespan_t timeDelta);

    void makeVisible(bool yes = true);
    void pause(bool yes = true);

    /**
     * Returns @c true if @a widget is present on the page.
     */
    bool hasWidget(FinaleWidget *widget);

    /**
     * Adds a widget to the page if not already present. Page takes ownership.
     *
     * @param widgetToAdd  Widget to be added.
     *
     * @return  Same as @a widgetToAdd, for convenience.
     */
    FinaleWidget *addWidget(FinaleWidget *widgetToAdd);

    /**
     * Removes a widget from the page if present. Page gives up ownership.
     *
     * @param widgetToRemove  Widget to be removed.
     *
     * @return  Same as @a widgetToRemove, for convenience.
     */
    FinaleWidget *removeWidget(FinaleWidget *widgetToRemove);

    FinalePageWidget &setOffset(de::Vector3f const &newOffset, int steps = 0);
    FinalePageWidget &setOffsetX(float newOffsetX, int steps = 0);
    FinalePageWidget &setOffsetY(float newOffsetY, int steps = 0);
    FinalePageWidget &setOffsetZ(float newOffsetZ, int steps = 0);

    /// Current background Material.
    Material *backgroundMaterial() const;

    /// Sets the background Material.
    FinalePageWidget &setBackgroundMaterial(Material *newMaterial);

    /// Sets the background top color.
    FinalePageWidget &setBackgroundTopColor(de::Vector3f const &newColor, int steps = 0);

    /// Sets the background top color and alpha.
    FinalePageWidget &setBackgroundTopColorAndAlpha(de::Vector4f const &newColorAndAlpha, int steps = 0);

    /// Sets the background bottom color.
    FinalePageWidget &setBackgroundBottomColor(de::Vector3f const &newColor, int steps = 0);

    /// Sets the background bottom color and alpha.
    FinalePageWidget &setBackgroundBottomColorAndAlpha(de::Vector4f const &newColorAndAlpha, int steps = 0);

    /// Sets the filter color and alpha.
    FinalePageWidget &setFilterColorAndAlpha(de::Vector4f const &newColorAndAlpha, int steps = 0);

    /// @return  Animator which represents the identified predefined color.
    animatorvector3_t const *predefinedColor(uint idx);

    /// Sets a predefined color.
    FinalePageWidget &setPredefinedColor(uint idx, de::Vector3f const &newColor, int steps = 0);

    /// @return  Unique identifier of the predefined font.
    fontid_t predefinedFont(uint idx);

    /// Sets a predefined font.
    FinalePageWidget &setPredefinedFont(uint idx, fontid_t font);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_UI_INFINE_WIDGETS_H
