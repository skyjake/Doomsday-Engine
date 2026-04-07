/** @file widgets/labelwidget.h
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

#ifndef LIBAPPFW_LABELWIDGET_H
#define LIBAPPFW_LABELWIDGET_H

#include "ui/defs.h"
#include "ui/data.h"
#include "de/guiwidget.h"
#include "de/proceduralimage.h"

namespace de {

class GLUniform;
class GridLayout;
class Image;

/**
 * Widget showing a label text and/or image.
 *
 * LabelWidget offers several parameters for controlling the layout of the text
 * and image components. The widget is also able to independently determine its
 * size to exactly fit its contents (according to the LabelWidget::SizePolicy).
 *
 * The alignment parameters are applied as follows:
 *
 * - LabelWidget::setAlignment() defines where the content of the widget is
 *   aligned as a group, once the relative positions of the image and the text
 *   have been determined.
 *
 * - LabelWidget::setTextAlignment() defines where the text will be positioned
 *   in relation to the image. For instance, if the text alignment is AlignRight,
 *   the text will be placed on the right side of the image. If there is no
 *   image, this has no effect.
 *
 * - LabelWidget::setImageAlignment() defines how the image is aligned in
 *   relation to text when both are visible. For instance, if text is aligned to
 *   AlignRight (appearing on the right side of the image), then an image
 *   alignment of AlignTop would align the top of the image with the top of the
 *   text. AlignBottom would align the bottom of the image with the bottom of the
 *   text. This value must be on the perpendicular axis when compared to text
 *   alignment (otherwise it has no effect).
 *
 * - LabelWidget::setTextLineAlignment() defines the alignment of each individual
 *   wrapped line of text within the text block.
 *
 * Additionally, LabelWidget::setImageFit() defines how the image will be
 * scaled inside the area reserved for the image.
 *
 * LabelWidget is an Asset because the preparation of text for drawing occurs
 * asynchronously, and meanwhile the content size of the text is (0,0).
 * Observing the asset state allows others to determine when the label is ready
 * to be drawn/laid out with the final dimensions.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC LabelWidget : public GuiWidget, public IAssetGroup
{
public:
    LabelWidget(const String &name = String());

    AssetGroup &assets() override;

    void setText(const String &text);
    void setImage(const Image &image);

    /**
     * Sets the image drawn in the label. Procedural images can generate any
     * geometry on the fly, so the image can be fully animated.
     *
     * @param procImage  Procedural image. LabelWidget takes ownership.
     */
    void setImage(ProceduralImage *procImage);

    /**
     * Sets the image drawn in the label using a procedural image from the UI style.
     *
     * @param id  Image ID.
     * @param heightFromFont  Optionally overrides the image size using the height of
     *             this font. Default is to use the image's own size.
     */
    void setStyleImage(const DotPath &id, const String &heightFromFont = "");

    ProceduralImage *image() const;

    /**
     * Sets an overlay image that gets drawn over the label contents.
     *
     * @param overlayProcImage  Procedural image. LabelWidget takes ownership.
     * @param alignment         Alignment for the overlaid image.
     */
    void setOverlayImage(ProceduralImage *overlayProcImage,
                         const ui::Alignment &alignment = ui::AlignCenter);

    String text() const;
    String plainText() const;

    /**
     * Returns the actual size of the text in pixels.
     */
    Vec2ui textSize() const;

    const Rule &contentWidth() const;
    const Rule &contentHeight() const;

    enum FillMode {
        FillWithImage,
        FillWithText
    };

    void setFillMode(FillMode fillMode);

    /**
     * Sets the gap between the text and image. Defaults to "label.gap".
     *
     * @param styleRuleId  Id of a rule in the style.
     */
    void setTextGap(const DotPath &styleRuleId);

    const DotPath &textGap() const;

    enum TextShadow {
        NoShadow,
        RectangleShadow
    };

    void setTextShadow(TextShadow shadow, const DotPath &shadowColor = "label.shadow");

    enum AlignmentMode {
        AlignByCombination,
        AlignOnlyByImage,
        AlignOnlyByText
    };

    /**
     * Sets the alignment of the entire contents of the widget inside its
     * rectangle.
     *
     * @param align      Alignment for all content.
     * @param alignMode  Mode of alignment (by combo/text/image).
     */
    void setAlignment(const ui::Alignment &align,
                      AlignmentMode alignMode = AlignByCombination);

    void setTextAlignment(const ui::Alignment &textAlign);

    ui::Alignment textAlignment() const;

    void setTextLineAlignment(const ui::Alignment &textLineAlign);

    void setTextModulationColorf(const Vec4f &colorf);

    Vec4f textModulationColorf() const;

    /**
     * Sets the maximum width used for text. By default, the maximum width is determined
     * automatically based on the layout of the label content.
     *
     * @param pixels  Maximum width of text, or 0 to determine automatically.
     */
    void setMaximumTextWidth(int pixels);

    void setMaximumTextWidth(const Rule &pixels);

    void setMinimumWidth(const Rule &minWidth);

    void setMinimumHeight(const Rule &minHeight);

    /**
     * Sets an alternative style for text. By default, the rich text styling comes
     * from Style.
     *
     * @param richStyle  Rich text styling.
     */
    void setTextStyle(const Font::RichFormat::IStyle *richStyle);

    /**
     * Sets the font, text color, and margins for use as a menu/dialog separator label.
     */
    void useSeparatorStyle();

    /**
     * Sets the alignment of the image when there is both an image
     * and a text in the label.
     *
     * @param imageAlign  Alignment for the image.
     */
    void setImageAlignment(const ui::Alignment &imageAlign);

    void setImageFit(const ui::ContentFit &fit);

    /**
     * The image's actual size will be overridden by this size.
     * @param size  Image size.
     */
    void setOverrideImageSize(const Rule &width, const Rule &height);

    void setOverrideImageSize(const ISizeRule &size)
    {
        setOverrideImageSize(size.width(), size.height());
    }

    void setOverrideImageSize(const Rule &widthAndHeight)
    {
        setOverrideImageSize(widthAndHeight, widthAndHeight);
    }

    RulePair overrideImageSize() const;

    void setImageScale(float scaleFactor);

    void setImageColor(const Vec4f &imageColor);

    bool hasImage() const;

    /**
     * Sets the policy for the widget to adjust its own width and/or height. By default,
     * labels do not adjust their own size.
     * - ui::Fixed means that the content uses its own size and the widget needs to be
     *   sized by the user.
     * - ui::Filled means content is expanded to fill the entire contents of the widget
     *   area, but the widget still needs to be sized by the user.
     * - ui::Expand means the widget resizes itself to the size of the content.
     *
     * @param horizontal  Horizontal sizing policy.
     * @param vertical    Vertical sizing policy.
     */
    void setSizePolicy(ui::SizePolicy horizontal, ui::SizePolicy vertical) {
        setWidthPolicy(horizontal);
        setHeightPolicy(vertical);
    }

    void setWidthPolicy(ui::SizePolicy policy);
    void setHeightPolicy(ui::SizePolicy policy);

    enum AppearanceAnimation {
        AppearInstantly,
        AppearGrowHorizontally,
        AppearGrowVertically
    };

    /**
     * Sets the way the label's content affects its size.
     *
     * @param method  Method of appearance:
     * - AppearInstantly: The size is unaffected by the content's state.
     *                    This is the default.
     * - AppearGrowHorizontally: The widget's width is initially zero, but when the
     *                    content is ready for drawing, the width will animate
     *                    to the appropriate width in the specified time span.
     * - AppearGrowVertically: The widget's height is initially zero, but when the
     *                    content is ready for drawing, the height will animate
     *                    to the appropriate height in the specified time span.
     * @param span  Animation time span for the appearance.
     */
    void setAppearanceAnimation(AppearanceAnimation method, TimeSpan span = 0.0);

    // Events.
    void update() override;
    void drawContent() override;

    struct ContentLayout {
        Rectanglef image;
        Rectanglei text;
    };

    void contentLayout(ContentLayout &layout);

public:
    static LabelWidget *newWithText(const String &label, GuiWidget *parent = nullptr);
    static LabelWidget *appendSeparatorWithText(const String &text, GuiWidget *parent = nullptr,
                                                GridLayout *appendToGrid = nullptr);

protected:
    void glInit() override;
    void glDeinit() override;
    void glMakeGeometry(GuiVertexBuilder &verts) override;
    void updateStyle() override;

    /**
     * Called before drawing to update the model-view-projection matrix.
     * Derived classes may override this to set a custom matrix for the label.
     * @return @c true, if a customized matrix was set.
     */
    virtual bool updateModelViewProjection(Mat4f &mvp);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_LABELWIDGET_H
