/**
 * @file main.cpp
 *
 * StringPool unit tests. @ingroup tests
 *
 * @author Copyright &copy; 2010-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/StringPool>
#include <de/Reader>
#include <de/Writer>
#include <QDebug>

using namespace de;

int main(int, char **)
{
    try
    {
        StringPool p;

        String s = String("Hello");
        DENG2_ASSERT(!p.isInterned(s));
        DENG2_ASSERT(p.empty());

        // First string.
        p.intern(s);
        DENG2_ASSERT(p.isInterned(s) == 1);

        // Re-insertion.
        DENG2_ASSERT(p.intern(s) == 1);

        // Case insensitivity.
        s = String("heLLO");
        DENG2_ASSERT(p.intern(s) == 1);

        // Another string.
        s = String("abc");
        String const &is = p.internAndRetrieve(s);
        DENG2_ASSERT(!is.compare(s));

        String s2 = String("ABC");
        String const &is2 = p.internAndRetrieve(s2);
        DENG2_ASSERT(!is2.compare(s));

        DENG2_ASSERT(p.intern(is2) == 2);

        DENG2_ASSERT(p.size() == 2);
        //p.print();

        DENG2_ASSERT(!p.empty());

        p.setUserValue(1, 1234);
        DENG2_ASSERT(p.userValue(1) == 1234);

        DENG2_ASSERT(p.userValue(2) == 0);

        s = String("HELLO");
        p.remove(s);
        DENG2_ASSERT(!p.isInterned(s));
        DENG2_ASSERT(p.size() == 1);
        DENG2_ASSERT(!p.string(2).compare("abc"));

        s = String("Third!");
        DENG2_ASSERT(p.intern(s) == 1);
        DENG2_ASSERT(p.size() == 2);

        s = String("FOUR");
        p.intern(s);
        p.removeById(1); // "Third!"

        // Serialize.
        Block b;
        Writer(b) << p;
        qDebug() << "Serialized stringpool to" << b.size() << "bytes.";

        // Deserialize.
        StringPool p2;
        Reader(b) >> p2;
        //p2.print();
        DENG2_ASSERT(p2.size() == 2);
        DENG2_ASSERT(!p2.string(2).compare("abc"));
        DENG2_ASSERT(!p2.string(3).compare("FOUR"));
        s = String("hello again");
        DENG2_ASSERT(p2.intern(s) == 1);

        p.clear();
        DENG2_ASSERT(p.empty());
    }
    catch(Error const &err)
    {
        qWarning() << err.asText() << "\n";
    }

    qDebug() << "Exiting main()...\n";
    return 0;
}
