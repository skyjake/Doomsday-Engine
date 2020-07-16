/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 
#ifndef LIBCORE_MODULE_H
#define LIBCORE_MODULE_H

#include "de/string.h"

namespace de {

class File;
class Process;
class Record;
class Script;

/**
 * Modules are scripts that are loaded from the file system and then stay in
 * memory for other scripts to use. For instance, a module could contain
 * functions that are useful for many other scripts. Scripts can use the import
 * statement to gain access to a module's namespace. The module's source script
 * is executed only the first time it gets imported -- subsequent calls simply
 * provide a reference to the existing module's namespace.
 *
 * @ingroup script
 */
class Module
{
public:
    /**
     * Constructs a new module. The source script is loaded from a
     * file and executed.
     *
     * @param sourcePath  Path of the source file.
     */
    Module(const String &sourcePath);

    /**
     * Constructs a new module. The source script is loaded from a
     * file and executed.
     *
     * @param sourceFile  Script source file.
     */
    Module(const File &sourceFile);

    virtual ~Module();

    /// Returns the module's source script's absolute path.
    const String &sourcePath() const { return _sourcePath; }

    /// Returns the namespace of the module. The import statement gives access
    /// to this.
    Record &names();

protected:
    void initialize(const Script &script);

private:
    /// Path of the script source file. Used to identify whether a script has
    /// already been loaded or not. Imported scripts are run only once.
    String _sourcePath;

    /// Process where the source script of the module was executed. Owns
    /// the namespace of the module.
    Process *_process;
};

} // namespace de

#endif /* LIBCORE_MODULE_H */
