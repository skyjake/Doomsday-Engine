/** @file infobank.h  Abstract Bank read from Info definitions.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_INFOBANK_H
#define LIBDENG2_INFOBANK_H

#include "../String"
#include "../Time"
#include "../Bank"
#include "../IObject"

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
class DENG2_PUBLIC InfoBank : public Bank, public IObject
{
public:
    InfoBank(char const *nameForLog           = "InfoBank",
             Bank::Flags const &flags         = Bank::DefaultFlags,
             String const &hotStorageLocation = "/home/cache");

    /**
     * Parses definitions directly from Info source. This will erase the
     * previously parsed ScriptedInfo elements, but keep the existing
     * definitions kept in the IObject namespace.
     *
     * @param infoSource  Source text in Info syntax.
     */
    void parse(String const &infoSource);

    /**
     * Parses definitions from a file. This will erase the previously parsed
     * ScriptedInfo elements, but keep the existing definitions kept in the
     * IObject namespace.
     *
     * @param infoFile  File that uses Info syntax.
     */
    void parse(File const &infoFile);

    ScriptedInfo &info();
    ScriptedInfo const &info() const;

    void addFromInfoBlocks(String const &blockType);

    /**
     * Removes all bank items read from a matching source path. The check is
     * based on the special "__source__" variables in the object namespace.
     *
     * @param rootPath  Root path to look for.
     */
    void removeAllWithRootPath(String const &rootPath);

    /**
     * Removes all bank items whose source path can be identified as belonging
     * to a given package.
     *
     * @param packageId  Package identifier.
     */
    void removeAllFromPackage(String const &packageId);

    Time sourceModifiedAt() const;
    String bankRootPath() const;

    /**
     * Determines what relatives paths should be relative to, given a specific context.
     * This should be used when resolving paths in ScriptedInfo records.
     *
     * In practice, checks if the context has a "__source__" specified; if not, returns
     * the root path of the bank.
     *
     * @param context  Namespace to use as context.
     *
     * @return Path that relative paths should be resolved with.
     */
    String relativeToPath(Record const &context) const;

    // Implements IObject.
    Record &objectNamespace();
    Record const &objectNamespace() const;

protected:
    virtual ISource *newSourceFromInfo(String const &id) = 0;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_INFOBANK_H
