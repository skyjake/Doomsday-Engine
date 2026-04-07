/** @file rulebank.cpp  Bank of Rules.
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

#include "de/rulebank.h"
#include "de/constantrule.h"
#include "de/operatorrule.h"
#include "de/scripting/scriptedinfo.h"

namespace de {

const DotPath RuleBank::UNIT("unit");

DE_PIMPL_NOREF(RuleBank)
{
    const Rule *dpiRule = nullptr;

    struct RuleSource : public ISource
    {
        RuleBank &bank;
        String id;

        RuleSource(RuleBank &b, const String &ruleId) : bank(b), id(ruleId) {}
        Time modifiedAt() const { return bank.sourceModifiedAt(); }

        const Rule &load() const
        {
            const Record &def = bank[id];
            return *bank.d->dpiRule * float(def["constant"].value().asNumber());
        }
    };

    struct RuleData : public IData
    {
        const Rule *rule;

        RuleData(const Rule &r) : rule(holdRef(r)) {}
        ~RuleData() { releaseRef(rule); }
    };

    ~Impl()
    {
        releaseRef(dpiRule);
    }
};

RuleBank::RuleBank(const Rule &dpiRule)
    : InfoBank("RuleBank", DisableHotStorage)
    , d(new Impl)
{
    d->dpiRule = holdRef(dpiRule);
}

void RuleBank::addFromInfo(const File &file)
{
    LOG_AS("RuleBank");
    parse(file);
    addFromInfoBlocks("rule");
}

const Rule &RuleBank::rule(const DotPath &path) const
{
    if (path.isEmpty()) return ConstantRule::zero();
    return *static_cast<Impl::RuleData &>(data(path)).rule;
}

const Rule &RuleBank::dpiRule() const
{
    return *d->dpiRule;
}

Bank::ISource *RuleBank::newSourceFromInfo(const String &id)
{
    return new Impl::RuleSource(*this, id);
}

Bank::IData *RuleBank::loadFromSource(ISource &source)
{
    return new Impl::RuleData(static_cast<Impl::RuleSource &>(source).load());
}

} // namespace de
