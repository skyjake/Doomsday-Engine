/** @file rulebank.h  Bank of length Rules.
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

#ifndef LIBGUI_RULEBANK_H
#define LIBGUI_RULEBANK_H

#include "de/infobank.h"
#include "de/constantrule.h"

namespace de {

class File;
class Record;

/**
 * Bank of Rules where each is identified by a Path.
 * @ingroup widgets
 */
class DE_PUBLIC RuleBank : public InfoBank
{
public:
    RuleBank(const Rule &dpiRule);

    /**
     * Creates a number of rules based on information in an Info document.
     * The contents of the file are parsed first.
     *
     * @param file  File with Info source containing rule definitions.
     */
    void addFromInfo(const File &file);

    /**
     * Finds a specific rule.
     *
     * @param path  Identifier of the rule.
     *
     * @return  Rule instance.
     */
    const Rule &rule(const DotPath &path) const;

    const Rule &dpiRule() const;

    static DotPath const UNIT;

protected:
    virtual ISource *newSourceFromInfo(const String &id);
    virtual IData *loadFromSource(ISource &source);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_RULEBANK_H
