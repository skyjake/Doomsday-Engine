/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_SCRIPT_H
#define LIBCORE_SCRIPT_H

/**
 * @defgroup script Script Runtime
 *
 * The script runtime parses and executes scripts. @ingroup core
 */

#include "compound.h"
#include "de/string.h"

#include <vector>

namespace de {

class File;

/**
 * Contains statements and expressions which are ready to be executed.
 * A Script instance is built from source code text, which is parsed and
 * converted to statements and expressions.
 *
 * @see Process (used to execute scripts)
 *
 * @ingroup script
 */
class DE_PUBLIC Script
{
public:
    /// Constructs an empty script with no statements.
    Script();

    /**
     * Parses the source into statements.
     *
     * @param source  Script source.
     */
    Script(const String &source);

    /**
     * Parses the source file info statements. The path of the source file
     * is saved and used when importing modules.
     *
     * @param file  Source file.
     */
    Script(const File &file);

    /**
     * Parses a source into statements, replacing any statements currently
     * in the Script. The user must ensure that the script is not currently
     * being executed by a Process.
     *
     * @param source  Script source.
     */
    void parse(const String &source);

    /**
     * Sets the path of the source. Used as the value of __file__ in the
     * executing process's global namespace.
     *
     * @param path  Path.
     */
    void setPath(const String &path);

    const String &path() const;

    /// Returns the statement that begins the script. This is where
    /// a process begins the execution of a script.
    const Statement *firstStatement() const;

    /// Returns a modifiable reference to the main statement compound
    /// of the script.
    Compound &compound();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif
