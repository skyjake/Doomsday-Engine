/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/Writer>
#include <de/Reader>
#include <de/Block>
#include <QDebug>

using namespace de;

int main(int, char **)
{
    try
    {
        Vector2f a(1, 2.5);
        Vector3f b(3, 5, 6);

        // Note: Using QDebug because no de::App (and therefore no log message
        // buffer) is available.

        qDebug() << "Sizeof Vector2f:" << sizeof(a);
        qDebug() << "Sizeof Vector2f.x:" << sizeof(a.x);
        qDebug() << "Sizeof Vector3f:" << sizeof(b);

        qDebug() << "Direct access to members:";
        qDebug() << a.x << a.y;
        qDebug() << b.x << b.y << b.z;

        qDebug() << "First operand defines type of result:";

        qDebug() << "Vector2f + Vector3f:" << (a + b).asText();
        qDebug() << "Vector3f + Vector2f:" << (b + a).asText();

        Vector2i c(6, 5);

        // This would downgrade the latter to int; won't do it.
        //qDebug() << "Vector2i + Vector2f (converted to int!): " << (c + a).asText();
        
        qDebug() << "Vector2i:" << c.asText();
        qDebug() << "Vector2f + Vector2i:" << (a + c).asText();

        a += b;
        b += a;
        qDebug() << "After sum:" ;
        qDebug() << "a:" << a.asText() << "b:" << b.asText();
        
        qDebug() << "a > b: " << (a > b);
        qDebug() << "b > a: " << (b > a);
        
        Vector2f s(1, 1);
        Vector3f t(2, 2, 2);
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

        Vector2d u(3.1415926535, 3.33333333333333333333333);
        qDebug() << "u:" << u.asText();
        Block block, block2;
        //Writer writer(block); writer << u;
        Writer(block) << u;

        Writer writer(block2);
        writer << u;

        Vector2d w;
        Reader(block) >> w;

        Vector2d y;
        Reader reader(block2);
        reader >> y;
        qDebug() << "w:" << w.asText();
        qDebug() << "y:" << w.asText();
    }
    catch(Error const &err)
    {
        qWarning() << err.asText() << "\n";
    }

    qDebug() << "Exiting main()...\n";
    return 0;        
}
