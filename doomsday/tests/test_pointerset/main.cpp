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

#include <de/PointerSet>
#include <QDebug>

using namespace de;

void printSet(PointerSet const &pset)
{
    qDebug() << "[ Size:" << pset.size() << "/" << pset.allocatedSize() << "range:"
             << pset.usedRange().asText();
    for (auto const p : pset)
    {
        qDebug() << "   " << QString("%1").arg(reinterpret_cast<dintptr>(p), 16, 16, QChar('0')).toLatin1().constData();
    }
    qDebug() << "]";
}

int main(int, char **)
{
    try
    {
        PointerSet::Pointer a = reinterpret_cast<PointerSet::Pointer>(0x1000);
        PointerSet::Pointer b = reinterpret_cast<PointerSet::Pointer>(0x2000);
        PointerSet::Pointer c = reinterpret_cast<PointerSet::Pointer>(0x3000);
        PointerSet::Pointer d = reinterpret_cast<PointerSet::Pointer>(0x4000);
        PointerSet::Pointer e = reinterpret_cast<PointerSet::Pointer>(0x5000);

        PointerSet pset;
        qDebug() << "Empty PointerSet:";
        printSet(pset);

        pset.insert(a);
        qDebug() << "Added one pointer:";
        printSet(pset);

        pset.insert(a);
        qDebug() << "'a' is there?" << pset.contains(a);
        qDebug() << "'b' should not be there?" << pset.contains(b);

        qDebug() << "Trying to remove a non-existing pointer.";
        pset.remove(b);
        printSet(pset);

        pset.remove(a);
        qDebug() << "Removed the pointer:";
        printSet(pset);

        qDebug() << "Adding everything:";
        pset.insert(d);
        pset.insert(a);
        pset.insert(c);
        pset.insert(b);
        pset.insert(e);
        printSet(pset);

        qDebug() << "Removing the ends:";
        pset.remove(a);
        pset.remove(e);
        printSet(pset);

        qDebug() << "Removing the middle:";
        pset.remove(c);
        printSet(pset);

        qDebug() << "Adding everything again:";
        pset.insert(d);
        pset.insert(a);
        pset.insert(c);
        pset.insert(b);
        pset.insert(e);
        printSet(pset);

        qDebug() << "Removing everything:";
        pset.remove(d);
        pset.remove(a);
        pset.remove(c);
        pset.remove(b);
        pset.remove(e);
        printSet(pset);

        qDebug() << "Adding one:";
        pset.insert(e);
        printSet(pset);

        qDebug() << "Adding another:";
        pset.insert(a);
        printSet(pset);

        qDebug() << "Removing during iteration:";
        pset.insert(e);
        pset.insert(d);
        pset.insert(c);
        pset.insert(b);
        pset.insert(a);
        pset.setFlags(PointerSet::BeingIterated);
        for (auto i : pset)
        {
            if (i == c)
            {
                qDebug() << "Removing 'c'...";
                pset.remove(i);
            }
            if (i == a)
            {
                qDebug() << "Removing 'a'...";
                pset.remove(i);
            }
            if (i == e)
            {
                qDebug() << "Removing 'e'...";
                pset.remove(i);
            }
            pset.remove(d);
        }
        pset.setFlags(PointerSet::BeingIterated, false);
        printSet(pset);
    }
    catch (Error const &err)
    {
        qWarning() << err.asText() << "\n";
    }

    qDebug() << "Exiting main()...\n";
    return 0;
}
