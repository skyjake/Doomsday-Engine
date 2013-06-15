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
using namespace ui;

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
    float imageScale;

    ConstantRule *width;
    ConstantRule *height;

    // Style.
    int margin;
    int gap;
    ColorBank::Color highlightColor;
    ColorBank::Color dimmedColor;
    ColorBank::Color accentColor;
    ColorBank::Color dimAccentColor;

    String styledText;
    FontLineWrapping wraps;
    int wrapWidth;
    GLTextComposer composer;
    Image image;
    bool needImageUpdate;

    Id imageTex;
    Drawable drawable;
    GLUniform uMvpMatrix;
    GLUniform uColor;

    Instance(Public *i)
        : Base(i),
          horizPolicy(Fixed), vertPolicy(Fixed),
          align(AlignCenter),
          textAlign(AlignCenter),
          lineAlign(AlignCenter),
          imageAlign(AlignCenter),
          imageFit(OriginalAspectRatio | FitToSize),
          imageScale(1),
          wrapWidth(0),
          needImageUpdate(false),
          imageTex(Id::None),
          uMvpMatrix("uMvpMatrix", GLUniform::Mat4),
          uColor    ("uColor",     GLUniform::Vec4)
    {
        width  = new ConstantRule(0);
        height = new ConstantRule(0);

        uColor = Vector4f(1, 1, 1, 1);        
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

        margin = self.margin().valuei();
        gap = margin / 2;

        // Colors.
        highlightColor = st.colors().color("label.highlight");
        dimmedColor    = st.colors().color("label.dimmed");
        accentColor    = st.colors().color("label.accent");
        dimAccentColor = st.colors().color("label.dimaccent");

        wraps.setFont(self.font());
        wrapWidth = 0;

        self.requestGeometry();
    }

    Color richStyleColor(int index) const
    {
        switch(index)
        {
        default:
        case Font::RichFormat::NormalColor:
            return self.textColor();

        case Font::RichFormat::HighlightColor:
            return highlightColor;

        case Font::RichFormat::DimmedColor:
            return dimmedColor;

        case Font::RichFormat::AccentColor:
            return accentColor;

        case Font::RichFormat::DimAccentColor:
            return dimAccentColor;
        }
    }

    void richStyleFormat(int contentStyle, float &sizeFactor, Font::RichFormat::Weight &fontWeight,
                         Font::RichFormat::Style &fontStyle, int &colorIndex) const
    {
        switch(contentStyle)
        {
        default:
        case Font::RichFormat::NormalStyle:
            sizeFactor = 1.f;
            fontWeight = Font::RichFormat::OriginalWeight;
            fontStyle  = Font::RichFormat::OriginalStyle;
            colorIndex = Font::RichFormat::OriginalColor;
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
        self.root().shaders().build(drawable.program(), "generic.textured.color_ucolor")
                << uMvpMatrix << uColor << self.root().uAtlas();

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
     * @param laoyut  Placement of the image and text.
     */
    void contentPlacement(ContentLayout &layout) const
    {
        Rectanglei const contentRect = self.rule().recti().shrunk(margin);
        Vector2f const imageSize = image.size() * imageScale;

        // Determine the sizes of the elements first.
        layout.image = Rectanglef::fromSize(imageSize);
        layout.text  = Rectanglei::fromSize(textSize());

        if(horizPolicy == Filled)
        {
            if(textAlign & (AlignLeft | AlignRight))
            {
                layout.image.setWidth(int(contentRect.width()) - int(layout.text.width()) - gap);
            }
            else
            {
                layout.image.setWidth(contentRect.width());
                layout.text.setWidth(contentRect.width());
            }
        }
        if(vertPolicy == Filled)
        {
            if(textAlign & (AlignTop | AlignBottom))
            {
                layout.image.setHeight(int(contentRect.height()) - int(layout.text.height()) - gap);
            }
            else
            {
                layout.image.setHeight(contentRect.height());
                layout.text.setHeight(contentRect.height());
            }
        }

        // By default the image and the text are centered over each other.
        layout.image.move((Vector2f(layout.text.size()) - layout.image.size()) / 2);

        // Determine the position of the image in relation to the text
        // (keeping the image at its current position).
        if(textAlign & AlignLeft)
        {
            layout.text.moveLeft(layout.image.left() - layout.text.width() - gap);
        }
        if(textAlign & AlignRight)
        {
            layout.text.moveLeft(layout.image.right() + gap);
        }
        if(textAlign & AlignTop)
        {
            layout.text.moveTop(layout.image.top() - layout.text.height() - gap);
        }
        if(textAlign & AlignBottom)
        {
            layout.text.moveTop(layout.image.bottom() + gap);
        }

        // Align the image in relation to the text on the other axis.
        if(textAlign & (AlignLeft | AlignRight))
        {
            if(imageAlign & AlignTop)
            {
                layout.image.moveTop(layout.text.top());
            }
            if(imageAlign & AlignBottom)
            {
                layout.image.moveTop(layout.text.bottom() - layout.image.height());
            }
        }
        if(textAlign & (AlignTop | AlignBottom))
        {
            if(imageAlign & AlignLeft)
            {
                layout.image.moveLeft(layout.text.left());
            }
            if(imageAlign & AlignRight)
            {
                layout.image.moveLeft(layout.text.right() - layout.image.width());
            }
        }

        // Align the combination within the content.
        Rectanglef combined = layout.image | layout.text;

        Vector2f delta = applyAlignment(align, combined.size(), contentRect);
        delta -= combined.topLeft;

        layout.image.move(delta);
        layout.text.move(delta.toVector2i());

        if(hasImage())
        {
            // Figure out how much room is left for the image.
            Rectanglef const rect = layout.image;

            // Fit the image.
            if(!imageFit.testFlag(FitToWidth))
            {
                layout.image.setWidth(image.width());
            }
            if(!imageFit.testFlag(FitToHeight))
            {
                layout.image.setHeight(image.height());
            }

            // Should the original aspect ratio be preserved?
            if(imageFit & OriginalAspectRatio)
            {
                if(imageFit & FitToWidth)
                {
                    layout.image.setHeight(image.height() * layout.image.width() / image.width());
                }
                if(imageFit & FitToHeight)
                {
                    layout.image.setWidth(image.width() * layout.image.height() / image.height());

                    if(imageFit.testFlag(FitToWidth))
                    {
                        float scale = 1;
                        if(layout.image.width() > rect.width())
                        {
                            scale = float(rect.width()) / float(layout.image.width());
                        }
                        else if(layout.image.height() > rect.height())
                        {
                            scale = float(rect.height()) / float(layout.image.height());
                        }
                        layout.image.setSize(Vector2f(layout.image.size()) * scale);
                    }
                }
            }

            applyAlignment(imageAlign, layout.image, rect);
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
            self.requestGeometry();

            Font::RichFormat format(*this);
            String plain = format.initFromStyledText(styledText);
            wraps.wrapTextToWidth(plain, format, wrapWidth);

            composer.setText(plain, format);
            composer.update();

            // Figure out the actual size of the content.
            ContentLayout layout;
            contentPlacement(layout);
            Rectanglef combined = layout.image | layout.text;
            width->set(combined.width() + 2 * margin);
            height->set(combined.height() + 2 * margin);
        }
    }

    void updateGeometry()
    {
        self.requestGeometry(false);

        VertexBuf::Builder verts;
        self.glMakeGeometry(verts);
        drawable.buffer<VertexBuf>().setVertices(gl::TriangleStrip, verts, gl::Static);
    }

    void draw()
    {
        Rectanglei pos;
        if(self.hasChangedPlace(pos) || self.geometryRequested())
        {
            updateGeometry();
        }
        self.updateModelViewProjection(uMvpMatrix);
        drawable.draw();
    }

    void atlasContentRepositioned(Atlas &)
    {
        self.requestGeometry();
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

String LabelWidget::text() const
{
    return d->styledText;
}

void LabelWidget::setAlignment(Alignment const &align)
{
    d->align = align;
}

void LabelWidget::setTextAlignment(Alignment const &textAlign)
{
    d->textAlign = textAlign;
}

void LabelWidget::setTextLineAlignment(Alignment const &textLineAlign)
{
    d->lineAlign = textLineAlign;
}

void LabelWidget::setImageAlignment(Alignment const &imageAlign)
{
    d->imageAlign = imageAlign;
}

void LabelWidget::setImageFit(ContentFit const &fit)
{
    d->imageFit = fit;
}

void LabelWidget::setImageScale(float scaleFactor)
{
    d->imageScale = scaleFactor;
}

void LabelWidget::update()
{
    GuiWidget::update();
    d->update();
}

void LabelWidget::drawContent()
{
    d->uColor = Vector4f(1, 1, 1, visibleOpacity());
    d->draw();
}

void LabelWidget::contentLayout(LabelWidget::ContentLayout &layout)
{
    d->contentPlacement(layout);
}

void LabelWidget::glInit()
{
    d->glInit();
}

void LabelWidget::glDeinit()
{
    d->glDeinit();
}

void LabelWidget::glMakeGeometry(DefaultVertexBuf::Builder &verts)
{
    // Background/frame.
    GuiWidget::glMakeGeometry(verts);

    ContentLayout layout;
    contentLayout(layout);

    if(d->hasImage())
    {
        verts.makeQuad(layout.image, Vector4f(1, 1, 1, 1), d->atlas().imageRectf(d->imageTex));
    }
    if(d->hasText())
    {
        // Shadow + text.
        /*composer.makeVertices(verts, textPos.topLeft + Vector2i(0, 2),
                              lineAlign, Vector4f(0, 0, 0, 1));*/
        d->composer.makeVertices(verts, layout.text, AlignCenter, d->lineAlign);
    }
}

void LabelWidget::updateStyle()
{
    d->updateStyle();
}

void LabelWidget::updateModelViewProjection(GLUniform &uMvp)
{
    uMvp = root().projMatrix2D();
}

void LabelWidget::viewResized()
{
    updateModelViewProjection(d->uMvpMatrix);
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
