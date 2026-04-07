/** @file scriptsystem.h  Subsystem for the scripts.
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

#ifndef LIBCORE_SCRIPTSYSTEM_H
#define LIBCORE_SCRIPTSYSTEM_H

#include "de/error.h"
#include "de/system.h"
#include "de/record.h"
#include "de/file.h"

namespace de {

/**
 * App subsystem for running scripts.
 *
 * @ingroup script
 */
class DE_PUBLIC ScriptSystem : public System
{
public:
    /// The module or script that was being looked for was not found. @ingroup errors
    DE_ERROR(NotFoundError);

public:
    ScriptSystem();

    ~ScriptSystem();

    /**
     * Specifies an additional path where modules may be imported from.
     * "Config.importPath" is checked before any of the paths specified using this
     * method.
     *
     * Paths specified using this method are not saved persistently in Config.
     *
     * @param path  Additional module import path.
     */
    void addModuleImportPath(const Path &path);

    void removeModuleImportPath(const Path &path);

    /**
     * Adds a native module to the set of modules that can be imported in
     * scripts.
     *
     * @param name    Name of the module.
     * @param module  Module namespace. App will observe this for deletion.
     */
    void addNativeModule(const String &name, Record &module);

    void removeNativeModule(const String &name);

    bool nativeModuleExists(const String &name) const;

    Record &nativeModule(const String &name);

    /**
     * Returns a native or an imported module.
     *
     * @param name  Name of the module.
     *
     * @return Module namespace.
     */
    Record &operator[](const String &nativeModuleName);

    /**
     * Returns a list of the names of all the existing native modules.
     *
     * @return List of module names.
     */
    StringList nativeModules() const;

    /**
     * Imports a script module that is located on the import path.
     *
     * @param name              Name of the module.
     * @param importedFromPath  Absolute path of the script doing the importing.
     *
     * @return  The imported module.
     */
    Record &importModule(const String &name, const String &importedFromPath = "");

    /**
     * Looks for the source file of a module.
     *
     * @param name       Name of the module to look for.
     * @param localPath  Which absolute path to use as the local folder (as import path "").
     *
     * @return Found source file, or @c NULL.
     */
    const File *tryFindModuleSource(const String &name, const String &localPath = "");

    /**
     * Looks for the source file of a module.
     *
     * @param name       Name of the module to look for.
     * @param localPath  Which absolute path to use as the local folder (as import path "").
     *
     * @return Found source file.
     */
    const File &findModuleSource(const String &name, const String &localPath = "");

    void timeChanged(const Clock &);

public:
    /**
     * Returns a built-in Doomsday Script class from the Core module.
     * @param name  Name of the class.
     * @return Class record.
     */
    static Record &builtInClass(const String &name);

    /**
     * Returns a built-in Doomsday Script class from the specified module.
     * @param nativeModuleName  Name of the module where the class is located.
     * @param className         Name of the class.
     * @return Class record.
     */
    static Record &builtInClass(const String &nativeModuleName,
                                const String &className);

    static ScriptSystem &get();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_SCRIPTSYSTEM_H
