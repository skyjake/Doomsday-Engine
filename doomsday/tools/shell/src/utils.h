/** @file utils.h
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef GUISHELLAPP_UTILS_H
#define GUISHELLAPP_UTILS_H

#include <QString>
#include <de/String>

QString imageResourcePath(QString const &path);

inline de::String convert(const QString &qstr)
{
    return qstr.toStdWString();
}

inline de::String convertToString(const QString &qstr)
{
    return convert(qstr);
}

inline QString convert(const de::String &str)
{
    return QString::fromUtf8(str);
}

inline QString convertToQString(const de::String &str)
{
    return convert(str);
}

#endif // GUISHELLAPP_UTILS_H
