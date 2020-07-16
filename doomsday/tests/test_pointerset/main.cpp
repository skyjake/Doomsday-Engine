/**
 * @file main.cpp
 *
 * PointerSet tests. @ingroup tests
 *
 * @author Copyright &copy; 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <de/pointerset.h>
#include <iostream>

using namespace de;
using namespace std;

void printSet(const PointerSet &pset)
{
    cout << "[ Size: " << pset.size() << " / " << pset.allocatedSize() << " range: "
             << pset.usedRange().asText() << " flags: " << stringf("%x", pset.flags()) << endl;
    for (auto const p : pset)
    {
        cout << "   " << stringf("%p", p) << endl;
    }
    cout << "]" << endl;
}

int main(int, char **)
{
    init_Foundation();
    try
    {
        PointerSet::Pointer a = reinterpret_cast<PointerSet::Pointer>(0x1000);
        PointerSet::Pointer b = reinterpret_cast<PointerSet::Pointer>(0x2000);
        PointerSet::Pointer c = reinterpret_cast<PointerSet::Pointer>(0x3000);
        PointerSet::Pointer d = reinterpret_cast<PointerSet::Pointer>(0x4000);
        PointerSet::Pointer e = reinterpret_cast<PointerSet::Pointer>(0x5000);
//        PointerSet::Pointer f = reinterpret_cast<PointerSet::Pointer>(0x6000);
//        PointerSet::Pointer g = reinterpret_cast<PointerSet::Pointer>(0x7000);
//        PointerSet::Pointer h = reinterpret_cast<PointerSet::Pointer>(0x8000);

        PointerSet pset;
        cout << "Empty PointerSet: " << endl;
        printSet(pset);

        pset.insert(a);
        cout << "Added one pointer: " << endl;
        printSet(pset);

        pset.insert(a);
        cout << "'a' is there? " << pset.contains(a) << endl;
        cout << "'b' should not be there? " << pset.contains(b) << endl;

        cout << "Trying to remove a non-existing pointer." << endl;
        pset.remove(b);
        printSet(pset);

        pset.remove(a);
        cout << "Removed the pointer:" << endl;
        printSet(pset);

        cout << "Adding again:" << endl;
        pset.insert(b);
        pset.insert(c);
        printSet(pset);

        cout << "Adding everything:" << endl;
        pset.insert(d);
        pset.insert(a);
        pset.insert(c);
        pset.insert(b);
        pset.insert(e);
        printSet(pset);

        cout << "Removing the ends:" << endl;
        pset.remove(a);
        pset.remove(e);
        printSet(pset);

        cout << "Removing the middle:" << endl;
        pset.remove(c);
        printSet(pset);

        cout << "Adding everything again:" << endl;
        pset.insert(e);
        pset.insert(d);
        pset.insert(c);
        pset.insert(b);
        pset.insert(a);
        printSet(pset);
        
        cout << "Taking one:" << endl;
        pset.take();
        printSet(pset);

        cout << "Removing everything:" << endl;
        pset.remove(d);
        pset.remove(a);
        pset.remove(c);
        pset.remove(b);
        pset.remove(e);
        printSet(pset);

        cout << "Adding one:" << endl;
        pset.insert(e);
        printSet(pset);

        cout << "Adding another:" << endl;
        pset.insert(a);
        printSet(pset);

        cout << "Removing during iteration:" << endl;
        pset.insert(e);
        pset.insert(d);
        pset.insert(c);
        pset.insert(b);
        pset.insert(a);
        pset.setBeingIterated(true);
        printSet(pset);
        for (auto i : pset)
        {
            if (i == c)
            {
                cout << "Removing 'c'..." << endl;
                pset.remove(i);
            }
            if (i == a)
            {
                cout << "Removing 'a'..." << endl;
                pset.remove(i);
            }
            if (i == e)
            {
                cout << "Removing 'e'..." << endl;
                pset.remove(i);
            }
            pset.remove(d);
        }
        pset.setBeingIterated(false);
        printSet(pset);

        cout << "Assignment:" << endl;
        pset = PointerSet();
        printSet(pset);
    }
    catch (const Error &err)
    {
        err.warnPlainText();
    }
    deinit_Foundation();
    debug("Exiting main()...");
    return 0;
}
