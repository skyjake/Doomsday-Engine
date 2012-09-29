/**
 * @file json.cpp
 * JSON parser. @ingroup data
 *
 * Parses JSON and outputs a QVariant with the data.
 *
 * @todo Move this to libdeng2.
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG_JSON_H
#define LIBDENG_JSON_H

#ifdef __cplusplus

#include <QVariant>
#include <de/String>

/**
 * Parses text as JSON and returns the data structured in a QVariant.
 *
 * @param jsonText  Text to parse.
 *
 * @return Parsed data, or an invalid variant if an error occurred.
 */
QVariant parseJSON(const de::String& jsonText);

#endif // __cplusplus

#endif // LIBDENG_JSON_H
