/**
 * @file main.cpp
 *
 * StringPool unit tests. @ingroup tests
 *
 * @author Copyright &copy; 2010-2013 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/stringpool.h>
#include <de/reader.h>
#include <de/writer.h>
#include <iostream>

using namespace de;

int main(int, char **)
{
    init_Foundation();
    try
    {
        StringPool p;

        String s = String("Hello");
        DE_ASSERT(!p.isInterned(s));
        DE_ASSERT(p.empty());

        // First string.
        p.intern(s);
        DE_ASSERT(p.isInterned(s) == 1);

        // Re-insertion.
        DE_ASSERT(p.intern(s) == 1);

        // Case insensitivity.
        s = String("heLLO");
        DE_ASSERT(p.intern(s) == 1);

        // Another string.
        s = String("abc");
        const String &is = p.internAndRetrieve(s);
        DE_ASSERT(!is.compare(s));
        DE_UNUSED(is);

        String s2 = String("ABC");
        const String &is2 = p.internAndRetrieve(s2);
        DE_ASSERT(!is2.compare(s));
        DE_UNUSED(is2);

        DE_ASSERT(p.intern(is2) == 2);

        DE_ASSERT(p.size() == 2);
        //p.print();

        DE_ASSERT(!p.empty());

        p.setUserValue(1, 1234);
        DE_ASSERT(p.userValue(1) == 1234);

        DE_ASSERT(p.userValue(2) == 0);

        s = String("HELLO");
        p.remove(s);
        DE_ASSERT(!p.isInterned(s));
        DE_ASSERT(p.size() == 1);
        DE_ASSERT(!p.string(2).compare("abc"));

        s = String("Third!");
        DE_ASSERT(p.intern(s) == 1);
        DE_ASSERT(p.size() == 2);

        s = String("FOUR");
        p.intern(s);
        p.removeById(1); // "Third!"

        // Serialize.
        Block b;
        Writer(b) << p;
        std::cout << "Serialized StringPool to " << b.size() << " bytes." << std::endl;

        // Deserialize.
        StringPool p2;
        Reader(b) >> p2;
        //p2.print();
        DE_ASSERT(p2.size() == 2);
        DE_ASSERT(!p2.string(2).compare("abc"));
        DE_ASSERT(!p2.string(3).compare("four"));
        s = String("hello again");
        DE_ASSERT(p2.intern(s) == 1);

        p.clear();
        DE_ASSERT(p.empty());
    }
    catch (const Error &err)
    {
        err.warnPlainText();
    }
    deinit_Foundation();
    debug("Exiting main()...");
    return 0;
}
