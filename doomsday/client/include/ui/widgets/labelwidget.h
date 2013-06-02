/** @file labelwidget.h
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

#ifndef DENG_CLIENT_LABELWIDGET_H
#define DENG_CLIENT_LABELWIDGET_H

#include <de/Image>
#include <de/GLBuffer>
#include <de/GLUniform>

#include "guiwidget.h"
#include "alignment.h"

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
 */
class LabelWidget : public GuiWidget
{
public:
    LabelWidget(de::String const &name = "");

    void setText(de::String const &text);
    void setImage(de::Image const &image);

    /**
     * Sets the alignment of the entire contents of the widget inside its
     * rectangle.
     *
     * @param align  Alignment for all content.
     */
    void setAlignment(Alignment const &align);

    void setTextAlignment(Alignment const &textAlign);

    void setTextLineAlignment(Alignment const &textLineAlign);

    /**
     * Sets the alignment of the image when there is both an image
     * and a text in the label.
     *
     * @param imageAlign  Alignment for the image.
     */
    void setImageAlignment(Alignment const &imageAlign);

    void setImageFit(ContentFit const &fit);

    void setImageScale(float scaleFactor);

    enum SizePolicy {
        Fixed,  ///< Size is fixed, content positioned inside.
        Filled, ///< Size is fixed, content expands to fill entire area.
        Expand  ///< Size depends on content, expands/contracts to fit.
    };

    /**
     * Allows or disallows the label to expand vertically to fit the provided
     * content. By default, labels do not adjust their own size.
     *
     * @param expand  @c true to allow the widget to modify its own height.
     */
    void setSizePolicy(SizePolicy horizontal, SizePolicy vertical) {
        setWidthPolicy(horizontal);
        setHeightPolicy(vertical);
    }

    void setWidthPolicy(SizePolicy policy);
    void setHeightPolicy(SizePolicy policy);

    void setOpacity(float opacity, de::TimeDelta span = 0);

    // Events.
    void viewResized();
    void update();
    void draw();

    struct ContentLayout {
        de::Rectanglef image;
        de::Rectanglei text;
    };

    void contentLayout(ContentLayout &layout);

protected:
    void glInit();
    void glDeinit();

    void glMakeGeometry(DefaultVertexBuf::Builder &verts);

    /**
     * Called before drawing to update the model-view-projection matrix.
     * Derived classes may override this to set a custom matrix for the label.
     */
    virtual void updateModelViewProjection(de::GLUniform &uMvp);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_LABELWIDGET_H
