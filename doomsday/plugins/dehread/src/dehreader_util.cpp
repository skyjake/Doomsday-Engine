/**
 * @file dehreader_util.cpp
 * Miscellaneous utility routines. @ingroup dehread
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include "dehreader.h"
#include <QString>
#include <QStringList>
#include <de/Block>

void* DD_Realloc(void* ptr, int newSize)
{
    ded_count_t cnt;
    cnt.max = 0;
    cnt.num = newSize;
    DED_NewEntries(&ptr, &cnt, 1, 0);
    return ptr;
}

Uri* composeMapUri(int episode, int map)
{
    /// @todo broken format strings.
    if(episode > 0) // ExMy format.
    {
        de::Block pathUtf8 = QString("E%1M%2").arg(episode).arg(map).toUtf8();
        return Uri_NewWithPath2(pathUtf8.constData(), RC_NULL);
    }
    else // MAPxx format.
    {
        de::Block pathUtf8 = QString("MAP%1").arg(map % 100).toUtf8();
        return Uri_NewWithPath2(pathUtf8.constData(), RC_NULL);
    }
}

int mapInfoDefForUri(const Uri& uri, ded_mapinfo_t** def)
{
    if(!Str_IsEmpty(Uri_Path(&uri)))
    for(int i = ded->count.mapInfo.num - 1; i >= 0; i--)
    {
        ded_mapinfo_t& info = ded->mapInfo[i];
        if(info.uri && Uri_Equality(info.uri, &uri))
        {
            if(def) *def = &info;
            return i;
        }
    }
    return -1; // Not found.
}

int valueDefForPath(const QString& id, ded_value_t** def)
{
    if(!id.isEmpty())
    {
        de::Block idUtf8 = id.toUtf8();
        for(int i = ded->count.values.num - 1; i >= 0; i--)
        {
            ded_value_t& value = ded->values[i];
            if(!qstricmp(value.id, idUtf8.constData()))
            {
                if(def) *def = &value;
                return i;
            }
        }
    }
    return -1; // Not found.
}

/// @todo Reimplement with a regex?
QStringList splitMax(const QString& str, QChar sep, int max)
{
    if(max < 0)
    {
        return str.split(sep);
    }
    else if(max == 0)
    {
        return QStringList(); // An empty list.
    }
    else if(max == 1)
    {
        return QStringList(str);
    }

    QString buf = str;
    QStringList tokens;

    int pos = 0, substrEnd = 0;
    while(pos < max-1 && (substrEnd = buf.indexOf(sep)) >= 0)
    {
        tokens << buf.mid(0, substrEnd);

        // Find the start of the next token.
        while(substrEnd < buf.size() && buf.at(substrEnd) == sep)
        { ++substrEnd; }

        buf.remove(0, substrEnd);

        pos++; // On to the next substring.
    }

    // Anything remaining goes into the last token (the rest of the line).
    if(pos < max)
    {
        tokens << buf;
    }

    //qDebug() << "splitMax: " << tokens.join("|");

    return tokens;
}
