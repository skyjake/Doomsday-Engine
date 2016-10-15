/** @file widgets/labelwidget.cpp
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

#include "de/LabelWidget"
#include "de/TextDrawable"
#include "de/AtlasProceduralImage"
#include "de/StyleProceduralImage"

#include <de/Drawable>
#include <de/AtlasTexture>
#include <de/ConstantRule>

namespace de {

using namespace ui;

DENG_GUI_PIMPL(LabelWidget),
public Font::RichFormat::IStyle
{
    typedef DefaultVertexBuf VertexBuf;

    AssetGroup assets;
    SizePolicy horizPolicy;
    SizePolicy vertPolicy;
    AlignmentMode alignMode;
    Alignment align;
    Alignment textAlign;
    Alignment lineAlign;
    Alignment imageAlign;
    Alignment overlayAlign;
    FillMode fillMode = FillWithImage;
    TextShadow textShadow = NoShadow;
    DotPath textShadowColorId { "label.shadow" };
    ContentFit imageFit;
    Vector2f overrideImageSize;
    float imageScale;
    Vector4f imageColor;
    Vector4f textGLColor;
    Vector4f shadowColor;
    Rule const *maxTextWidth;

    ConstantRule *width;
    ConstantRule *height;
    IndirectRule *minHeight;
    Rule const *outHeight;
    AnimationRule *appearSize;
    LabelWidget::AppearanceAnimation appearType;
    TimeDelta appearSpan;

    // Style.
    DotPath gapId;
    DotPath shaderId;
    int gap;
    ColorBank::Color highlightColor;
    ColorBank::Color dimmedColor;
    ColorBank::Color accentColor;
    ColorBank::Color dimAccentColor;
    ColorBank::Color altAccentColor;
    Font::RichFormat::IStyle const *richStyle;

    TextDrawable glText;
    mutable Vector2ui latestTextSize;
    bool wasVisible;

    QScopedPointer<ProceduralImage> image;
    QScopedPointer<ProceduralImage> overlayImage;
    Drawable drawable;
    GLUniform uMvpMatrix;
    GLUniform uColor;

    Impl(Public *i)
        : Base(i)
        , horizPolicy (Fixed)
        , vertPolicy  (Fixed)
        , alignMode   (AlignByCombination)
        , align       (AlignCenter)
        , textAlign   (AlignCenter)
        , lineAlign   (AlignCenter)
        , imageAlign  (AlignCenter)
        , imageFit    (OriginalAspectRatio | FitToSize)
        , imageScale  (1)
        , imageColor  (1, 1, 1, 1)
        , textGLColor (1, 1, 1, 1)
        , maxTextWidth(0)
        , appearSize  (new AnimationRule(0))
        , appearType  (AppearInstantly)
        , appearSpan  (0.0)
        , gapId       ("label.gap")
        , shaderId    ("generic.textured.color_ucolor")
        , richStyle   (0)
        , wasVisible  (true)
        , uMvpMatrix  ("uMvpMatrix", GLUniform::Mat4)
        , uColor      ("uColor",     GLUniform::Vec4)
    {
        width     = new ConstantRule(0);
        height    = new ConstantRule(0);
        minHeight = new IndirectRule;
        outHeight = new OperatorRule(OperatorRule::Maximum, *height, *minHeight);

        uColor = Vector4f(1, 1, 1, 1);
        updateStyle();

        // The readiness of the LabelWidget depends on glText being ready.
        assets += glText;
    }

    ~Impl()
    {
        releaseRef(width);
        releaseRef(height);
        releaseRef(minHeight);
        releaseRef(outHeight);
        releaseRef(appearSize);
        releaseRef(maxTextWidth);
    }

    void updateStyle()
    {
        Style const &st = self.style();

        gap = rule(gapId).valuei();

        // Colors.
        highlightColor = st.colors().color("label.highlight");
        dimmedColor    = st.colors().color("label.dimmed");
        accentColor    = st.colors().color("label.accent");
        dimAccentColor = st.colors().color("label.dimaccent");
        altAccentColor = st.colors().color("label.altaccent");
        shadowColor    = st.colors().colorf(textShadowColorId);

        glText.setFont(self.font());
        glText.forceUpdate();

        self.requestGeometry();
    }

    Vector4i margin() const
    {
        return self.margins().toVector();
    }

    Color richStyleColor(int index) const
    {
        switch (index)
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

        case Font::RichFormat::AltAccentColor:
            return altAccentColor;
        }
    }

    void richStyleFormat(int contentStyle, float &sizeFactor, Font::RichFormat::Weight &fontWeight,
                         Font::RichFormat::Style &fontStyle, int &colorIndex) const
    {
        if (richStyle)
        {
            richStyle->richStyleFormat(contentStyle, sizeFactor, fontWeight, fontStyle, colorIndex);
        }
        else
        {
            style().richStyleFormat(contentStyle, sizeFactor, fontWeight, fontStyle, colorIndex);
        }
    }

    Font const *richStyleFont(Font::RichFormat::Style fontStyle) const
    {
        if (richStyle)
        {
            return richStyle->richStyleFont(fontStyle);
        }
        return style().richStyleFont(fontStyle);
    }

    void glInit()
    {
        drawable.addBuffer(new VertexBuf);
        shaders().build(drawable.program(), shaderId)
                << uMvpMatrix << uColor << uAtlas();

        glText.init(atlas(), self.font(), this);

        if (!image.isNull())
        {
            image->glInit();
        }

        if (!overlayImage.isNull())
        {
            overlayImage->glInit();
        }
    }

    void glDeinit()
    {
        drawable.clear();
        glText.deinit();
        if (!image.isNull())
        {
            image->glDeinit();
        }
        if (!overlayImage.isNull())
        {
            overlayImage->glDeinit();
        }
    }

    bool hasImage() const
    {
        return !image.isNull() && image->size() != ProceduralImage::Size(0, 0);
    }

    bool hasText() const
    {
        return !glText.text().isEmpty();
    }

    Vector2f imageSize() const
    {
        Vector2f size = image.isNull()? Vector2f() : image->size();
        // Override components separately.
        if (overrideImageSize.x > 0)
        {
            size.x = overrideImageSize.x;
        }
        if (overrideImageSize.y > 0)
        {
            size.y = overrideImageSize.y;
        }
        return size;
    }

    Vector2ui textSize() const
    {
        if (!glText.isBeingWrapped())
        {
            latestTextSize = glText.wrappedSize();
        }
        return latestTextSize;
    }

    /**
     * Determines where the label's image and text should be drawn.
     *
     * @param layout  Placement of the image and text.
     */
    void contentPlacement(ContentLayout &layout) const
    {
        Rectanglei const contentRect = self.contentRect();

        Vector2f const imgSize = imageSize() * imageScale;

        // Determine the sizes of the elements first.
        layout.image = Rectanglef::fromSize(imgSize);
        layout.text  = Rectanglei::fromSize(textSize());

        if (horizPolicy == Filled)
        {
            if (hasText() && textAlign & (AlignLeft | AlignRight))
            {
                if (fillMode == FillWithImage)
                {
                    layout.image.setWidth(int(contentRect.width()) - int(layout.text.width()) - gap);
                }
                else
                {
                    layout.text.setWidth(int(contentRect.width()) - int(layout.image.width()) - gap);
                }
            }
            else
            {
                layout.image.setWidth(contentRect.width());
                layout.text.setWidth(contentRect.width());
            }
        }
        if (vertPolicy == Filled)
        {
            if (hasText() && textAlign & (AlignTop | AlignBottom))
            {
                if (fillMode == FillWithImage)
                {
                    layout.image.setHeight(int(contentRect.height()) - int(layout.text.height()) - gap);
                }
                else
                {
                    layout.text.setHeight(int(contentRect.height()) - int(layout.image.height()) - gap);
                }
            }
            else
            {
                layout.image.setHeight(contentRect.height());
                layout.text.setHeight(contentRect.height());
            }
        }

        if (hasImage())
        {
            // Figure out how much room is left for the image.
            Rectanglef const rect = layout.image;

            // Fit the image.
            if (!imageFit.testFlag(FitToWidth)  || imageFit.testFlag(OriginalAspectRatio))
            {
                layout.image.setWidth(imageSize().x);
            }
            if (!imageFit.testFlag(FitToHeight) || imageFit.testFlag(OriginalAspectRatio))
            {
                layout.image.setHeight(imageSize().y);
            }

            // The width and height of the image have now been set. Now we'll apply
            // a suitable scaling factor.
            float const horizScale = rect.width() / layout.image.width();
            float const vertScale  = rect.height() / layout.image.height();
            float scale = 1;

            if (imageFit.testFlag(CoverArea))
            {
                scale = de::max(horizScale, vertScale);
            }
            else if (imageFit.testFlag(FitToWidth) && imageFit.testFlag(FitToHeight))
            {
                scale = de::min(horizScale, vertScale);
            }
            else if (imageFit.testFlag(FitToWidth))
            {
                scale = horizScale;
            }
            else if (imageFit.testFlag(FitToHeight))
            {
                scale = vertScale;
            }

            layout.image.setSize(Vector2f(layout.image.size()) * scale);

            // Apply additional user-provided image scaling factor now.
            if (horizPolicy == Filled)
            {
                layout.image.setWidth(imageScale * layout.image.width());
            }
            if (vertPolicy == Filled)
            {
                layout.image.setHeight(imageScale * layout.image.height());
            }
        }

        // By default the image and the text are centered over each other.
        layout.image.move((Vector2f(layout.text.size()) - layout.image.size()) / 2);

        if (hasImage() && hasText())
        {
            // Determine the position of the image in relation to the text
            // (keeping the image at its current position).
            if (textAlign & AlignLeft)
            {
                layout.text.moveLeft(layout.image.left() - layout.text.width() - gap);
            }
            if (textAlign & AlignRight)
            {
                layout.text.moveLeft(layout.image.right() + gap);
            }
            if (textAlign & AlignTop)
            {
                layout.text.moveTop(layout.image.top() - layout.text.height() - gap);
            }
            if (textAlign & AlignBottom)
            {
                layout.text.moveTop(layout.image.bottom() + gap);
            }

            // Align the image in relation to the text on the other axis.
            if (textAlign & (AlignLeft | AlignRight))
            {
                if (imageAlign & AlignTop)
                {
                    layout.image.moveTop(layout.text.top());
                }
                if (imageAlign & AlignBottom)
                {
                    layout.image.moveTop(layout.text.bottom() - layout.image.height());
                }
            }
            if (textAlign & (AlignTop | AlignBottom))
            {
                if (imageAlign & AlignLeft)
                {
                    layout.image.moveLeft(layout.text.left());
                }
                if (imageAlign & AlignRight)
                {
                    layout.image.moveLeft(layout.text.right() - layout.image.width());
                }
            }
        }

        // Align the final combination within the content.
        Rectanglef combined =
                (alignMode == AlignByCombination? (layout.image | layout.text) :
                 alignMode == AlignOnlyByImage?    layout.image :
                                                   layout.text);

        Vector2f delta = applyAlignment(align, combined.size(), contentRect);
        delta -= combined.topLeft;

        layout.image.move(delta);
        layout.text.move(delta.toVector2i());
    }

    /**
     * Determines the maximum amount of width available for text, taking into
     * account the given constraints for the possible image of the label.
     */
    int availableTextWidth() const
    {
        int w = 0;
        int h = 0;

        // The theorical upper limit is the entire view (when expanding) or
        // the given widget width.
        if (horizPolicy == Expand)
        {
            // Expansion can occur to full view width.
            w = root().viewSize().x - (margin().x + margin().z);
        }
        else
        {
            w = self.rule().width().valuei() - (margin().x + margin().z);
        }
        if (vertPolicy != Expand)
        {
            h = self.rule().height().valuei() - (margin().y + margin().w);
        }

        if (hasImage())
        {
            if (textAlign & (AlignLeft | AlignRight))
            {
                // Image will be placed beside the text.
                Vector2f imgSize = imageSize() * imageScale;

                if (vertPolicy != Expand)
                {
                    if (imageFit & FitToHeight && imgSize.y > h)
                    {
                        float factor = float(h) / imgSize.y;
                        imgSize.y *= factor;
                        if (imageFit & OriginalAspectRatio) imgSize.x *= factor;
                    }
                }

                w -= gap + imgSize.x;
            }
        }
        // Apply an optional manual constraint to the text width.
        if (maxTextWidth)
        {
            return de::min(maxTextWidth->valuei(), w);
        }
        return w;
    }

    void updateSize()
    {
        // Figure out the actual size of the content.
        ContentLayout layout;
        contentPlacement(layout);
        Rectanglef combined = layout.image | layout.text;
        width->set (combined.width()  + self.margins().width().valuei());
        height->set(combined.height() + self.margins().height().valuei());
    }

    void updateAppearanceAnimation()
    {
        if (appearType != AppearInstantly)
        {
            float const target = (appearType == AppearGrowHorizontally? width->value() : height->value());
            if (!fequal(appearSize->animation().target(), target))
            {
                appearSize->set(target, appearSpan);
            }
        }
    }

    void updateGeometry()
    {
        // Update the image on the atlas.
        if (!image.isNull() && image->update())
        {
            self.requestGeometry();
        }
        if (!overlayImage.isNull() && overlayImage->update())
        {
            self.requestGeometry();
        }

        glText.setLineWrapWidth(availableTextWidth());
        if (glText.update())
        {
            // Need to recompose.
            updateSize();
            self.requestGeometry();
        }

        Rectanglei pos;
        if (!self.hasChangedPlace(pos) && !self.geometryRequested())
        {
            // Nothing changed.
            return;
        }

        VertexBuf::Builder verts;
        self.glMakeGeometry(verts);
        drawable.buffer<VertexBuf>().setVertices(gl::TriangleStrip, verts, gl::Static);

        self.requestGeometry(false);
    }

    void draw()
    {
        updateGeometry();

        self.updateModelViewProjection(uMvpMatrix);
        drawable.draw();
    }

    Rule const *widthRule() const
    {
        switch (appearType)
        {
        case AppearInstantly:
        case AppearGrowVertically:
            if (horizPolicy == Expand) return width;
            break;

        case AppearGrowHorizontally:
            if (horizPolicy == Expand) return appearSize;
            break;
        }
        return 0;
    }

    Rule const *heightRule() const
    {
        switch (appearType)
        {
        case AppearInstantly:
        case AppearGrowHorizontally:
            if (vertPolicy == Expand) return outHeight;
            break;

        case AppearGrowVertically:
            if (vertPolicy == Expand) return appearSize;
            break;
        }
        return 0;
    }
};

