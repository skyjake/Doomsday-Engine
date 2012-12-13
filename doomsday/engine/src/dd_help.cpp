/** @file dd_help.cpp Runtime help text strings.
 * @ingroup base
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_console.h"

#include <de/FS>
#include <de/App>
#include <de/Log>
#include <de/Reader>

#include <QMap>
#include <QStringBuilder>

D_CMD(LoadHelp);

using namespace de;

typedef QMap<int, String> StringsByType; // HST_* type => string
typedef QMap<String, StringsByType> HelpStrings; // id => typed strings

static HelpStrings helps;

void DH_Register(void)
{
    C_CMD("loadhelp", "", LoadHelp);
}

/**
 * Parses the given file looking for help strings. The contents of the file are
 * expected to use UTF-8 encoding.
 *
 * @param file  File containing help strings.
 */
static void readStrings(File const &file)
{
    LOG_VERBOSE("Reading help strings from ") << file.description();

    de::Reader reader(file);
    StringsByType *node = 0;

    while(!reader.atEnd())
    {
        String line = reader.readLine().trimmed();

        // Comments and empty lines are ignored.
        if(line.isEmpty() || line.startsWith("#"))
            continue;

        // A new node?
        if(line.startsWith("["))
        {
            int end = line.indexOf(']');
            String id = line.mid(1, end > 0? end - 1 : -1).trimmed();
            node = &helps.insert(id, StringsByType()).value();

            LOG_DEV_TRACE("Help node '%s'", id);
        }
        else if(node && line.contains('=')) // It must be a key?
        {
            int type = HST_DESCRIPTION;
            if(line.startsWith("cv", Qt::CaseInsensitive))
            {
                type = HST_CONSOLE_VARIABLE;
            }
            else if(line.startsWith("def", Qt::CaseInsensitive))
            {
                type = HST_DEFAULT_VALUE;
            }
            else if(line.startsWith("inf", Qt::CaseInsensitive))
            {
                type = HST_INFO;
            }

            // Strip the beginning.
            line = line.mid(line.indexOf('=') + 1).trimmed();

            // The full text is collected here.
            QString text;

            // The value may be split over multiple lines.
            while(!line.isEmpty())
            {
                // Process the current line.
                bool escape = false;
                foreach(QChar ch, line)
                {
                    if(ch == QChar('\\'))
                    {
                        escape = true;
                    }
                    else if(escape)
                    {
                        if     (ch == QChar('n') ) text = text % "\n";
                        else if(ch == QChar('b') ) text = text % "\b";
                        else if(ch == QChar('\\')) text = text % "\\";
                        escape = false;
                    }
                    else
                    {
                        text = text % ch;
                    }
                }

                // This part has been processed.
                line.clear();

                if(escape)
                {
                    // Line ended with a backslash; read the next line.
                    line = reader.readLine().trimmed();
                }
            }

            node->insert(type, text);

            LOG_DEV_TRACE("Help string (type %i): \"%s\"", type << text);
        }
    }
}

HelpNode DH_Find(char const *id)
{
    // The identifiers are case insensitive.
    HelpStrings::const_iterator found = helps.constFind(String(id).lower());
    if(found != helps.constEnd())
    {
        return &found.value();
    }
    return NULL;
}

char const *DH_GetString(HelpNode found, int type)
{
    if(!found || type < 0 || type > NUM_HELPSTRING_TYPES)
        return NULL;

    StringsByType const *hs = reinterpret_cast<StringsByType const *>(found);

    StringsByType::const_iterator i = hs->constFind(type);
    if(i != hs->constEnd())
    {
        return Str_Text(AutoStr_FromTextStd(i.value().toUtf8().constData()));
    }
    return NULL;
}

void DD_InitHelp(void)
{
    LOG_AS("DD_InitHelp");
    try
    {
        readStrings(App::fileSystem().find("data/cphelp.txt"));
    }
    catch(Error const &er)
    {
        LOG_WARNING("") << er.asText();
    }
}

void DD_ReadGameHelp(void)
{
    LOG_AS("DD_ReadGameHelp");
    try
    {
        de::Uri uri(Path("$(App.DataPath)/$(GamePlugin.Name)/conhelp.txt"));
        readStrings(App::fileSystem().find(uri.resolved()));
    }
    catch(Error const &er)
    {
        LOG_WARNING("") << er.asText();
    }
}

void DD_ShutdownHelp(void)
{
    helps.clear();
}

D_CMD(LoadHelp)
{
    DENG2_UNUSED(src); DENG2_UNUSED(argc); DENG2_UNUSED(argv);

    DD_ShutdownHelp();
    DD_InitHelp();
    return true;
}
