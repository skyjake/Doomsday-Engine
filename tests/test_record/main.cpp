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
#include <de/record.h>
#include <de/reader.h>
#include <de/writer.h>
#include <de/textvalue.h>
#include <de/numbervalue.h>
#include <de/variable.h>
#include <de/json.h>

using namespace de;

int main(int argc, char **argv)
{
    init_Foundation();
    try
    {
        TextApp app(makeList(argc, argv));
        app.initSubsystems(App::DisablePersistentData);

        Record rec;

        LOG_MSG("Empty record:\n") << rec;

        rec.add(new Variable("hello", new TextValue("World!")));
        LOG_MSG("With one variable:\n") << rec;

        rec.add(new Variable("size", new NumberValue(1024)));
        LOG_MSG("With two variables:\n") << rec;

        LOG_MSG("Record as JSON:\n") << composeJSON(rec);

        Record rec2;
        Block b;
        Writer(b) << rec;
        LOG_MSG("Serialized record to ") << b.size() << " bytes.";

        String str;
        for (duint i = 0; i < b.size(); ++i)
        {
            str += String::asText(int(b[i]));
            str += " ";
        }
        LOG_MSG(str);

        Reader(b) >> rec2;
        LOG_MSG("After being deserialized:\n") << rec2;

        Record before;
        before.addSubrecord("subrecord");
        before.subrecord("subrecord").set("value", true);
        DE_ASSERT(before.hasSubrecord("subrecord"));
        LOG_MSG("Before copying:\n") << before;

        Record copied = before;
        DE_ASSERT(copied.hasSubrecord("subrecord"));
        LOG_MSG("Copied:\n") << copied;

        LOG_MSG("...and as JSON:\n") << composeJSON(copied);
    }
    catch (const Error &err)
    {
        err.warnPlainText();
    }
    deinit_Foundation();
    debug("Exiting main()...");
    return 0;
}