LabelWidget::LabelWidget(String const &name) : GuiWidget(name), d(new Impl(this))
{}

AssetGroup &LabelWidget::assets()
{
    return d->assets;
}

void LabelWidget::setText(String const &text)
{
    if (text != d->glText.text())
    {
        d->glText.setText(text);
    }
}

void LabelWidget::setImage(Image const &image)
{
    if (!image.isNull())
    {
        AtlasProceduralImage *proc = new AtlasProceduralImage(*this);
        proc->setImage(image);
        d->image.reset(proc);
    }
    else
    {
        d->image.reset();
    }
}

void LabelWidget::setImage(ProceduralImage *procImage)
{
    d->image.reset(procImage);
}

void LabelWidget::setStyleImage(DotPath const &id, String const &heightFromFont)
{
    if (!id.isEmpty())
    {
        setImage(new StyleProceduralImage(id, *this));
        if (!heightFromFont.isEmpty())
        {
            setOverrideImageSize(style().fonts().font(heightFromFont).height().value());
        }
    }
}

ProceduralImage *LabelWidget::image() const
{
    return d->image.data();
}

void LabelWidget::setOverlayImage(ProceduralImage *overlayProcImage, ui::Alignment const &alignment)
{
    d->overlayImage.reset(overlayProcImage);
    d->overlayAlign = alignment;
}

