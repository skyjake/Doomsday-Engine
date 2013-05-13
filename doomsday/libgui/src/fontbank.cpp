/** @file fontbank.cpp  Font bank.
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

#include "de/FontBank"
#include <de/ScriptedInfo>
#include <de/Block>

namespace de {

DENG2_PIMPL(FontBank)
{
    ScriptedInfo info;

    Instance(Public *i) : Base(i)
    {}
};

FontBank::FontBank()
    : Bank(DisableHotStorage), d(new Instance(this))
{}

FontBank::FontBank(String const &source)
    : Bank(DisableHotStorage), d(new Instance(this))
{
    readInfo(source);
}

FontBank::FontBank(File const &file)
    : Bank(DisableHotStorage), d(new Instance(this))
{
    readInfo(String::fromUtf8(Block(file)));
}

void FontBank::readInfo(String const &source)
{
    LOG_AS("FontBank");
    try
    {
        d->info.parse(source);

        ScriptedInfo::Paths fonts = d->info.allBlocksOfType("font");
        LOG_DEBUG("Found %i fonts:") << fonts.size();
        foreach(String fn, fonts)
        {
            LOG_DEBUG("  %s") << fn;
        }
    }
    catch(Error const &er)
    {
        LOG_WARNING("Failed to read Info source:\n") << er.asText();
    }
}

void FontBank::readInfo(File const &file)
{
    readInfo(String::fromUtf8(Block(file)));
}

Bank::IData *FontBank::loadFromSource(ISource &)
{
    return 0;
}

Bank::IData *FontBank::newData()
{
    return 0;
}

} // namespace de
