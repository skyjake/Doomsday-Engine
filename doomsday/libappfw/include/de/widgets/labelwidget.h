/** @file labelwidget.h
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/Image>
#include <de/GLBuffer>
#include <de/GLUniform>

#include "../ui/defs.h"
#include "../GuiWidget"
#include "../ProceduralImage"
#include "../ui/Data"

namespace de {

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
 * @ingroup gui
 */
class LIBAPPFW_PUBLIC LabelWidget : public GuiWidget
{
public:
    LabelWidget(String const &name = "");

    void setText(String const &text);
    void setImage(Image const &image);

    /**
     * Sets the image drawn in the label. Procedural images can generate any
     * geometry on the fly, so the image can be fully animated.
     *
     * @param procImage  Procedural image. LabelWidget takes ownership.
     */
    void setImage(ProceduralImage *procImage);

    /**
     * Sets an overlay image that gets drawn over the label contents.
     *
     * @param overlayProcImage  Procedural image. LabelWidget takes ownership.
     * @param alignment         Alignment for the overlaid image.
     */
    void setOverlayImage(ProceduralImage *overlayProcImage,
                         ui::Alignment const &alignment = ui::AlignCenter);

    String text() const;

    /**
     * Returns the actual size of the text in pixels.
     */
    Vector2ui textSize() const;

    /**
     * Sets the gap between the text and image. Defaults to "label.gap".
     *
     * @param styleRuleId  Id of a rule in the style.
     */
    void setTextGap(DotPath const &styleRuleId);

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
    void setAlignment(ui::Alignment const &align,
                      AlignmentMode alignMode = AlignByCombination);

    void setTextAlignment(ui::Alignment const &textAlign);

    void setTextLineAlignment(ui::Alignment const &textLineAlign);

    void setTextModulationColorf(Vector4f const &colorf);

    /**
     * Sets the maximum width used for text. By default, the maximum width is determined
     * automatically based on the layout of the label content.
     *
     * @param pixels  Maximum width of text, or 0 to determine automatically.
     */
    void setMaximumTextWidth(int pixels);

    /**
     * Sets an alternative style for text. By default, the rich text styling comes
     * from Style.
     *
     * @param richStyle  Rich text styling.
     */
    void setTextStyle(Font::RichFormat::IStyle const *richStyle);

    /**
     * Sets the alignment of the image when there is both an image
     * and a text in the label.
     *
     * @param imageAlign  Alignment for the image.
     */
    void setImageAlignment(ui::Alignment const &imageAlign);

    void setImageFit(ui::ContentFit const &fit);

    /**
     * The image's actual size will be overridden by this size.
     * @param size  Image size.
     */
    void setOverrideImageSize(Vector2f const &size);

    void setOverrideImageSize(float widthAndHeight);

    void setImageScale(float scaleFactor);

    void setImageColor(Vector4f const &imageColor);

    bool hasImage() const;

    /**
     * Allows or disallows the label to expand vertically to fit the provided
     * content. By default, labels do not adjust their own size.
     *
     * @param expand  @c true to allow the widget to modify its own height.
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
    void setAppearanceAnimation(AppearanceAnimation method, TimeDelta const &span = 0.0);

    // Events.
    void viewResized();
    void update();
    void drawContent();

    struct ContentLayout {
        Rectanglef image;
        Rectanglei text;
    };

    void contentLayout(ContentLayout &layout);

public:
    static LabelWidget *newWithText(String const &label, GuiWidget *parent = 0);

protected:
    void glInit();
    void glDeinit();
    void glMakeGeometry(DefaultVertexBuf::Builder &verts);
    void updateStyle();

    /**
     * Called before drawing to update the model-view-projection matrix.
     * Derived classes may override this to set a custom matrix for the label.
     */
    virtual void updateModelViewProjection(GLUniform &uMvp);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_LABELWIDGET_H
