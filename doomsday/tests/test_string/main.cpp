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

#include <de/textapp.h>
#include <de/math.h>
#include <de/webrequest.h>

using namespace de;

int main(int argc, char **argv)
{
    init_Foundation();
    try
    {
        TextApp app(makeList(argc, argv));
        app.initSubsystems();

        // Iterators.
        {
            const String str = u8"H★l—lo Wörld";
            for (Char ch : str)
            {
                debug("Char: %x %lc", unsigned(ch), ch.unicode());
            }
            for (auto i = str.begin(); i != str.end(); ++i)
            {
                debug("Char %u: %x %lc", i.pos().index, unsigned(*i), (*i).unicode());
            }
            for (auto i = str.rbegin(); i != str.rend(); ++i)
            {
                debug("Char %u: %x %lc", i.pos().index, unsigned(*i), (*i).unicode());
            }
        }

        // URI splitting.
        {
            const String uri = "https://dengine.net:8080/some/page.php?query&arg#first-section";
            String c[5];
            WebRequest::splitUriComponents(uri, &c[0], &c[1], &c[2], &c[3], &c[4]);
            for (const auto &i : c)
            {
                LOG_MSG("URI component: %s") << i;
            }
            LOG_MSG("Host name: %s") << WebRequest::hostNameFromUri(uri);
        }

        LOG_MSG("Escaped %%: arg %i") << 1;
        LOG_MSG("Escaped %%: arg %%%i%%") << 1;
        //LOG_MSG("Error: %") << 1; // incomplete formatting
        //LOG_MSG("Error: %i %i") << 1; // ran out of arguments
        LOG_MSG("More args than formats: %i appended:") << 1 << 2 << 3 << "hello";

        LOG_MSG("String: '%s'") << "Hello World";
        LOG_MSG(" Min width 8:  '%8s'") << "Hello World";
        LOG_MSG(" Max width .8: '%.8s'") << "Hello World";
        LOG_MSG(" Left align:   '%-.8s'") << "Hello World";
        LOG_MSG("String: '%s'") << "Hello";
        LOG_MSG(" Min width 8:  '%8s'") << "Hello";
        LOG_MSG(" Max width .8: '%.8s'") << "Hello";
        LOG_MSG(" Left align:   '%-8s'") << "Hello";

        LOG_MSG("Integer (64-bit signed): %i") << uint64_t(0x1000000000LL);
        LOG_MSG("Integer (64-bit signed): %d") << uint64_t(0x1000000000LL);
        LOG_MSG("Integer (64-bit unsigned): %u") << uint64_t(0x123456789abcLL);
        LOG_MSG("Boolean: %b %b") << true << false;
        LOG_MSG("16-bit Unicode character: %c") << 0x44;
        LOG_MSG("Hexadecimal (64-bit): %x") << uint64_t(0x123456789abcLL);
        LOG_MSG("Hexadecimal (64-bit): %X") << uint64_t(0x123456789abcLL);
        LOG_MSG("Pointer: %p") << &app;
        LOG_MSG("Double precision floating point: %f") << PI;
        LOG_MSG("Decimal places .4: %.4f") << PI;
        LOG_MSG("Decimal places .10: %.10f") << PI;
    }
    catch (const Error &err)
    {
        err.warnPlainText();
    }
    deinit_Foundation();
    debug("Exiting main()...");
    return 0;
}
