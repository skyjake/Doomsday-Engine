/** @file waveformbank.cpp  Bank containing Waveform instances.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/waveformbank.h"
#include "de/app.h"
#include "de/folder.h"

#include <de/scripting/scriptedinfo.h>

namespace de {

DE_PIMPL_NOREF(WaveformBank)
{
    struct Source : public ISource
    {
        String filePath;

        Source(const String &path) : filePath(path) {}

        Time modifiedAt() const
        {
            return App::rootFolder().locate<File>(filePath).status().modifiedAt;
        }

        Waveform *load() const
        {
            std::unique_ptr<Waveform> wf(new Waveform);
            wf->load(App::rootFolder().locate<File const>(filePath));
            return wf.release();
        }
    };

    struct Data : public IData
    {
        Waveform *waveform;

        Data(Waveform *wf = 0) : waveform(wf) {}

        duint sizeInMemory() const
        {
            if (!waveform) return 0;
            return waveform->sampleData().size();
        }
    };
};

WaveformBank::WaveformBank(const Flags &flags) : InfoBank("WaveformBank", flags), d(new Impl)
{}

void WaveformBank::add(const DotPath &id, const String &waveformFilePath)
{
    Bank::add(id, new Impl::Source(waveformFilePath));
}

void WaveformBank::addFromInfo(const File &file)
{
    LOG_AS("WaveformBank");
    parse(file);
    addFromInfoBlocks("waveform");
}

const Waveform &WaveformBank::waveform(const DotPath &id) const
{
    return *data(id).as<Impl::Data>().waveform;
}

Bank::ISource *WaveformBank::newSourceFromInfo(const String &id)
{
    const Record &def = info()[id];
    return new Impl::Source(absolutePathInContext(def, def["path"]));
}

Bank::IData *WaveformBank::loadFromSource(ISource &source)
{
    return new Impl::Data(source.as<Impl::Source>().load());
}

Bank::IData *WaveformBank::newData()
{
    return new Impl::Data();
}

} // namespace de

