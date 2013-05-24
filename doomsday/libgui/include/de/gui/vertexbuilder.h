/** @file vertexbuilder.h  Utility for composing triangle strips.
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

#ifndef LIBGUI_VERTEXBUILDER_H
#define LIBGUI_VERTEXBUILDER_H

#include <QVector>

namespace de {

/**
 * Utility for composing simple geometric constructs (using triangle strips).
 */
template <typename VertexType>
struct VertexBuilder
{
    struct Vertices : public QVector<VertexType> {
        Vertices &operator += (Vertices const &other) {
            concatenate(other, *this);
            return *this;
        }
        Vertices operator + (Vertices const &other) const {
            Vertices v(*this);
            return v += other;
        }
    };

    static void concatenate(Vertices const &stripSequence, Vertices &destStrip)
    {
        if(!stripSequence.size()) return;
        if(!destStrip.isEmpty())
        {
            destStrip << destStrip.back();
            destStrip << stripSequence.front();
        }
        destStrip << stripSequence;
    }
};

} // namespace de

#endif // LIBGUI_VERTEXBUILDER_H
