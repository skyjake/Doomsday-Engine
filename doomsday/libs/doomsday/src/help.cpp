/** @file help.cpp  Runtime help text strings.
 *
 * @ingroup base
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/help.h"
#include "doomsday/console/cmd.h"

#include <de/app.h>
#include <de/packageloader.h>
#include <de/logbuffer.h>
#include <de/reader.h>

#include <de/keymap.h>

using namespace de;

typedef KeyMap<int, String> StringsByType; // HST_* type => string
typedef KeyMap<String, StringsByType> HelpStrings; // id => typed strings

static HelpStrings helps;

/**
 * Parses the given file looking for help strings. The contents of the file are
 * expected to use UTF-8 encoding.
 *
 * @param file  File containing help strings.
 */
void Help_ReadStrings(const File &file)
{
    LOG_RES_VERBOSE("Reading help strings from ") << file.description();

    de::Reader reader(file);
    StringsByType *node = nullptr;

    while (!reader.atEnd())
    {
        String line = reader.readLine().strip();

        // Comments and empty lines are ignored.
        if (line.isEmpty() || line.beginsWith("#"))
            continue;

        // A new node?
        if (line.beginsWith("["))
        {
            auto end = line.indexOf(']');
            String id = line.substr(BytePos(1), end > 0 ? (end.index - 1) : String::npos)
                    .strip().lower();
            helps.insert(id, StringsByType());
            node = &helps.find(id)->second;

            LOG_TRACE_DEBUGONLY("Help node '%s'", id);
        }
        else if (node && line.contains('=')) // It must be a key?
        {
            int type = HST_DESCRIPTION;
            if (line.beginsWith("cv", CaseInsensitive))
            {
                type = HST_CONSOLE_VARIABLE;
            }
            else if (line.beginsWith("def", CaseInsensitive))
            {
                type = HST_DEFAULT_VALUE;
            }
            else if (line.beginsWith("inf", CaseInsensitive))
            {
                type = HST_INFO;
            }

            // Strip the beginning.
            line = line.substr(line.indexOf('=') + 1).strip();

            // The full text is collected here.
            String text;

            // The value may be split over multiple lines.
            while (!line.isEmpty())
            {
                // Process the current line.
                bool escape = false;
                for (Char ch : line)
                {
                    if (ch == Char('\\'))
                    {
                        escape = true;
                    }
                    else if (escape)
                    {
                        if     (ch == Char('n') ) text += "\n";
                        else if (ch == Char('b') ) text += "\b";
                        else if (ch == Char('\\')) text += "\\";
                        escape = false;
                    }
                    else
                    {
                        text += ch;
                    }
                }

                // This part has been processed.
                line.clear();

                if (escape)
                {
                    // Line ended with a backslash; read the next line.
                    line = reader.readLine().strip();
                }
            }

            node->insert(type, text);

            LOG_TRACE_DEBUGONLY("Help string (type %i): \"%s\"", type << text);
        }
    }
}

HelpId DH_Find(const char *id)
{
    // The identifiers are case insensitive.
    HelpStrings::const_iterator found = helps.find(String(id).lower());
    if (found != helps.end())
    {
        return &found->second;
    }
    return nullptr;
}

const char *DH_GetString(HelpId found, int type)
{
    if (!found) return nullptr;
    if (type < 0 || type > NUM_HELPSTRING_TYPES) return nullptr;

    const StringsByType *hs = reinterpret_cast<const StringsByType *>(found);

    StringsByType::const_iterator i = hs->find(type);
    if (i != hs->end())
    {
        return Str_Text(AutoStr_FromTextStd(i->second.c_str()));
    }
    return nullptr;
}

void DD_InitHelp()
{
    LOG_AS("DD_InitHelp");
    try
    {
        Help_ReadStrings(App::packageLoader().package("net.dengine.base")
                         .root().locate<File>("helpstrings.txt"));
    }
    catch (const Error &er)
    {
        LOG_RES_WARNING("") << er.asText();
    }
}

void DD_ShutdownHelp()
{
    helps.clear();
}

D_CMD(LoadHelp)
{
    DE_UNUSED(src, argc, argv);

    DD_ShutdownHelp();
    DD_InitHelp();
    return true;
}

void DH_Register()
{
    C_CMD("loadhelp", "",   LoadHelp);
}
