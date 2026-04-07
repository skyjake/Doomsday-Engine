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

#include <de/vector.h>
#include <de/matrix.h>
#include <de/writer.h>
#include <de/reader.h>
#include <de/block.h>
#include <iostream>

using namespace de;

int main(int, char **)
{
    init_Foundation();
    using namespace std;
    try
    {
        Vec2f a(1, 2.5);
        Vec3f b(3, 5, 6);

        Mat3f ma;
        Mat4f mb;
        Mat4d mc;

        // Note: Using QDebug because no de::App (and therefore no log message
        // buffer) is available.

        cout << "Sizeof Vec2f: " << sizeof(a) << endl;
        cout << "Sizeof Vec2f.x: " << sizeof(a.x) << endl;
        cout << "Sizeof Vec3f: " << sizeof(b) << endl;

        cout << "Sizeof Mat3f: " << sizeof(ma) << endl;
        cout << "Sizeof Mat4f: " << sizeof(mb) << endl;
        cout << "Sizeof Mat4d: " << sizeof(mc) << endl;

        cout << "Direct access to members:" << endl;
        cout << stringf("%f %f %f %f", a.x, a.y, a[0], a[1]) << endl;
        cout << stringf("%f %f %f %f %f %f", b.x, b.y, b.z, b[0], b[1], b[2]) << endl;

        cout << "First operand defines type of result:" << endl;

        cout << "Vec2f + Vec3f: " << (a + b).asText() << endl;
        cout << "Vec3f + Vec2f: " << (b + a).asText() << endl;

        Vec2i c(6, 5);

        // This would downgrade the latter to int; won't do it.
        //cout << "Vec2i + Vec2f (converted to int!): " << (c + a).asText();

        cout << "Vec2i: " << c.asText() << endl;
        cout << "Vec2f + Vec2i: " << (a + c).asText() << endl;

        a += b;
        b += a;
        cout << "After sum:" << endl;
        cout << "a: " << a.asText() << " b: " << b.asText() << endl;

        cout << "a > b: " << (a > b) << endl;
        cout << "b > a: " << (b > a) << endl;

        Vec2f s(1, 1);
        Vec3f t(2, 2, 2);
        cout << "s: " << s.asText() << " t: " << t.asText() << endl;
        cout << "s > t: " << (s > t) << endl;
        cout << "t > s: " << (t > s) << endl;
        cout << "s < t: " << (s < t) << endl;
        cout << "t < s: " << (t < s) << endl;
        t.z = -100;
        cout << "t is now: " << t.asText() << endl;
        cout << "s > t: " << (s > t) << endl;
        cout << "t > s: " << (t > s) << endl;
        cout << "s < t: " << (s < t) << " <- first operand causes conversion to Vector2" << endl;
        cout << "t < s: " << (t < s) << endl;

        Vec2d u(3.1415926535, 3.33333333333333333333333);
        cout << "u: " << u.asText() << endl;
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
        cout << "w: " << w.asText() << endl;
        cout << "y: " << w.asText() << endl;

        cout << "Matrix operations:" << endl;

        cout << "Identity " << ma.asText() << endl;
        cout << "Identity " << mc.asText() << endl;

        cout << "Rotation 45 degrees " << Mat4f::rotate(45).asText() << endl;
        cout << "Rotation 90 degrees " << Mat4f::rotate(90).asText() << endl;
        cout << "Rotation 45 degrees, X axis "
                 << Mat4f::rotate(45, Vec3f(1, 0, 0)).asText() << endl;

        cout << "Translation "
                 << Mat4f::translate(Vec3f(1, 2, 3)).asText() << endl;

        cout << "Scale "
                 << Mat4f::scale(Vec3f(1, 2, 3)).asText() << endl;

        t = Vec3f(1, 2, 3);
        Mat4f scaleTrans = Mat4f::scaleThenTranslate(Vec3f(10, 10, 10), Vec3f(-5, -5, -5));
        cout << "Scale and translate with "
                 << scaleTrans.asText() << "result: " << (scaleTrans * t).asText() << endl;

        cout << "Seperate matrices (translate * scale): "
                 << (Mat4f::translate(Vec3f(-5, -5, -5)) * Mat4f::scale(10) * t).asText() << endl;

        cout << "Seperate matrices (scale * translate): "
                 << (Mat4f::scale(10) * Mat4f::translate(Vec3f(-5, -5, -5)) * t).asText() << endl;

        cout << "Inverse " << scaleTrans.inverse().asText() << endl;

        t = scaleTrans * t;
        cout << "Result " << (scaleTrans.inverse() * t).asText() << endl;

        cout << "X axis rotated to Z " <<
                    (Mat4d::rotate(90, Vec3d(0, -1, 0)) * Vec3d(1, 0, 0)).asText() << endl;

        cout << "Look at (10,10,10) from (1,1,1) "
                 << Mat4f::lookAt(Vec3f(10, 10, 10), Vec3f(1), Vec3f(0, 0, 1)).asText() << endl;

        cout << "Cross product " << Vec3f(1, 0, 0).cross(Vec3f(0, 1, 0)).asText() << endl;
    }
    catch (const Error &err)
    {
        err.warnPlainText();
    }
    deinit_Foundation();
    debug("Exiting main()...");
    return 0;
}
