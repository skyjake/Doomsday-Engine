/** @file rulebank.h  Bank of length Rules.
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

#ifndef LIBGUI_RULEBANK_H
#define LIBGUI_RULEBANK_H

#include "../InfoBank"
#include "../ConstantRule"

namespace de {

class File;
class Record;

/**
 * Bank of Rules where each is identified by a Path.
 */
class DENG2_PUBLIC RuleBank : public InfoBank
{
public:
    RuleBank();

    /**
     * Creates a number of rules based on information in an Info document.
     * The contents of the file are parsed first.
     *
     * @param source  File with Info source containing rule definitions.
     */
    void addFromInfo(File const &file);

    /**
     * Finds a specific rule.
     *
     * @param path  Identifier of the rule.
     *
     * @return  Rule instance.
     */
    Rule const &rule(Path const &path) const;

protected:
    virtual ISource *newSourceFromInfo(String const &id);
    virtual IData *loadFromSource(ISource &source);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_RULEBANK_H
