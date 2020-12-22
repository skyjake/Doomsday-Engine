/** @file filelogsink.cpp  Log sink that uses a File for output.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/filelogsink.h"
#include "de/filesystem.h"

namespace de {

FileLogSink::FileLogSink(File &outputFile)
    : LogSink(_format)
    , _file(&outputFile)
{}

LogSink &FileLogSink::operator<<(const String &plainText)
{
    static const Block endl("\n");
    if (_file)
    {
        *_file << plainText;
        *_file << endl;
    }
    return *this;
}

void FileLogSink::flush()
{
    if (_file)
    {
        _file->flush();
    }
}

} // namespace de
