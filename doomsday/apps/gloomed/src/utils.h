/** @file utils.h  Utilities.
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef GLOOMAPP_UTILS_H
#define GLOOMAPP_UTILS_H

#include <QVector2D>
#include <QString>
#include <de/String>
#include <de/Vector>

inline de::Vec2d toVec2d(const QVector2D &vec)
{
    return de::Vec2d(vec.x(), vec.y());
}

inline QVector2D toQVector2D(const de::Vec2d &vec)
{
    return QVector2D(float(vec.x), float(vec.y));
}

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

#endif // GLOOMAPP_UTILS_H
