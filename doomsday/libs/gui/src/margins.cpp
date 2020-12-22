/** @file margins.cpp
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

#include "de/ui/margins.h"
#include "de/ui/style.h"

#include <de/indirectrule.h>
#include <de/operatorrule.h>

namespace de {
namespace ui {

enum Side
{
    SideLeft,
    SideRight,
    SideTop,
    SideBottom,

    LeftRight,
    TopBottom,

    MAX_SIDES
};

DE_PIMPL(Margins)
{
    const Rule *inputs[4];
    IndirectRule *outputs[MAX_SIDES];

    Impl(Public *i, const DotPath &defaultId) : Base(i)
    {
        zap(inputs);
        zap(outputs);

        for (int i = 0; i < 4; ++i)
        {
            setInput(i, defaultId);
        }
    }

    ~Impl()
    {
        for (int i = 0; i < 4; ++i)
        {
            releaseRef(inputs[i]);
        }
        for (int i = 0; i < int(MAX_SIDES); ++i)
        {
            if (outputs[i])
            {
                outputs[i]->unsetSource();
                releaseRef(outputs[i]);
            }
        }
    }

    void setInput(int side, const DotPath &styleId)
    {
        setInput(side, Style::get().rules().rule(styleId));
    }

    void setInput(int side, const Rule &rule)
    {
        DE_ASSERT(side >= 0 && side < 4);
        changeRef(inputs[side], rule);
        updateOutput(side);

        DE_NOTIFY_VAR(Change, i)
        {
            i->marginsChanged();
        }
    }

    void updateOutput(int side)
    {
        if (side < 4 && outputs[side] && inputs[side])
        {
            outputs[side]->setSource(*inputs[side]);
        }

        // Update the sums.
        if (side == LeftRight || side == SideLeft || side == SideRight)
        {
            if (outputs[LeftRight] && inputs[SideLeft] && inputs[SideRight])
            {
                outputs[LeftRight]->setSource(*inputs[SideLeft] + *inputs[SideRight]);
            }
        }
        else if (side == TopBottom || side == SideTop || side == SideBottom)
        {
            if (outputs[TopBottom] && inputs[SideTop] && inputs[SideBottom])
            {
                outputs[TopBottom]->setSource(*inputs[SideTop] + *inputs[SideBottom]);
            }
        }
    }

    const Rule &getOutput(int side)
    {
        if (!outputs[side])
        {
            outputs[side] = new IndirectRule;
            updateOutput(side);
        }
        return *outputs[side];
    }

    DE_PIMPL_AUDIENCE(Change)
};

DE_AUDIENCE_METHOD(Margins, Change)

Margins::Margins(const String &defaultMargin) : d(new Impl(this, defaultMargin))
{}

Margins &Margins::set(Direction dir, const DotPath &marginId)
{
    d->setInput(dir == Left?  SideLeft  :
                dir == Right? SideRight :
                dir == Up?    SideTop   : SideBottom, marginId);
    return *this;
}

Margins &Margins::set(const DotPath &marginId)
{
    set(Left,  marginId);
    set(Right, marginId);
    set(Up,    marginId);
    set(Down,  marginId);
    return *this;
}

Margins &Margins::setLeft(const DotPath &leftMarginId)
{
    return set(ui::Left, leftMarginId);
}

Margins &Margins::setRight(const DotPath &rightMarginId)
{
    return set(ui::Right, rightMarginId);
}

Margins &Margins::setLeftRight(const DotPath &marginId)
{
    return set(ui::Left, marginId).set(ui::Right, marginId);
}

Margins &Margins::setTopBottom(const DotPath &marginId)
{
    return set(ui::Up, marginId).set(ui::Down, marginId);
}

Margins &Margins::setTop(const DotPath &topMarginId)
{
    return set(ui::Up, topMarginId);
}

Margins &Margins::setBottom(const DotPath &bottomMarginId)
{
    return set(ui::Down, bottomMarginId);
}

Margins &Margins::set(Direction dir, const Rule &rule)
{
    d->setInput(dir == Left?  SideLeft  :
                dir == Right? SideRight :
                dir == Up?    SideTop   : SideBottom, rule);
    return *this;
}

Margins &Margins::set(const Rule &rule)
{
    set(Left,  rule);
    set(Right, rule);
    set(Up,    rule);
    set(Down,  rule);
    return *this;
}

Margins &Margins::setAll(const Margins &margins)
{
    if (this == &margins) return *this;

    set(Left,  margins.left());
    set(Right, margins.right());
    set(Up,    margins.top());
    set(Down,  margins.bottom());
    return *this;
}

Margins &Margins::setZero()
{
    return set("");
}

Margins &Margins::setLeft(const Rule &rule)
{
    return set(ui::Left, rule);
}

Margins &Margins::setRight(const Rule &rule)
{
    return set(ui::Right, rule);
}

Margins &Margins::setTop(const Rule &rule)
{
    return set(ui::Up, rule);
}

Margins &Margins::setBottom(const Rule &rule)
{
    return set(ui::Down, rule);
}

const Rule &Margins::left() const
{
    return d->getOutput(SideLeft);
}

const Rule &Margins::right() const
{
    return d->getOutput(SideRight);
}

const Rule &Margins::top() const
{
    return d->getOutput(SideTop);
}

const Rule &Margins::bottom() const
{
    return d->getOutput(SideBottom);
}

const Rule &Margins::width() const
{
    return d->getOutput(LeftRight);
}

const Rule &Margins::height() const
{
    return d->getOutput(TopBottom);
}

const Rule &Margins::margin(Direction dir) const
{
    return d->getOutput(dir == Left?  SideLeft  :
                        dir == Right? SideRight :
                        dir == Up?    SideTop   : SideBottom);
}

Vec4i Margins::toVector() const
{
    return Vec4i(left().valuei(), top().valuei(), right().valuei(), bottom().valuei());
}

} // namespace ui
} // namespace de
