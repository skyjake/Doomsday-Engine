/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/bitfield.h>
#include <iostream>

using namespace de;

int main(int, char **)
{
    init_Foundation();
    using namespace std;
    try
    {
        BitField::Elements elems;
        elems.add(1, 1);

        BitField pack(elems);
        pack.set(1, duint(1));
        cout << pack.asText() << endl;

        elems.clear();
        elems.add(1, 1);
        elems.add(2, 1);

        pack.setElements(elems);
        pack.set(2, true);
        cout << pack.asText() << endl;
        pack.set(1, true);
        cout << pack.asText() << endl;

        elems.add(3, 3);
        pack.set(1, false);
        cout << pack.asText() << endl;
        pack.set(3, 6u);
        cout << pack.asText() << endl;

        elems.add(10, 8);
        pack.set(10, 149u);
        cout << pack.asText() << endl;

        cout << "Field 1: " << pack[1] << " Field 2: " << pack[2] << " Field 3: " << pack[3]
             << " Field 10: " << pack[10] << endl;

        DE_ASSERT(pack[10] == 149);

        BitField pack2 = pack;
        cout << "Copied: " << pack2.asText() << endl;

        cout << "Equal: " << (pack2 == pack? "true" : "false") << endl;
        cout << "Delta: " << pack.delta(pack2) << endl;

        pack2.set(3, 3u);
        cout << "Modified: " << pack2.asText() << endl;
        cout << "Equal: " << (pack2 == pack? "true" : "false") << endl;
        cout << "Delta: " << pack.delta(pack2) << endl;

        pack2.set(3, 6u);
        pack2.set(10, 128u);
        cout << "Modified: " << pack2.asText() << endl;
        cout << "Field 10: " << pack2[10] << endl;
        cout << "Equal: " << (pack2 == pack? "true" : "false") << endl;
        cout << "Delta: " << pack.delta(pack2) << endl;

        pack2.set(1, true);
        cout << "Modified: " << pack2.asText() << endl;
        cout << "Equal: " << (pack2 == pack? "true" : "false") << endl;
        cout << "Delta: " << pack.delta(pack2) << endl;
        cout << "Delta (reverse): " << pack2.delta(pack) << endl;
    }
    catch (const Error &err)
    {
        err.warnPlainText();
    }
    deinit_Foundation();
    debug("Exiting main()...");
    return 0;
}
