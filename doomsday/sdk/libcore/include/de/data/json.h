/** @file json.h  JSON parser and composer.
 * @ingroup data
 *
 * Parses JSON and outputs a QVariant with the data.
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_JSON_H
#define LIBDENG2_JSON_H

#include <QVariant>
#include <de/String>

namespace de {

class Block;
class Record;

/**
 * Parses text as JSON and returns the data structured in a QVariant.
 *
 * @param jsonText  Text to parse.
 *
 * @return Parsed data, or an invalid variant if an error occurred.
 */
DENG2_PUBLIC QVariant parseJSON(String const &jsonText);

/**
 * Composes a JSON representation of a Record.
 *
 * @param rec  Record.
 *
 * @return JSON in UTF-8 encoding.
 */
DENG2_PUBLIC Block composeJSON(Record const &rec);

} // namespace de

#endif // LIBDENG2_JSON_H
