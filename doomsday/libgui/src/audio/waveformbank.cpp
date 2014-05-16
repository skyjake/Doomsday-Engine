/** @file waveformbank.cpp  Bank containing Waveform instances.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/WaveformBank"
#include "de/App"

#include <de/ScriptedInfo>

namespace de {

DENG2_PIMPL_NOREF(WaveformBank)
{
    struct Source : public ISource
    {
        String filePath;

        Source(String const &path) : filePath(path) {}

        Time modifiedAt() const
        {
            return App::rootFolder().locate<File>(filePath).status().modifiedAt;
        }

        Waveform *load() const
        {
            QScopedPointer<Waveform> wf(new Waveform);
            wf->load(App::rootFolder().locate<File const>(filePath));
            return wf.take();
        }
    };

    struct Data : public IData
    {
        Waveform *waveform;

        Data(Waveform *wf = 0) : waveform(wf) {}

        duint sizeInMemory() const
        {
            if(!waveform) return 0;
            return waveform->sampleData().size();
        }
    };
};

WaveformBank::WaveformBank(Flags const &flags) : InfoBank("WaveformBank", flags), d(new Instance)
{}

void WaveformBank::add(DotPath const &id, String const &waveformFilePath)
{
    Bank::add(id, new Instance::Source(waveformFilePath));
}

void WaveformBank::addFromInfo(File const &file)
{
    LOG_AS("WaveformBank");
    parse(file);
    addFromInfoBlocks("waveform");
}

Waveform const &WaveformBank::waveform(DotPath const &id) const
{
    return *data(id).as<Instance::Data>().waveform;
}

Bank::ISource *WaveformBank::newSourceFromInfo(String const &id)
{
    Record const &def = info()[id];
    return new Instance::Source(relativeToPath(def) / def["path"]);
}

Bank::IData *WaveformBank::loadFromSource(ISource &source)
{
    return new Instance::Data(source.as<Instance::Source>().load());
}

Bank::IData *WaveformBank::newData()
{
    return new Instance::Data();
}

} // namespace de

