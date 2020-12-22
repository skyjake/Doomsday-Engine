/** @file mapoutlinewidget.cpp
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/gui/mapoutlinewidget.h"

#include <de/progresswidget.h>
#include <de/drawable.h>
#include <de/image.h>

using namespace de;

DE_GUI_PIMPL(MapOutlineWidget)
{
    using Player  = network::PlayerInfoPacket::Player;
    using Players = network::PlayerInfoPacket::Players;

    struct Marker {
        LabelWidget *  pin;
        LabelWidget *  label;
        AnimationRule *pos[2];
    };

    network::MapOutlinePacket outlinePacket;

    ProgressWidget *progress; // shown initially, before outline received
    DotPath oneSidedColorId{"inverted.altaccent"};
    DotPath twoSidedColorId{"altaccent"};

    // Outline.
    Rectanglei        mapBounds;
    DefaultVertexBuf *outlineVBuf = nullptr;

    // Player markers.
    Players           players;
    KeyMap<int, Vec2i>   oldPlayerPositions;
    KeyMap<int, Marker>  playerLabels;      // number and name on a round rect background
    DefaultVertexBuf *plrVBuf = nullptr; // tick marks

    // Drawing.
    Drawable  drawable;
    GLUniform uMvpMatrix { "uMvpMatrix", GLUniform::Mat4 };
    GLUniform uColor     { "uColor",     GLUniform::Vec4 };
    Animation mapOpacity { 0, Animation::Linear };

    Impl(Public *i) : Base(i)
    {
        self().add(progress = new ProgressWidget);
        progress->setMode(ProgressWidget::Indefinite);
        progress->setColor("progress.dark.wheel");
        progress->setShadowColor("progress.dark.shadow");
        progress->rule().setRect(self().rule());
    }

    void glInit()
    {
        drawable.addBuffer(outlineVBuf = new DefaultVertexBuf);
//        drawable.addBuffer(plrVBuf     = new DefaultVertexBuf);

        shaders().build(drawable.program(), "generic.textured.color_ucolor")
            << uMvpMatrix << uColor << uAtlas();
    }

    void glDeinit()
    {
        drawable.clear();
        outlineVBuf = nullptr;
        plrVBuf     = nullptr;
    }

    void makeOutline(const network::MapOutlinePacket &mapOutline)
    {
        if (!outlineVBuf) return;

        progress->setOpacity(0, 0.5);
        mapOpacity.setValue(1, 0.5);

        mapBounds = Rectanglei();

        const Vec4f oneSidedColor = style().colors().colorf(oneSidedColorId);
        const Vec4f twoSidedColor = style().colors().colorf(twoSidedColorId);

        DefaultVertexBuf::Vertices verts;
        DefaultVertexBuf::Type vtx;

        vtx.texCoord = atlas().imageRectf(root().solidWhitePixel()).middle();

        for (int i = 0; i < mapOutline.lineCount(); ++i)
        {
            const auto &line = mapOutline.line(i);

            vtx.rgba = (line.type == network::MapOutlinePacket::OneSidedLine? oneSidedColor : twoSidedColor);

            // Two vertices per line.
            vtx.pos = line.start; verts << vtx;
            vtx.pos = line.end;   verts << vtx;

            if (i > 0)
            {
                mapBounds.include(line.start);
            }
            else
            {
                mapBounds = {line.start, line.start}; // initialize
            }
            mapBounds.include(line.end);
        }

//        vtx.rgba = Vec4f(1, 0, 1, 1);
//        vtx.pos = mapBounds.topLeft; verts << vtx;
//        vtx.pos = mapBounds.bottomRight; verts << vtx;

        outlineVBuf->setVertices(gfx::Lines, verts, gfx::Static);
    }

    Vec2f playerViewPosition(int plrNum) const
    {
        if (players.contains(plrNum))
        {
            const auto &plr = players[plrNum];
            Vec3f pos = (modelMatrix() * Vec4f(plr.position.x, plr.position.y, 0.0f, 1.0f))
                            .toEuclidean();
            return pos;
        }
        return {};
    }

    void updatePlayerPositions()
    {
        for (const auto &plr : players)
        {
            auto mark = playerLabels.find(plr.first);
            if (mark != playerLabels.end())
            {
                const auto pos = playerViewPosition(plr.first);
                mark->second.pos[0]->set(pos.x);
                mark->second.pos[1]->set(pos.y);
            }
        }
    }

    void makePlayers()
    {
        // Create the player labels.
        {
            AutoRef<Rule> markerOffset = new ConstantRule(style().images().image("widget.pin").height());

            // Add new labels.
            for (const auto &plr : players)
            {
                const int   plrNum     = plr.first;
                const Vec2f pos        = playerViewPosition(plrNum);
                const Vec4f plrColor   = Vec4f(plr.second.color.toVec3f() / 255.0f, 1.0f);
                const float brightness = Image::hsv(plr.second.color).z;
                Marker *    mark;

                if (!playerLabels.contains(plrNum))
                {
                    playerLabels.insert(plrNum, {});

                    mark = &playerLabels[plrNum];
                    mark->pos[0] = new AnimationRule(pos.x);
                    mark->pos[1] = new AnimationRule(pos.y);

                    mark->pin = &self().addNew<LabelWidget>();
                    mark->pin->setStyleImage("widget.pin");
                    mark->pin->setImageFit(ui::OriginalSize);
                    mark->pin->setAlignment(ui::AlignBottom);
                    mark->pin->margins().setZero();
                    mark->pin->rule()
                        .setInput(Rule::Width, Const(3))
                        .setInput(Rule::Height, mark->pin->rule().width())
                        .setInput(Rule::Bottom, *mark->pos[1] + *markerOffset + rule("halfunit"))
                        .setMidAnchorX(*mark->pos[0]);

                    mark->label = &self().addNew<LabelWidget>();
                    mark->label->setSizePolicy(ui::Expand, ui::Expand);
                    mark->label->setFont("small");
                    mark->label->margins().set(rule("unit"));
                    mark->label->rule().setAnchorPoint({0.5f, 0.0f});
                    mark->label->rule()
                        .setInput(Rule::AnchorX, *mark->pos[0])
                        .setInput(Rule::AnchorY, *mark->pos[1] + *markerOffset);
                }
                else
                {
                    mark = &playerLabels[plrNum];

                    // Update positions.
                    mark->pos[0]->set(pos.x, 500_ms);
                    mark->pos[1]->set(pos.y, 500_ms);
                }

                mark->pin->setImageColor(plrColor);
                mark->label->set(Background(Vec4f(plrColor, 0.85f),
                                            Background::GradientFrameWithRoundedFill,
                                            Vec4f(0.0f),
                                            rule("halfunit").valuei()));
                mark->label->setTextColor(brightness < 0.5f ? "text" : "inverted.text");
                mark->label->setText(Stringf("%d: %s", plr.second.number, plr.second.name.c_str()));
            }
            // Remove old labels.
            for (auto i = playerLabels.begin(); i != playerLabels.end(); )
            {
                if (!players.contains(i->first))
                {
                    GuiWidget::destroy(i->second.label);
                    GuiWidget::destroy(i->second.pin);
                    releaseRef(i->second.pos[0]);
                    releaseRef(i->second.pos[1]);
                    i = playerLabels.erase(i);
                }
                else
                {
                    ++i;
                }
            }
        }

        if (!plrVBuf) return;

        root().window().glActivate();

        DefaultVertexBuf::Builder verts;
        for (const auto &plr : players)
        {
            DE_UNUSED(plr);

            // Position dot.

            // Gradient line to previous position.

            // Line to label.

        }
        plrVBuf->setVertices(gfx::TriangleStrip, verts, gfx::Static);

        root().window().glDone();
    }

    Mat4f modelMatrix() const
    {
        DE_ASSERT(outlineVBuf);

        if (mapBounds.isNull()) return Mat4f();

        const Rectanglef rect = self().contentRect();
        const float scale = de::min(rect.width()  / mapBounds.width(),
                                    rect.height() / mapBounds.height());
        return Mat4f::translate(rect.middle()) *
               Mat4f::scale    (Vec3f(scale, -scale, 1)) *
               Mat4f::translate(Vec2f(-mapBounds.middle()));
    }

    void clearPlayers()
    {
        oldPlayerPositions.clear();
        players.clear();
    }
};

MapOutlineWidget::MapOutlineWidget(const String &name)
    : GuiWidget(name)
    , d(new Impl(this))
{}

void MapOutlineWidget::setColors(const DotPath &oneSidedLine, const DotPath &twoSidedLine)
{
    d->oneSidedColorId = oneSidedLine;
    d->twoSidedColorId = twoSidedLine;
}

void MapOutlineWidget::setOutline(const network::MapOutlinePacket &mapOutline)
{
    // This is likely called wherever incoming network packets are being processed,
    // and thus there should currently be no active OpenGL context.
    root().window().glActivate();

    d->outlinePacket = mapOutline;
    d->makeOutline(mapOutline);
    d->clearPlayers();

    root().window().glDone();
}

void MapOutlineWidget::setPlayerInfo(const network::PlayerInfoPacket &plrInfo)
{
    for (const auto &plr : d->players)
    {
        d->oldPlayerPositions[plr.second.number] =
            Vec2i(plr.second.position.x, -plr.second.position.y);
    }
    d->players = plrInfo.players();
    d->makePlayers();
}

void MapOutlineWidget::drawContent()
{
    GuiWidget::drawContent();
    if (d->outlineVBuf && d->outlineVBuf->count())
    {
        auto &painter = root().painter();
        painter.flush();
        GLState::push().setNormalizedScissor(painter.normalizedScissor());
        d->uMvpMatrix = root().projMatrix2D() * d->modelMatrix();
        d->uColor = Vec4f(1, 1, 1, d->mapOpacity * visibleOpacity());
        d->drawable.draw();
        GLState::pop();
    }
}

void MapOutlineWidget::update()
{
    GuiWidget::update();
    if (geometryRequested())
    {
        d->makeOutline(d->outlinePacket);
        requestGeometry(false);
    }
}

void MapOutlineWidget::viewResized()
{
    GuiWidget::viewResized();
    d->updatePlayerPositions();
}

void MapOutlineWidget::glInit()
{
    GuiWidget::glInit();
    d->glInit();
}

void MapOutlineWidget::glDeinit()
{
    GuiWidget::glDeinit();
    d->glDeinit();
}