String LabelWidget::text() const
{
    return d->glText.text();
}

Vector2ui LabelWidget::textSize() const
{
    return d->textSize();
}

Rule const &LabelWidget::contentWidth() const
{
    return *d->width;
}

Rule const &LabelWidget::contentHeight() const
{
    return *d->height;
}

void LabelWidget::setTextGap(DotPath const &styleRuleId)
{
    d->gapId = styleRuleId;
    d->updateStyle();
}

DotPath const &LabelWidget::textGap() const
{
    return d->gapId;
}

void LabelWidget::setTextShadow(TextShadow shadow, DotPath const &colorId)
{
    d->textShadow = shadow;
    d->textShadowColorId = colorId;
    d->updateStyle();
}

void LabelWidget::setFillMode(FillMode fillMode)
{
    d->fillMode = fillMode;
}

void LabelWidget::setAlignment(Alignment const &align, AlignmentMode mode)
{
    d->align = align;
    d->alignMode = mode;
}

void LabelWidget::setTextAlignment(Alignment const &textAlign)
{
    d->textAlign = textAlign;
}

ui::Alignment LabelWidget::textAlignment() const
{
    return d->textAlign;
}

void LabelWidget::setTextLineAlignment(Alignment const &textLineAlign)
{
    d->lineAlign = textLineAlign;
}

