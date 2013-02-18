/** @file scriptsystem.h  Subsystem for the scripts.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef LIBDENG2_SCRIPTSYSTEM_H
#define LIBDENG2_SCRIPTSYSTEM_H

#include "../Error"
#include "../System"
#include "../Record"

namespace de {

/**
 * App subsystem for running scripts.
 */
class DENG2_PUBLIC ScriptSystem : public System
{
public:
    /// The module or script that was being looked for was not found. @ingroup errors
    DENG2_ERROR(NotFoundError);

public:
    ScriptSystem();

    ~ScriptSystem();

    /**
     * Adds a native module to the set of modules that can be imported in
     * scripts.
     *
     * @param name    Name of the module.
     * @param module  Module namespace. App will observe this for deletion.
     */
    void addNativeModule(String const &name, Record &module);

    /**
     * Imports a script module that is located on the import path.
     *
     * @param name      Name of the module.
     * @param fromPath  Absolute path of the script doing the importing.
     *
     * @return  The imported module.
     */
    Record &importModule(String const &name, String const &fromPath = "");

    void timeChanged(Clock const &);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_SCRIPTSYSTEM_H
