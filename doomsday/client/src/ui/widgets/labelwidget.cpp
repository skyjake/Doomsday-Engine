/** @file labelwidget.cpp
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

#include "ui/widgets/labelwidget.h"
#include "ui/widgets/gltextcomposer.h"
#include "ui/widgets/fontlinewrapping.h"
#include "ui/widgets/guirootwidget.h"

#include <de/Drawable>
#include <de/AtlasTexture>
#include <de/ConstantRule>

using namespace de;

DENG2_PIMPL(LabelWidget),
DENG2_OBSERVES(Atlas, Reposition),
public Font::RichFormat::IStyle
{
    typedef GLBufferT<Vertex2TexRgba> VertexBuf;

    SizePolicy horizPolicy;
    SizePolicy vertPolicy;
    Alignment align;
    Alignment textAlign;
    Alignment lineAlign;
    Alignment imageAlign;
    ContentFit imageFit;

    ConstantRule *width;
    ConstantRule *height;

    // Style.
    Font const *font;
    int margin;
    int gap;

    String styledText;
    FontLineWrapping wraps;
    int wrapWidth;
    GLTextComposer composer;
    Image image;
    bool needImageUpdate;

    Id imageTex;
    bool needGeometry;
    Drawable drawable;
    GLUniform uMvpMatrix;

    Instance(Public *i)
        : Base(i),
          horizPolicy(Fixed), vertPolicy(Fixed),
          align(AlignCenter),
          textAlign(AlignRight),
          lineAlign(AlignLeft),
          imageAlign(AlignTop),
          imageFit(OriginalAspectRatio | FitToSize),
          font(0),
          wrapWidth(0),
          needImageUpdate(false),
          imageTex(Id::None),
          needGeometry(false),
          uMvpMatrix("uMvpMatrix", GLUniform::Mat4)
    {
        width  = new ConstantRule(0);
        height = new ConstantRule(0);

        updateStyle();
    }

    ~Instance()
    {
        releaseRef(width);
        releaseRef(height);
    }

    void updateStyle()
    {
        Style const &st = self.style();

        font   = &st.fonts().font("default");
        margin = st.rules().rule("gap").valuei();
        gap    = margin / 2;

        wraps.setFont(*font);
        wrapWidth = 0;
        needGeometry = true;
    }

    Color richStyleColor(int index) const
    {
        return Vector4ub(255, 255, 255, 255);
    }

    void richStyleFormat(int contentStyle, float &sizeFactor, Font::RichFormat::Weight &fontWeight,
                         Font::RichFormat::Style &fontStyle, int &colorIndex) const
    {
        switch(contentStyle)
        {
        default:
            sizeFactor = 1.f;
            fontWeight = Font::RichFormat::Normal;
            fontStyle  = Font::RichFormat::Regular;
            colorIndex = Font::RichFormat::NormalColor;
            break;
        }
    }

    AtlasTexture &atlas()
    {
        DENG2_ASSERT(self.hasRoot());
        return self.root().atlas();
    }

    void glInit()
    {
        drawable.addBuffer(new VertexBuf);
        self.root().shaders().build(drawable.program(), "generic.textured.color")
                << uMvpMatrix << self.root().uAtlas();

        composer.setAtlas(atlas());
        composer.setWrapping(wraps);
    }

    void glDeinit()
    {
        composer.release();
        atlas().release(imageTex);
    }

    bool hasImage() const
    {
        return !image.isNull();
    }

    bool hasText() const
    {
        return !styledText.isEmpty();
    }

    /**
     * Determines where the label's image and text should be drawn.
     *
     * @param imageRect  Placement of the image.
     * @param textRect   Placement of the text.
     */
    void contentPlacement(Rectanglef &imageRect, Rectanglei &textRect) const
    {
        Rectanglei const contentRect = self.rule().recti().shrunk(margin);

        // Determine the sizes of the elements first.
        imageRect = Rectanglef::fromSize(image.size());
        textRect  = Rectanglei::fromSize(textSize());

        if(horizPolicy == Filled)
        {
            if(textAlign & (AlignLeft | AlignRight))
            {
                imageRect.setWidth(int(contentRect.width()) - int(textRect.width()) - gap);
            }
            else
            {
                imageRect.setWidth(contentRect.width());
                textRect.setWidth(contentRect.width());
            }
        }
        if(vertPolicy == Filled)
        {
            if(textAlign & (AlignTop | AlignBottom))
            {
                imageRect.setHeight(int(contentRect.height()) - int(textRect.height()) - gap);
            }
            else
            {
                imageRect.setHeight(contentRect.height());
                textRect.setHeight(contentRect.height());
            }
        }

        // By default the image and the text are centered over each other.
        imageRect.move((Vector2f(textRect.size()) - imageRect.size()) / 2);

        // Determine the position of the image in relation to the text
        // (keeping the image at its current position).
        if(textAlign & AlignLeft)
        {
            textRect.moveLeft(imageRect.left() - textRect.width() - gap);
        }
        if(textAlign & AlignRight)
        {
            textRect.moveLeft(imageRect.right() + gap);
        }
        if(textAlign & AlignTop)
        {
            textRect.moveTop(imageRect.top() - textRect.height() - gap);
        }
        if(textAlign & AlignBottom)
        {
            textRect.moveTop(imageRect.bottom() + gap);
        }

        // Align the image in relation to the text on the other axis.
        if(textAlign & (AlignLeft | AlignRight))
        {
            if(imageAlign & AlignTop)
            {
                imageRect.moveTop(textRect.top());
            }
            if(imageAlign & AlignBottom)
            {
                imageRect.moveTop(textRect.bottom() - imageRect.height());
            }
        }
        if(textAlign & (AlignTop | AlignBottom))
        {
            if(imageAlign & AlignLeft)
            {
                imageRect.moveLeft(textRect.left());
            }
            if(imageAlign & AlignRight)
            {
                imageRect.moveLeft(textRect.right() - imageRect.width());
            }
        }

        // Align the combination within the content.
        Rectanglef combined = imageRect | textRect;

        Vector2f delta = applyAlignment(align, combined.size(), contentRect);
        delta -= combined.topLeft;

        imageRect.move(delta);
        textRect.move(delta.toVector2i());

        if(hasImage())
        {
            // Figure out how much room is left for the image.
            Rectanglef const rect = imageRect;

            // Fit the image.
            if(!imageFit.testFlag(FitToWidth))
            {
                imageRect.setWidth(image.width());
            }
            if(!imageFit.testFlag(FitToHeight))
            {
                imageRect.setHeight(image.height());
            }

            // Should the original aspect ratio be preserved?
            if(imageFit & OriginalAspectRatio)
            {
                if(imageFit & FitToWidth)
                {
                    imageRect.setHeight(image.height() * imageRect.width() / image.width());
                }
                if(imageFit & FitToHeight)
                {
                    imageRect.setWidth(image.width() * imageRect.height() / image.height());

                    if(imageFit.testFlag(FitToWidth))
                    {
                        float scale = 1;
                        if(imageRect.width() > rect.width())
                        {
                            scale = float(rect.width()) / float(imageRect.width());
                        }
                        else if(imageRect.height() > rect.height())
                        {
                            scale = float(rect.height()) / float(imageRect.height());
                        }
                        imageRect.setSize(Vector2f(imageRect.size()) * scale);
                    }
                }
            }

            applyAlignment(imageAlign, imageRect, rect);
        }
    }

    Vector2ui textSize() const
    {
        return Vector2ui(wraps.width(), wraps.totalHeightInPixels());
    }

    int availableTextWidth() const
    {
        int w = 0;
        if(horizPolicy == Expand)
        {
            // Expansion can occur to full view width.
            w = self.root().viewSize().x - 2 * margin;
        }
        else
        {
            w = self.rule().width().valuei() - 2 * margin;
        }
        if(textAlign & (AlignLeft | AlignRight))
        {
            // Image will be placed beside the text.
            w -= gap + image.width();
        }
        return w;
    }

    void update()
    {
        // Update the image on the atlas.
        if(needImageUpdate)
        {
            needImageUpdate = false;
            atlas().release(imageTex);
            imageTex = atlas().alloc(image);
        }

        // Update the wrapped text.
        if(wrapWidth != availableTextWidth())
        {
            wrapWidth = availableTextWidth();
            needGeometry = true;

            Font::RichFormat format;
            String plain = format.initFromStyledText(styledText);
            wraps.wrapTextToWidth(plain, format, wrapWidth);

            composer.setText(plain, format);
            composer.update();

            // Figure out the non-filled size of the content.
            Rectanglef imgPos;
            Rectanglei textPos;
            contentPlacement(imgPos, textPos);
            Rectanglef combined = imgPos | textPos;
            width->set(combined.width() + 2 * margin);
            height->set(combined.height() + 2 * margin);
        }
    }

    void updateGeometry()
    {
        needGeometry = false;

        Rectanglef imgPos;
        Rectanglei textPos;
        contentPlacement(imgPos, textPos);

        VertexBuf::Builder verts;

        // Background for testing.
        //verts.makeQuad(self.rule().rect(), Vector4f(.5f, 0, .5f, .5f),
        //               atlas().imageRectf(self.root().solidWhitePixel()).middle());

        if(hasImage())
        {
            verts.makeQuad(imgPos, Vector4f(1, 1, 1, 1), atlas().imageRectf(imageTex));
        }
        if(hasText())
        {
            // Shadow + text.
            /*composer.makeVertices(verts, textPos.topLeft + Vector2i(0, 2),
                                  lineAlign, Vector4f(0, 0, 0, 1));*/
            composer.makeVertices(verts, textPos.topLeft, lineAlign);
        }
        drawable.buffer<VertexBuf>().setVertices(gl::TriangleStrip, verts, gl::Static);
    }

    void draw()
    {
        Rectanglei pos;
        if(self.checkPlace(pos) || needGeometry)
        {
            updateGeometry();
        }
        drawable.draw();
    }

    void atlasContentRepositioned(Atlas &)
    {
        needGeometry = true;
    }
};

LabelWidget::LabelWidget(String const &name) : GuiWidget(name), d(new Instance(this))
{}

void LabelWidget::setText(String const &text)
{
    d->styledText = text;
    d->wrapWidth = 0; // force rewrap
}

void LabelWidget::setImage(Image const &image)
{
    d->image = image;
    d->needImageUpdate = true;
}

void LabelWidget::update()
{
    GuiWidget::update();
    d->update();
}

void LabelWidget::draw()
{
    d->draw();
}

void LabelWidget::glInit()
{
    d->glInit();
}

void LabelWidget::glDeinit()
{
    d->glDeinit();
}

void LabelWidget::viewResized()
{
    d->uMvpMatrix = root().projMatrix2D();
}

void LabelWidget::setWidthPolicy(SizePolicy policy)
{
    d->horizPolicy = policy;
    if(policy == Expand)
    {
        rule().setInput(Rule::Width, *d->width);
    }
    else
    {
        rule().clearInput(Rule::Width);
    }
}

void LabelWidget::setHeightPolicy(SizePolicy policy)
{
    d->vertPolicy = policy;
    if(policy == Expand)
    {
        rule().setInput(Rule::Height, *d->height);
    }
    else
    {
        rule().clearInput(Rule::Height);
    }
}
