/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/Vector>
#include <de/Matrix>
#include <de/Writer>
#include <de/Reader>
#include <de/Block>
#include <QDebug>

using namespace de;

int main(int, char **)
{
    try
    {
        Vec2f a(1, 2.5);
        Vec3f b(3, 5, 6);

        Mat3f ma;
        Mat4f mb;
        Mat4d mc;

        // Note: Using QDebug because no de::App (and therefore no log message
        // buffer) is available.

        qDebug() << "Sizeof Vec2f:" << sizeof(a);
        qDebug() << "Sizeof Vec2f.x:" << sizeof(a.x);
        qDebug() << "Sizeof Vec3f:" << sizeof(b);

        qDebug() << "Sizeof Mat3f:" << sizeof(ma);
        qDebug() << "Sizeof Mat4f:" << sizeof(mb);
        qDebug() << "Sizeof Mat4d:" << sizeof(mc);

        qDebug() << "Direct access to members:";
        qDebug() << a.x << a.y << a[0] << a[1];
        qDebug() << b.x << b.y << b.z << b[0] << b[1] << b[2];

        qDebug() << "First operand defines type of result:";

        qDebug() << "Vec2f + Vec3f:" << (a + b).asText();
        qDebug() << "Vec3f + Vec2f:" << (b + a).asText();

        Vec2i c(6, 5);

        // This would downgrade the latter to int; won't do it.
        //qDebug() << "Vec2i + Vec2f (converted to int!): " << (c + a).asText();

        qDebug() << "Vec2i:" << c.asText();
        qDebug() << "Vec2f + Vec2i:" << (a + c).asText();

        a += b;
        b += a;
        qDebug() << "After sum:" ;
        qDebug() << "a:" << a.asText() << "b:" << b.asText();

        qDebug() << "a > b: " << (a > b);
        qDebug() << "b > a: " << (b > a);

        Vec2f s(1, 1);
        Vec3f t(2, 2, 2);
        qDebug() << "s: " << s.asText() << " t:" << t.asText();
        qDebug() << "s > t: " << (s > t);
        qDebug() << "t > s: " << (t > s);
        qDebug() << "s < t: " << (s < t);
        qDebug() << "t < s: " << (t < s);
        t.z = -100;
        qDebug() << "t is now: " << t.asText();
        qDebug() << "s > t: " << (s > t);
        qDebug() << "t > s: " << (t > s);
        qDebug() << "s < t: " << (s < t) << " <- first operand causes conversion to Vector2";
        qDebug() << "t < s: " << (t < s);

        Vec2d u(3.1415926535, 3.33333333333333333333333);
        qDebug() << "u:" << u.asText();
        Block block, block2;
        //Writer writer(block); writer << u;
        Writer(block) << u;

        Writer writer(block2);
        writer << u;

        Vec2d w;
        Reader(block) >> w;

        Vec2d y;
        Reader reader(block2);
        reader >> y;
        qDebug() << "w:" << w.asText();
        qDebug() << "y:" << w.asText();

        qDebug() << "Matrix operations:";

        qDebug() << "Identity" << ma.asText();
        qDebug() << "Identity" << mc.asText();

        qDebug() << "Rotation 45 degrees" << Mat4f::rotate(45).asText();
        qDebug() << "Rotation 90 degrees" << Mat4f::rotate(90).asText();
        qDebug() << "Rotation 45 degrees, X axis"
                 << Mat4f::rotate(45, Vec3f(1, 0, 0)).asText();

        qDebug() << "Translation"
                 << Mat4f::translate(Vec3f(1, 2, 3)).asText();

        qDebug() << "Scale"
                 << Mat4f::scale(Vec3f(1, 2, 3)).asText();

        t = Vec3f(1, 2, 3);
        Mat4f scaleTrans = Mat4f::scaleThenTranslate(Vec3f(10, 10, 10), Vec3f(-5, -5, -5));
        qDebug() << "Scale and translate with"
                 << scaleTrans.asText() << "result:" << (scaleTrans * t).asText();

        qDebug() << "Seperate matrices (translate * scale):"
                 << (Mat4f::translate(Vec3f(-5, -5, -5)) * Mat4f::scale(10) * t).asText();

        qDebug() << "Seperate matrices (scale * translate):"
                 << (Mat4f::scale(10) * Mat4f::translate(Vec3f(-5, -5, -5)) * t).asText();

        qDebug() << "Inverse" << scaleTrans.inverse().asText();

        t = scaleTrans * t;
        qDebug() << "Result" << (scaleTrans.inverse() * t).asText();

        qDebug() << "X axis rotated to Z" <<
                    (Mat4d::rotate(90, Vec3d(0, -1, 0)) * Vec3d(1, 0, 0)).asText();

        qDebug() << "Look at (10,10,10) from (1,1,1)"
                 << Mat4f::lookAt(Vec3f(10, 10, 10), Vec3f(1, 1, 1), Vec3f(0, 0, 1)).asText();

        qDebug() << "Cross product" << Vec3f(1, 0, 0).cross(Vec3f(0, 1, 0)).asText();
    }
    catch (Error const &err)
    {
        qWarning() << err.asText() << "\n";
    }

    qDebug() << "Exiting main()...\n";
    return 0;
}
