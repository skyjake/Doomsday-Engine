/** @file iinterpreter.h  File interpreter.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_FILESYS_IINTERPRETER_H
#define LIBCORE_FILESYS_IINTERPRETER_H

namespace de {

class File;

namespace filesys {

/**
 * File interpreter interface.
 *
 * Interpreters produce specialized File instances that convert raw file
 * contents to domain-specific representations (e.g., image content).
 *
 * @ingroup fs
 */
class IInterpreter
{
public:
    virtual ~IInterpreter() = default;

    /**
     * Attempts to interpret a file.
     *
     * @param file  File whose contents are being interpreted. If
     *              interpretation is possible, ownership of @a file is
     *              given to the interpreter.
     *
     * @return  Interpreter for the file (ownership given to caller), or
     * NULL if no possible interpretation was recognized (in which case
     * ownership of @a file is retained by the caller).
     */
    virtual File *interpretFile(File *file) const = 0;
};

} // namespace filesys
} // namespace de

#endif // LIBCORE_FILESYS_IINTERPRETER_H