void LabelWidget::setTextModulationColorf(Vector4f const &colorf)
{
    d->textGLColor = colorf;
    requestGeometry();
}

Vector4f LabelWidget::textModulationColorf() const
{
    return d->textGLColor;
}

void LabelWidget::setImageAlignment(Alignment const &imageAlign)
{
    d->imageAlign = imageAlign;
}

void LabelWidget::setImageFit(ContentFit const &fit)
{
    d->imageFit = fit;
}

void LabelWidget::setMaximumTextWidth(int pixels)
{
    setMaximumTextWidth(Const(pixels));
}

void LabelWidget::setMaximumTextWidth(Rule const &pixels)
{
    changeRef(d->maxTextWidth, pixels);
    requestGeometry();
}

void LabelWidget::setMinimumHeight(Rule const &minHeight)
{
    d->minHeight->setSource(minHeight);
}

void LabelWidget::setTextStyle(Font::RichFormat::IStyle const *richStyle)
{
    d->richStyle = richStyle;
}

void LabelWidget::setOverrideImageSize(Vector2f const &size)
{
    d->overrideImageSize = size;
}

Vector2f LabelWidget::overrideImageSize() const
{
    return d->overrideImageSize;
}

void LabelWidget::setOverrideImageSize(float widthAndHeight)
{
    d->overrideImageSize = Vector2f(widthAndHeight, widthAndHeight);
}

