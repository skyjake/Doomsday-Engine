/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */
 
#include "de/Module"
#include "de/File"
#include "de/Process"
#include "de/Script"

using namespace de;

Module::Module(const File& sourceFile) : sourcePath_(sourceFile.path()), process_(0)
{
    // Load and execute the script.
    Script script(sourceFile);
    std::auto_ptr<Process> proc(new Process(script));
    proc->execute();
    process_ = proc.release();
}

Module::~Module()
{
    delete process_;
}

Record& Module::names()
{
    return process_->context().names();
}
