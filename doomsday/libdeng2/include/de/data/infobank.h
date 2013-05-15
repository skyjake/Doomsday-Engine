/** @file infobank.h  Abstract Bank read from Info definitions.
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

#ifndef LIBDENG2_INFOBANK_H
#define LIBDENG2_INFOBANK_H

#include "../String"
#include "../Time"
#include "../Bank"

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
 */
class DENG2_PUBLIC InfoBank : public Bank
{
public:
    InfoBank(Bank::Flags const &flags         = Bank::DefaultFlags,
             String const &hotStorageLocation = "/home/cache");

    void parse(String const &infoSource);
    void parse(File const &infoFile);

    ScriptedInfo &info();
    ScriptedInfo const &info() const;

    Record &names();
    Record const &names() const;
    Variable const &operator [] (String const &name) const;

    void addFromInfoBlocks(String const &blockType);

    Time sourceModifiedAt() const;

protected:
    virtual ISource *newSourceFromInfo(String const &id) = 0;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_INFOBANK_H
