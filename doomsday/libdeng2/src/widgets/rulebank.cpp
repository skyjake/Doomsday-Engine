/** @file rulebank.cpp  Bank of Rules.
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

#include "de/RuleBank"
#include <de/ConstantRule>
#include <de/ScriptedInfo>

namespace de {

DENG2_PIMPL(RuleBank)
{
    struct RuleSource : public ISource
    {
        InfoBank &bank;
        String id;

        RuleSource(InfoBank &b, String const &ruleId) : bank(b), id(ruleId) {}
        Time modifiedAt() const { return bank.sourceModifiedAt(); }

        Rule *load() const
        {
            Record const &def = bank[id];
            return refless(new ConstantRule(def["constant"].value().asNumber()));
        }
    };

    struct RuleData : public IData
    {
        Rule *rule;

        RuleData(Rule *r) : rule(holdRef(r)) {}
        ~RuleData() { releaseRef(rule); }
    };

    ConstantRule *zero;

    Instance(Public *i) : Base(i)
    {
        zero = new ConstantRule(0);
    }

    ~Instance()
    {
        releaseRef(zero);
    }
};

RuleBank::RuleBank() : InfoBank(DisableHotStorage), d(new Instance(this))
{}

void RuleBank::addFromInfo(File const &file)
{
    LOG_AS("RuleBank");
    parse(file);
    addFromInfoBlocks("rule");
}

Rule const &RuleBank::rule(DotPath const &path) const
{
    if(path.isEmpty()) return *d->zero;
    return *static_cast<Instance::RuleData &>(data(path)).rule;
}

Bank::ISource *RuleBank::newSourceFromInfo(String const &id)
{
    return new Instance::RuleSource(*this, id);
}

Bank::IData *RuleBank::loadFromSource(ISource &source)
{
    return new Instance::RuleData(static_cast<Instance::RuleSource &>(source).load());
}

} // namespace de
