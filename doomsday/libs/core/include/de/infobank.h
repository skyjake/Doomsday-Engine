/** @file infobank.h  Abstract Bank read from Info definitions.
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

#ifndef LIBCORE_INFOBANK_H
#define LIBCORE_INFOBANK_H

#include "de/string.h"
#include "de/time.h"
#include "de/bank.h"
#include "de/scripting/iobject.h"

namespace de {

class ScriptedInfo;
class Record;
class Variable;
class File;

/**
 * Abstract Bank read from Info definitions.
 *
 * InfoBank handles the common plumbing of parsing an Info file and iterating
 * through it for creating bank sources.
 *
 * InfoBank has its own namespace where ScriptedInfo will store all variables
 * from all parsed sources.
 *
 * @ingroup data
 */
class DE_PUBLIC InfoBank : public Bank, public IObject
{
public:
    InfoBank(const char *  nameForLog         = "InfoBank",
             const Flags & flags              = Bank::DefaultFlags,
             const String &hotStorageLocation = "/home/cache");

    /**
     * Parses definitions directly from Info source. This will erase the
     * previously parsed ScriptedInfo elements, but keep the existing
     * definitions kept in the IObject namespace.
     *
     * @param infoSource  Source text in Info syntax.
     */
    void parse(const String &infoSource);

    /**
     * Parses definitions from a file. This will erase the previously parsed
     * ScriptedInfo elements, but keep the existing definitions kept in the
     * IObject namespace.
     *
     * @param infoFile  File that uses Info syntax.
     */
    void parse(const File &infoFile);

    ScriptedInfo &      info();
    const ScriptedInfo &info() const;

    void addFromInfoBlocks(const String &blockType);

    /**
     * Removes all bank items read from a matching source path. The check is
     * based on the special "__source__" variables in the object namespace.
     *
     * @param rootPath  Root path to look for.
     */
    void removeAllWithRootPath(const String &rootPath);

    /**
     * Removes all bank items whose source path can be identified as belonging
     * to a given package.
     *
     * @param packageId  Package identifier.
     */
    void removeAllFromPackage(const String &packageId);

    Time   sourceModifiedAt() const;
    String bankRootPath() const;

    /**
     * Resolves a relative path into an absoluate path in the context of @a context.
     * This should be used when processing paths in ScriptedInfo records.
     *
     * In practice, first checks if the context has a "__source__" specified. If the
     * relative path cannot be found, also checks the possible inherited source locations.
     * If source information is not specified, uses the the root path of the bank.
     *
     * @param context       Namespace to use as context.
     * @param relativePath  Relative path to resolve.
     *
     * @return Absolute path.
     */
    String absolutePathInContext(const Record &context, const String &relativePath) const;

    // Implements IObject.
    Record &      objectNamespace() override;
    const Record &objectNamespace() const override;

protected:
    virtual ISource *newSourceFromInfo(const String &id) = 0;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_INFOBANK_H
