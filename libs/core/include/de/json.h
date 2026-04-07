/** @file json.h  JSON parser and composer.
 * @ingroup data
 *
 * Parses JSON and outputs a QVariant with the data.
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_JSON_H
#define LIBCORE_JSON_H

#include "de/string.h"

namespace de {

class Block;
class Record;
class Value;

/**
 * Parses text as JSON and returns the data as a Record. This will work if the JSON root
 * object is a dictionary.
 *
 * @param jsonText  Text to parse.
 *
 * @return Parsed data, or an empty Record if an error occurred.
 */
DE_PUBLIC Record parseJSON(const String &jsonText);

/**
 * Parses text as JSON and returns the parsed value.
 *
 * @param jsonText  Text to parse.
 *
 * @return Parsed value. Caller gets ownership.
 */
DE_PUBLIC Value *parseJSONValue(const String &jsonText);

/**
 * Composes a JSON representation of a Record.
 *
 * @param rec  Record.
 *
 * @return JSON in UTF-8 encoding.
 */
DE_PUBLIC String composeJSON(const Record &rec);

} // namespace de

#endif // LIBCORE_JSON_H
