/** @file vertexbuilder.h  Utility for composing triangle strips.
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

#ifndef LIBGUI_VERTEXBUILDER_H
#define LIBGUI_VERTEXBUILDER_H

#include <de/list.h>
#include <de/vector.h>
#include <de/matrix.h>
#include <de/rectangle.h>

namespace de {

/**
 * Utility for composing simple geometric constructs (using triangle strips).
 *
 * @ingroup gl
 */
template <typename VertexType>
struct VertexBuilder
{
    struct Vertices : public List<VertexType> {
        using Base = List<VertexType>;
        Vertices() {
            //Base::reserve(64);
        }
        explicit operator bool() const {
            return !Base::isEmpty();
        }
        void transform(const Mat4f &matrix) {
            for (dsize i = 0; i < Base::size(); ++i) {
                (*this)[i].pos = matrix * (*this)[i].pos;
            }
        }
        Vertices &operator += (const Vertices &other) {
            concatenate(other, *this);
            return *this;
        }
        Vertices operator + (const Vertices &other) const {
            Vertices v(*this);
            return v += other;
        }
        Vertices &makeQuad(const Rectanglef &rect, const Vec4f &color, const Vec2f &uv) {
            Vertices quad;
            VertexType v;
            v.rgba = color;
            v.texCoord = uv;
            v.pos = rect.topLeft;      quad << v;
            v.pos = rect.topRight();   quad << v;
            v.pos = rect.bottomLeft(); quad << v;
            v.pos = rect.bottomRight;  quad << v;
            return *this += quad;
        }
        Vertices &makeQuad(const Rectanglef &rect, const Rectanglef &uv) {
            Vertices quad;
            VertexType v;
            v.pos = rect.topLeft;      v.texCoord = uv.topLeft;      quad << v;
            v.pos = rect.topRight();   v.texCoord = uv.topRight();   quad << v;
            v.pos = rect.bottomLeft(); v.texCoord = uv.bottomLeft(); quad << v;
            v.pos = rect.bottomRight;  v.texCoord = uv.bottomRight;  quad << v;
            return *this += quad;
        }
        Vertices &makeQuad(const Rectanglef &rect, const Vec4f &color, const Rectanglef &uv,
                           const Mat4f *matrix = nullptr) {
            Vertices quad;
            VertexType v;
            v.rgba = color;
            v.pos = rect.topLeft;      v.texCoord = uv.topLeft;      quad << v;
            v.pos = rect.topRight();   v.texCoord = uv.topRight();   quad << v;
            v.pos = rect.bottomLeft(); v.texCoord = uv.bottomLeft(); quad << v;
            v.pos = rect.bottomRight;  v.texCoord = uv.bottomRight;  quad << v;
            if (matrix) quad.transform(*matrix);
            return *this += quad;
        }
        /// Makes a 3D quad with indirect UV coords. The points p1...p4 are specified
        /// with a clockwise winding (use Vertex3Tex2BoundsRgba).
        Vertices &makeQuadIndirect(const Vec3f &p1, const Vec3f &p2,
                                   const Vec3f &p3, const Vec3f &p4,
                                   const Vec4f &color, const Rectanglef &uv,
                                   const Vec4f &uvBounds, const Vec2f &texSize) {
            Vertices quad;
            VertexType v;
            v.rgba = color;
            v.texBounds = uvBounds;
            v.texCoord[1] = texSize;
            v.pos = p1; v.texCoord[0] = uv.topLeft;      quad << v;
            v.pos = p2; v.texCoord[0] = uv.topRight();   quad << v;
            v.pos = p4; v.texCoord[0] = uv.bottomLeft(); quad << v;
            v.pos = p3; v.texCoord[0] = uv.bottomRight;  quad << v;
            return *this += quad;
        }
        Vertices &makeCubeIndirect(const Vec3f &minPoint,
                                   const Vec3f &maxPoint,
                                   const Rectanglef &uv,
                                   const Vec4f &uvBounds,
                                   const Vec2f &texSize,
                                   Vec4f const faceColors[6]) {
            // Back.
            makeQuadIndirect(minPoint,
                             Vec3f(maxPoint.x, minPoint.y, minPoint.z),
                             Vec3f(maxPoint.x, maxPoint.y, minPoint.z),
                             Vec3f(minPoint.x, maxPoint.y, minPoint.z),
                             faceColors[0], uv, uvBounds, texSize);

            // Front.
            makeQuadIndirect(Vec3f(minPoint.x, minPoint.y, maxPoint.z),
                             Vec3f(maxPoint.x, minPoint.y, maxPoint.z),
                             maxPoint,
                             Vec3f(minPoint.x, maxPoint.y, maxPoint.z),
                             faceColors[1], uv, uvBounds, texSize);

            // Left.
            makeQuadIndirect(Vec3f(minPoint.x, minPoint.y, maxPoint.z),
                             minPoint,
                             Vec3f(minPoint.x, maxPoint.y, minPoint.z),
                             Vec3f(minPoint.x, maxPoint.y, maxPoint.z),
                             faceColors[2], uv, uvBounds, texSize);

            // Right.
            makeQuadIndirect(Vec3f(maxPoint.x, minPoint.y, minPoint.z),
                             Vec3f(maxPoint.x, minPoint.y, maxPoint.z),
                             maxPoint,
                             Vec3f(maxPoint.x, maxPoint.y, minPoint.z),
                             faceColors[3], uv, uvBounds, texSize);

            // Floor.
            makeQuadIndirect(Vec3f(minPoint.x, maxPoint.y, minPoint.z),
                             Vec3f(maxPoint.x, maxPoint.y, minPoint.z),
                             maxPoint,
                             Vec3f(minPoint.x, maxPoint.y, maxPoint.z),
                             faceColors[4], uv, uvBounds, texSize);

            // Ceiling.
            makeQuadIndirect(Vec3f(minPoint.x, minPoint.y, maxPoint.z),
                             Vec3f(maxPoint.x, minPoint.y, maxPoint.z),
                             Vec3f(maxPoint.x, minPoint.y, minPoint.z),
                             minPoint,
                             faceColors[5], uv, uvBounds, texSize);

            return *this;
        }
        Vertices &makeRing(const Vec2f &center, float outerRadius, float innerRadius,
                           int divisions, const Vec4f &color, const Rectanglef &uv,
                           float innerTexRadius = -1) {
            if (innerTexRadius < 0) innerTexRadius = innerRadius / outerRadius;
            Vertices ring;
            VertexType v;
            v.rgba = color;
            for (int i = 0; i <= divisions; ++i) {
                const float ang = 2.f * PIf * (i == divisions? 0 : i) / divisions;
                Vec2f r(cos(ang), sin(ang));
                // Outer.
                v.pos = center + r * outerRadius;
                v.texCoord = uv.middle() + r * .5f * uv.size();
                ring << v;
                // Inner.
                v.pos = center + r * innerRadius;
                v.texCoord = uv.middle() + r * (.5f * innerTexRadius) * uv.size();
                ring << v;
            }
            return *this += ring;
        }
        Vertices &makeRing(const Vec2f &center, float outerRadius, float innerRadius,
                           int divisions, const Vec4f &color, const Vec2f &uv) {
            return makeRing(center, outerRadius, innerRadius, divisions, color, Rectanglef(uv, uv));
        }
        Vertices &makeFlexibleFrame(const Rectanglef &rect, float cornerThickness,
                                    const Vec4f &color, const Rectanglef &uv) {
            const Vec2f uvOff = uv.size() / 2;
            Vertices verts;
            VertexType v;

            v.rgba = color;

            // Top left corner.
            v.pos      = rect.topLeft;
            v.texCoord = uv.topLeft;
            verts << v;

            v.pos      = rect.topLeft + Vec2f(0, cornerThickness);
            v.texCoord = uv.topLeft + Vec2f(0, uvOff.y);
            verts << v;

            v.pos      = rect.topLeft + Vec2f(cornerThickness, 0);
            v.texCoord = uv.topLeft + Vec2f(uvOff.x, 0);
            verts << v;

            v.pos      = rect.topLeft + Vec2f(cornerThickness, cornerThickness);
            v.texCoord = uv.topLeft + uvOff;
            verts << v;

            // Top right corner.
            v.pos      = rect.topRight() + Vec2f(-cornerThickness, 0);
            v.texCoord = uv.topRight() + Vec2f(-uvOff.x, 0);
            verts << v;

            v.pos      = rect.topRight() + Vec2f(-cornerThickness, cornerThickness);
            v.texCoord = uv.topRight() + Vec2f(-uvOff.x, uvOff.y);
            verts << v;

            v.pos      = rect.topRight();
            v.texCoord = uv.topRight();
            verts << v;

            v.pos      = rect.topRight() + Vec2f(0, cornerThickness);
            v.texCoord = uv.topRight() + Vec2f(0, uvOff.y);
            verts << v;

            // Discontinuity.
            verts << v;
            verts << v;

            v.pos      = rect.topRight() + Vec2f(-cornerThickness, cornerThickness);
            v.texCoord = uv.topRight() + Vec2f(-uvOff.x, uvOff.y);
            verts << v;

            // Bottom right corner.
            v.pos      = rect.bottomRight + Vec2f(0, -cornerThickness);
            v.texCoord = uv.bottomRight + Vec2f(0, -uvOff.y);
            verts << v;

            v.pos      = rect.bottomRight + Vec2f(-cornerThickness, -cornerThickness);
            v.texCoord = uv.bottomRight + Vec2f(-uvOff.x, -uvOff.y);
            verts << v;

            v.pos      = rect.bottomRight;
            v.texCoord = uv.bottomRight;
            verts << v;

            v.pos      = rect.bottomRight + Vec2f(-cornerThickness, 0);
            v.texCoord = uv.bottomRight + Vec2f(-uvOff.x, 0);
            verts << v;

            // Discontinuity.
            verts << v;
            verts << v;

            v.pos      = rect.bottomRight + Vec2f(-cornerThickness, -cornerThickness);
            v.texCoord = uv.bottomRight + Vec2f(-uvOff.x, -uvOff.y);
            verts << v;

            // Bottom left corner.
            v.pos      = rect.bottomLeft() + Vec2f(cornerThickness, 0);
            v.texCoord = uv.bottomLeft() + Vec2f(uvOff.x, 0);
            verts << v;

            v.pos      = rect.bottomLeft() + Vec2f(cornerThickness, -cornerThickness);
            v.texCoord = uv.bottomLeft() + Vec2f(uvOff.x, -uvOff.y);
            verts << v;

            v.pos      = rect.bottomLeft();
            v.texCoord = uv.bottomLeft();
            verts << v;

            v.pos      = rect.bottomLeft() + Vec2f(0, -cornerThickness);
            v.texCoord = uv.bottomLeft() + Vec2f(0, -uvOff.y);
            verts << v;

            // Discontinuity.
            verts << v;
            verts << v;

            // Closing the loop.
            v.pos      = rect.bottomLeft() + Vec2f(cornerThickness, -cornerThickness);
            v.texCoord = uv.bottomLeft() + Vec2f(uvOff.x, -uvOff.y);
            verts << v;

            v.pos      = rect.topLeft + Vec2f(0, cornerThickness);
            v.texCoord = uv.topLeft + Vec2f(0, uvOff.y);
            verts << v;

            v.pos      = rect.topLeft + Vec2f(cornerThickness, cornerThickness);
            v.texCoord = uv.topLeft + Vec2f(uvOff.x, uvOff.y);
            verts << v;

            return *this += verts;
        }
    };

    static void concatenate(const Vertices &stripSequence, Vertices &destStrip)
    {
        if (!stripSequence.size()) return;
        if (!destStrip.isEmpty())
        {
            destStrip << destStrip.back();
            destStrip << stripSequence.front();
        }
        destStrip << stripSequence;
    }
};

} // namespace de

#endif // LIBGUI_VERTEXBUILDER_H
