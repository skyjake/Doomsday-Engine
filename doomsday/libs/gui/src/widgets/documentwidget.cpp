/** @file documentwidget.cpp  Widget for displaying large amounts of text.
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

#include "de/documentwidget.h"
#include "de/progresswidget.h"
#include "de/textdrawable.h"

#include <de/drawable.h>

namespace de {

DE_GUI_PIMPL(DocumentWidget),
public Font::RichFormat::IStyle
{
    ProgressWidget *progress = nullptr;

    // Style.
    DotPath colorIds[Font::RichFormat::MaxColors];
    ColorBank::Color normalColor;
    ColorBank::Color highlightColor;
    ColorBank::Color dimmedColor;
    ColorBank::Color accentColor;
    ColorBank::Color dimAccentColor;

    // State.
    ui::SizePolicy widthPolicy = ui::Expand;
    const Rule *maxLineWidth = nullptr;
    ConstantRule *contentMaxWidth = new ConstantRule(0);
    int oldScrollY = 0;
    String styledText;
    String text;

    // GL objects.
    TextDrawable glText;
    GuiVertexBuilder bgVerts;
    GuiVertexBuilder textVerts;
    Mat4f scrollMvpMatrix;

    Impl(Public *i) : Base(i)
    {
        colorIds[Font::RichFormat::NormalColor]    = "document.normal";
        colorIds[Font::RichFormat::HighlightColor] = "document.highlight";
        colorIds[Font::RichFormat::DimmedColor]    = "document.dimmed";
        colorIds[Font::RichFormat::AccentColor]    = "document.accent";
        colorIds[Font::RichFormat::DimAccentColor] = "document.dimaccent";

        updateStyle();

        maxLineWidth = holdRef(rule("document.line.width"));

        // Widget to show while lines are being wrapped.
        progress = new ProgressWidget("progress-indicator");
        progress->setColor("progress.dark.wheel");
        progress->setShadowColor("progress.dark.shadow");
        progress->rule().setRect(self().rule());
        progress->hide();
        self().add(progress);
    }

    ~Impl()
    {
        releaseRef(contentMaxWidth);
        releaseRef(maxLineWidth);
    }

    void updateStyle()
    {
        const Style &st = style();

        normalColor    = st.colors().color(colorIds[Font::RichFormat::NormalColor]);
        highlightColor = st.colors().color(colorIds[Font::RichFormat::HighlightColor]);
        dimmedColor    = st.colors().color(colorIds[Font::RichFormat::DimmedColor]);
        accentColor    = st.colors().color(colorIds[Font::RichFormat::AccentColor]);
        dimAccentColor = st.colors().color(colorIds[Font::RichFormat::DimAccentColor]);

        glText.setFont(self().font());
        self().requestGeometry();
    }

    Font::RichFormat::IStyle::Color richStyleColor(int index) const
    {
        switch (index)
        {
        default:
        case Font::RichFormat::NormalColor:
            return normalColor;

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

    void richStyleFormat(int contentStyle,
                         float &sizeFactor,
                         Font::RichFormat::Weight &fontWeight,
                         Font::RichFormat::Style &fontStyle,
                         int &colorIndex) const
    {
        return style().richStyleFormat(contentStyle, sizeFactor, fontWeight, fontStyle, colorIndex);
    }

    const Font *richStyleFont(Font::RichFormat::Style fontStyle) const
    {
        if (fontStyle == Font::RichFormat::Monospace)
        {
            return &style().fonts().font(DE_STR("document.monospace"));
        }
        return style().richStyleFont(fontStyle);
    }

    void glInit()
    {
        atlas().audienceForReposition() += this;

        glText.init(atlas(), self().font(), this);

        self().setIndicatorUv(atlas().imageRectf(root().solidWhitePixel()).middle());
    }

    void glDeinit()
    {
        atlas().audienceForReposition() -= this;
        glText.deinit();
        bgVerts.clear();
        textVerts.clear();
    }

    void atlasContentRepositioned(Atlas &atlas)
    {
        self().setIndicatorUv(atlas.imageRectf(root().solidWhitePixel()).middle());
        self().requestGeometry();
    }

    void updateGeometry()
    {
        // If scroll position has changed, must update text geometry.
        int scrollY = self().scrollPositionY().valuei();
        if (oldScrollY != scrollY)
        {
            oldScrollY = scrollY;
            self().requestGeometry();
        }

        Rectanglei pos;
        if (self().hasChangedPlace(pos))
        {
            self().requestGeometry();
        }

        // Make sure the text has been wrapped for the current dimensions.
        int wrapWidth;
        if (widthPolicy == ui::Expand)
        {
            wrapWidth = maxLineWidth->valuei();
        }
        else
        {
            wrapWidth = self().rule().width().valuei() - self().margins().width().valuei();
        }
        glText.setLineWrapWidth(wrapWidth);
        if (glText.update())
        {
            // Text is ready for drawing?
            if (!glText.isBeingWrapped() && progress->isVisible())
            {
                contentMaxWidth->set(de::max(contentMaxWidth->value(), float(glText.wrappedSize().x)));
                self().setContentSize(Vec2ui(contentMaxWidth->valuei(), glText.wrappedSize().y));
                progress->hide();
            }

            self().requestGeometry();
        }

        if (!self().geometryRequested()) return;

        // Background and scroll indicator.
        bgVerts.clear();
        self().glMakeGeometry(bgVerts);

        if (!progress->isVisible())
        {
            DE_ASSERT(glText.isReady());

            // Determine visible range of lines.
            const Font &font     = self().font();
            int contentHeight    = de::min(self().contentHeight(), self().rule().height().valuei());
            const int extraLines = 1;
            int numVisLines      = contentHeight / font.lineSpacing().valuei() + 2 * extraLines;
            int firstVisLine     = scrollY / font.lineSpacing().valuei() - extraLines + 1;

            // Update visible range and release/alloc lines accordingly.
            Rangei visRange(firstVisLine, firstVisLine + numVisLines);
            if (visRange != glText.range())
            {
                glText.setRange(visRange);
                glText.update(); // alloc visible lines

                textVerts.clear();
                glText.makeVertices(textVerts, Vec2i(0, 0), ui::AlignLeft);

                // Update content size to match the generated vertices exactly.
                self().setContentWidth(glText.verticesMaxWidth());
            }

            scrollMvpMatrix = root().projMatrix2D() *
                    Mat4f::translate(Vec2f(self().contentRule().left().valuei(),
                                                 self().contentRule().top().valuei()));
        }

        // Geometry is now up to date.
        self().requestGeometry(false);
    }

    void draw()
    {
        updateGeometry();

        const Vec4f color = Vec4f(1, 1, 1, self().visibleOpacity());

        auto &painter = root().painter();
        if (bgVerts)
        {
            painter.setColor(color);
            painter.drawTriangleStrip(bgVerts);
        }
        if (textVerts)
        {
            painter.setModelViewProjection(scrollMvpMatrix);

            // Update the scissor for the text.
            const auto oldClip = painter.normalizedScissor();
            painter.setNormalizedScissor(oldClip & self().normalizedContentRect());
            painter.setColor(color);
            painter.drawTriangleStrip(textVerts);
            painter.setNormalizedScissor(oldClip);
            painter.setModelViewProjection(root().projMatrix2D());
        }
    }
};

DocumentWidget::DocumentWidget(const String &name)
    : ScrollAreaWidget(name)
    , d(new Impl(this))
{
    setWidthPolicy(ui::Expand);
    rule().setInput(Rule::Height, contentRule().height() + margins().height());

    enableIndicatorDraw(true);
}

void DocumentWidget::setText(const String &styledText)
{
    if (styledText != d->glText.text())
    {
        // Show the progress indicator until the text is ready for drawing.
        d->textVerts.clear();

        d->progress->show();

        int indSize = rule("document.progress").valuei();
        setContentSize(Vec2i(indSize, indSize));
        d->contentMaxWidth->set(indSize);

        d->styledText = styledText;

        d->glText.clear();
        d->glText.setText(styledText);
        d->glText.setRange(Rangei()); // none visible initially -- updated later

        requestGeometry();
    }
}

String DocumentWidget::text() const
{
    return d->glText.text();
}

void DocumentWidget::setWidthPolicy(ui::SizePolicy policy)
{
    d->widthPolicy = policy;

    if (policy == ui::Expand)
    {
        rule().setInput(Rule::Width, *d->contentMaxWidth + margins().width());
    }
    else
    {
        rule().clearInput(Rule::Width);
    }

    requestGeometry();
}

void DocumentWidget::setMaximumLineWidth(const Rule &maxWidth)
{
    changeRef(d->maxLineWidth, maxWidth);
    d->contentMaxWidth->set(0);
    requestGeometry();
}

void DocumentWidget::setStyleColor(Font::RichFormat::Color id, const DotPath &colorName)
{
    DE_ASSERT(id != Font::RichFormat::AltAccentColor); // FIXME: not implemented!

    if (id >= 0 && id < Font::RichFormat::MaxColors)
    {
        d->colorIds[id] = colorName;
        updateStyle();
    }
}

ProgressWidget &DocumentWidget::progress()
{
    return *d->progress;
}

void DocumentWidget::viewResized()
{
    ScrollAreaWidget::viewResized();
    requestGeometry();
}

void DocumentWidget::drawContent()
{
    d->draw();
    ScrollAreaWidget::drawContent();
}

void DocumentWidget::glInit()
{
    ScrollAreaWidget::glInit();
    d->glInit();
}

void DocumentWidget::glDeinit()
{
    ScrollAreaWidget::glDeinit();
    d->glDeinit();
}

void DocumentWidget::updateStyle()
{
    ScrollAreaWidget::updateStyle();
    d->updateStyle();
}

} // namespace de