void LabelWidget::setImageScale(float scaleFactor)
{
    d->imageScale = scaleFactor;
}

void LabelWidget::setImageColor(Vector4f const &imageColor)
{
    d->imageColor = imageColor;
    requestGeometry();
}

bool LabelWidget::hasImage() const
{
    return d->hasImage();
}

void LabelWidget::update()
{
    GuiWidget::update();

    // Check for visibility changes that affect asset readiness.
    bool visibleNow = isVisible();
    if (d->wasVisible && !visibleNow)
    {
        d->assets.setPolicy(d->glText, AssetGroup::Ignore);
    }
    else if (!d->wasVisible && visibleNow)
    {
        d->assets.setPolicy(d->glText, AssetGroup::Required);
    }
    d->wasVisible = visibleNow;

    if (isInitialized())
    {
        d->updateGeometry();
    }
    d->updateAppearanceAnimation();
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

    if (d->hasImage())
    {
        d->image->setColor(d->imageColor);
        d->image->glMakeGeometry(verts, layout.image);
    }
    if (d->hasText())
    {
        // Shadow behind the text.
        if (d->textShadow == RectangleShadow)
        {
            Rectanglef textBox = Rectanglef::fromSize(textSize());
            ui::applyAlignment(d->lineAlign, textBox, layout.text);
            int const boxSize = toDevicePixels(114);
            Vector2f const off(0, textBox.height() * .08f);
            Vector2f const hoff(textBox.height()/2, 0);
            verts.makeFlexibleFrame(Rectanglef(textBox.midLeft() + hoff + off,
                                               textBox.midRight() - hoff + off)
                                        .expanded(boxSize),
                                    boxSize,
                                    d->shadowColor,
                                    root().atlas().imageRectf(root().borderGlow()));
        }

        d->glText.makeVertices(verts, layout.text, d->lineAlign, d->lineAlign, d->textGLColor);
    }

    if (!d->overlayImage.isNull())
    {
        Rectanglef rect = Rectanglef::fromSize(d->overlayImage->size());
        applyAlignment(d->overlayAlign, rect, contentRect());
        d->overlayImage->glMakeGeometry(verts, rect);
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
    GuiWidget::viewResized();
    updateModelViewProjection(d->uMvpMatrix);
}

void LabelWidget::setWidthPolicy(SizePolicy policy)
{
    d->horizPolicy = policy;
    if (policy == Expand)
    {
        rule().setInput(Rule::Width, *d->widthRule());
    }
    else
    {
        rule().clearInput(Rule::Width);
    }
}

void LabelWidget::setHeightPolicy(SizePolicy policy)
{
    d->vertPolicy = policy;
    if (policy == Expand)
    {
        rule().setInput(Rule::Height, *d->heightRule());
    }
    else
    {
        rule().clearInput(Rule::Height);
    }
}

void LabelWidget::setAppearanceAnimation(AppearanceAnimation method, TimeDelta const &span)
{
    d->appearType = method;
    d->appearSpan = span;

    if (Rule const *w = d->widthRule())
    {
        rule().setInput(Rule::Width, *w);
    }
    if (Rule const *h = d->heightRule())
    {
        rule().setInput(Rule::Height, *h);
    }
}

void LabelWidget::setShaderId(DotPath const &shaderId)
{
    d->shaderId = shaderId;
}

GLProgram &LabelWidget::shaderProgram()
{
    return d->drawable.program();
}

LabelWidget *LabelWidget::newWithText(String const &label, GuiWidget *parent)
{
    LabelWidget *w = new LabelWidget;
    w->setText(label);
    if (parent)
    {
        parent->add(w);
    }
    return w;
}

} // namespace de
