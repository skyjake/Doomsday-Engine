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
        Instance *d;
        String id;

        RuleSource(Instance *inst, String const &ruleId) : d(inst), id(ruleId) {}
        Time modifiedAt() const { return d->modTime; }

        Rule *load() const
        {
            return refless(new ConstantRule(d->info[id].value().asNumber()));
        }
    };

    struct RuleData : public IData
    {
        Rule *rule;

        RuleData(Rule *r) : rule(holdRef(r)) {}
        ~RuleData() { releaseRef(rule); }

        duint sizeInMemory() const
        {
            return 0; // we don't count
        }
    };

    Time modTime;
    ScriptedInfo info;

    Instance(Public *i) : Base(i)
    {}
};

RuleBank::RuleBank()
    : Bank(DisableHotStorage), d(new Instance(this))
{}

void RuleBank::addFromInfo(String const &source)
{
    LOG_AS("RuleBank");
    try
    {
        d->modTime = Time();
        d->info.parse(source);

        foreach(String fn, d->info.allBlocksOfType("rule"))
        {
            add(fn, new Instance::RuleSource(d, fn));
        }
    }
    catch(Error const &er)
    {
        LOG_WARNING("Failed to read Info source:\n") << er.asText();
    }
}

void RuleBank::addFromInfo(File const &file)
{
    addFromInfo(String::fromUtf8(Block(file)));
    d->modTime = file.status().modifiedAt;
}

Rule const &RuleBank::rule(Path const &path) const
{
    return *static_cast<Instance::RuleData &>(data(path)).rule;
}

Bank::IData *RuleBank::loadFromSource(ISource &source)
{
    return new Instance::RuleData(static_cast<Instance::RuleSource &>(source).load());
}

} // namespace de
