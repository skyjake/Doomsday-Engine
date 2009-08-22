/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <dengmain.h>
#include <de/types.h>
#include "../testapp.h"

#include <iostream>

using namespace de;

int deng_Main(int argc, char** argv)
{
    using std::cout;
    using std::endl;
    
    try
    {
        Vector2f a(1, 2.5);
        Vector3f b(3, 5, 6);

        cout << "Sizeof Vector2f: " << sizeof(a) << endl;
        cout << "Sizeof Vector3f: " << sizeof(b) << endl;

        cout << "Direct access to members:" << endl;
        cout << a.x << ", " << a.y << endl;
        cout << b.x << ", " << b.y << ", " << b.z << endl;        

        cout << "First operand defines type of result:" << endl;
        
        cout << "Vector2f + Vector3f: " << a + b << endl;
        cout << "Vector3f + Vector2f: " << b + a << endl;

        // This would downgrade the latter to int; won't do it.
        //cout << "Vector2i + Vector2f: " << (c + a).asText() << endl;
        
        Vector2i c(6, 5);
        cout << "Vector2i: " << c << endl;
        cout << "Vector2f + Vector2i: " << a + c << endl;

        a += b;
        b += a;
        cout << "After sum:" << endl;
        cout << a.asText() << " " << b.asText() << endl;
        
        cout << "a > b: " << (a > b) << endl;
        cout << "b > a: " << (b > a) << endl;
        
        Vector2f s(1, 1);
        Vector3f t(2, 2, 2);
        cout << "s: " << s << " t:" << t << endl;
        cout << "s > t: " << (s > t) << endl;
        cout << "t > s: " << (t > s) << endl;
        cout << "s < t: " << (s < t) << endl;
        cout << "t < s: " << (t < s) << endl;
        t.z = -100;
        cout << "t is now: " << t << endl;
        cout << "s > t: " << (s > t) << endl;
        cout << "t > s: " << (t > s) << endl;
        cout << "s < t: " << (s < t) << " <- first operand causes 2-component conversion" << endl;
        cout << "t < s: " << (t < s) << endl;
    }
    catch(const Error& err)
    {
        std::cout << err.asText() << "\n";
    }

    std::cout << "Exiting deng_Main()...\n";
    return 0;        
}
