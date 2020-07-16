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

#include "de/scripting/module.h"
#include "de/scripting/process.h"
#include "de/scripting/script.h"
#include "de/file.h"
#include "de/app.h"
#include "de/folder.h"

using namespace de;

Module::Module(const String &sourcePath) : _sourcePath(sourcePath), _process(0)
{
    // Load the script.
    initialize(Script(App::rootFolder().locate<File>(sourcePath)));
}

Module::Module(const File &sourceFile) : _sourcePath(sourceFile.path()), _process(0)
{
    initialize(Script(sourceFile));
}

void Module::initialize(const Script &script)
{
    // Execute the script.
    std::unique_ptr<Process> proc(new Process(script));
    proc->execute();
    _process = proc.release();
}

Module::~Module()
{
    delete _process;
}

Record &Module::names()
{
    return _process->context().names();
}
