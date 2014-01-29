/** @file documentwidget.cpp  Widget for displaying large amounts of text.
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

#include "de/DocumentWidget"
#include "de/ProgressWidget"
#include "de/TextDrawable"

#include <de/Drawable>

namespace de {

static int const ID_BACKGROUND = 1; // does not scroll
static int const ID_TEXT = 2;       // scrolls

DENG_GUI_PIMPL(DocumentWidget),
public Font::RichFormat::IStyle
{
    typedef DefaultVertexBuf VertexBuf;

    ProgressWidget *progress;

    // Style.
    ColorBank::Color normalColor;
    ColorBank::Color highlightColor;
    ColorBank::Color dimmedColor;
    ColorBank::Color accentColor;
    ColorBank::Color dimAccentColor;

    // State.
    ui::SizePolicy widthPolicy;
    int maxLineWidth;
    int oldScrollY;
    String styledText;
    String text;

    // GL objects.
    TextDrawable glText;
    Drawable drawable;
    Matrix4f modelMatrix;
    GLUniform uMvpMatrix;
    GLUniform uScrollMvpMatrix;
    GLUniform uColor;
    GLState clippedTextState;

    Instance(Public *i)
        : Base(i)
        , progress(0)
        , widthPolicy(ui::Expand)
        , maxLineWidth(1000)
        , oldScrollY(0)
        , uMvpMatrix      ("uMvpMatrix", GLUniform::Mat4)
        , uScrollMvpMatrix("uMvpMatrix", GLUniform::Mat4)
        , uColor          ("uColor",     GLUniform::Vec4)
    {
        updateStyle();

        // Widget to show while lines are being wrapped.
        progress = new ProgressWidget;
        progress->setColor("progress.dark.wheel");
        progress->setShadowColor("progress.dark.shadow");
        progress->rule().setRect(self.rule());
        progress->hide();
        self.add(progress);
    }

    void updateStyle()
    {
        Style const &st = style();

        normalColor    = st.colors().color("document.normal");
        highlightColor = st.colors().color("document.highlight");
        dimmedColor    = st.colors().color("document.dimmed");
        accentColor    = st.colors().color("document.accent");
        dimAccentColor = st.colors().color("document.dimaccent");

        glText.setFont(self.font());
        self.requestGeometry();
    }

    Font::RichFormat::IStyle::Color richStyleColor(int index) const
    {
        switch(index)
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

    void glInit()
    {        
        atlas().audienceForReposition += this;

        glText.init(atlas(), self.font(), this);

        self.setIndicatorUv(atlas().imageRectf(root().solidWhitePixel()).middle());

        drawable.addBuffer(ID_BACKGROUND, new VertexBuf);
        drawable.addBuffer(ID_TEXT,       new VertexBuf);

        shaders().build(drawable.program(), "generic.textured.color_ucolor")
                << uMvpMatrix << uColor << uAtlas();

        shaders().build(drawable.addProgram(ID_TEXT), "generic.textured.color_ucolor")
                << uScrollMvpMatrix << uColor << uAtlas();
        drawable.setProgram(ID_TEXT, drawable.program(ID_TEXT));
        drawable.setState(ID_TEXT, clippedTextState);
    }

    void glDeinit()
    {
        atlas().audienceForReposition -= this;
        glText.deinit();
        drawable.clear();
    }

    void atlasContentRepositioned(Atlas &atlas)
    {
        self.setIndicatorUv(atlas.imageRectf(root().solidWhitePixel()).middle());
        self.requestGeometry();
    }

    void updateGeometry()
    {
        // If scroll position has changed, must update text geometry.
        int scrollY = self.scrollPositionY().valuei();
        if(oldScrollY != scrollY)
        {
            oldScrollY = scrollY;
            self.requestGeometry();
        }

        Rectanglei pos;
        if(self.hasChangedPlace(pos))
        {
            self.requestGeometry();
        }

        // Make sure the text has been wrapped for the current dimensions.
        int wrapWidth;
        if(widthPolicy == ui::Expand)
        {
            wrapWidth = maxLineWidth;
        }
        else
        {
            wrapWidth = self.rule().width().valuei() - self.margins().width().valuei();
        }
        glText.setLineWrapWidth(wrapWidth);
        if(glText.update())
        {
            // Text is ready for drawing.
            if(progress->isVisible())
            {
                self.setContentSize(glText.wrappedSize());
                progress->hide();
            }

            self.requestGeometry();
        }

        if(!self.geometryRequested()) return;

        // Background and scroll indicator.
        VertexBuf::Builder verts;
        self.glMakeGeometry(verts);
        drawable.buffer<VertexBuf>(ID_BACKGROUND)
                .setVertices(gl::TriangleStrip, verts, self.isScrolling()? gl::Dynamic : gl::Static);

        uMvpMatrix = root().projMatrix2D();

        if(!progress->isVisible())
        {
            DENG2_ASSERT(glText.isReady());

            // Determine visible range of lines.
            Font const &font = self.font();
            int contentHeight = self.contentHeight();
            int const extraLines = 1;
            int numVisLines = contentHeight / font.lineSpacing().valuei() + 2 * extraLines;
            int firstVisLine = scrollY / font.lineSpacing().valuei() - extraLines + 1;

            // Update visible range and release/alloc lines accordingly.
            Rangei visRange(firstVisLine, firstVisLine + numVisLines);
            if(visRange != glText.range())
            {
                glText.setRange(visRange);
                glText.update(); // alloc visible lines

                VertexBuf::Builder verts;
                glText.makeVertices(verts, Vector2i(0, 0), ui::AlignLeft);
                drawable.buffer<VertexBuf>(ID_TEXT).setVertices(gl::TriangleStrip, verts, gl::Static);
            }

            uScrollMvpMatrix = root().projMatrix2D() *
                    Matrix4f::translate(Vector2f(self.contentRule().left().valuei(),
                                                 self.contentRule().top().valuei()));
        }

        // Geometry is now up to date.
        self.requestGeometry(false);
    }

    void draw()
    {
        updateGeometry();

        uColor = Vector4f(1, 1, 1, self.visibleOpacity());

        // Update the scissor for the text.
        clippedTextState = GLState::current();
        clippedTextState.setNormalizedScissor(self.normalizedContentRect());

        drawable.draw();
    }
};

DocumentWidget::DocumentWidget(String const &name)
    : ScrollAreaWidget(name)
    , d(new Instance(this))
{
    setWidthPolicy(ui::Expand);

    rule().setInput(Rule::Height, contentRule().height() + margins().height());
}

void DocumentWidget::setText(String const &styledText)
{
    if(styledText != d->glText.text())
    {
        // Show the progress indicator until the text is ready for drawing.
        if(d->drawable.hasBuffer(ID_TEXT))
        {
            d->drawable.buffer(ID_TEXT).clear();
        }

        d->progress->show();
        int indSize = style().rules().rule("document.progress").valuei();
        setContentSize(Vector2i(indSize, indSize));

        d->styledText = styledText;

        d->glText.clear();
        d->glText.setText(styledText);
        d->glText.setRange(Rangei()); // none visible initially -- updated later

        requestGeometry();
    }
}

void DocumentWidget::setWidthPolicy(ui::SizePolicy policy)
{
    d->widthPolicy = policy;

    if(policy == ui::Expand)
    {
        rule().setInput(Rule::Width, contentRule().width() + margins().width());
    }
    else
    {
        rule().clearInput(Rule::Width);
    }

    requestGeometry();
}

void DocumentWidget::setMaximumLineWidth(int maxWidth)
{
    d->maxLineWidth = maxWidth;
    requestGeometry();
}

void DocumentWidget::viewResized()
{
    ScrollAreaWidget::viewResized();

    d->uMvpMatrix = root().projMatrix2D();
    requestGeometry();
}

void DocumentWidget::update()
{
    ScrollAreaWidget::update();
}

void DocumentWidget::drawContent()
{
    d->draw();
}

bool DocumentWidget::handleEvent(Event const &event)
{
    return ScrollAreaWidget::handleEvent(event);
}

void DocumentWidget::glInit()
{
    d->glInit();
}

void DocumentWidget::glDeinit()
{
    d->glDeinit();
}

void DocumentWidget::glMakeGeometry(DefaultVertexBuf::Builder &verts)
{
    ScrollAreaWidget::glMakeGeometry(verts);

    glMakeScrollIndicatorGeometry(verts, Vector2f(rule().left().value() + margins().left().value(),
                                                  rule().top().value()  + margins().top().value()));
}

void DocumentWidget::updateStyle()
{
    d->updateStyle();
}

} // namespace de
