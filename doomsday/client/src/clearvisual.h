/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CLEARVISUAL_H
#define CLEARVISUAL_H

#include <QFlags>
#include <de/Vector>
#include "visual.h"

/**
 * Visual for clearing the surface.
 */
class ClearVisual : public Visual
{
    Q_OBJECT

public:
    enum Flag
    {
        ColorBuffer = 0x1,
        DepthBuffer = 0x2
    };
    Q_DECLARE_FLAGS(Flags, Flag);

public:
    explicit ClearVisual(Flags flags = Flags(ColorBuffer | DepthBuffer),
                         const de::Vector4f& color = de::Vector4f(0, 0, 0, 1),
                         Visual *parent = 0);

    void drawSelf(DrawingStage stage) const;

private:
    de::Vector4f _color;
    Flags _flags;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ClearVisual::Flags);

#endif // CLEARVISUAL_H
