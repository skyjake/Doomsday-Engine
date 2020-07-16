/** @file waveformbank.h  Bank containing Waveform instances.
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

#ifndef LIBGUI_WAVEFORMBANK_H
#define LIBGUI_WAVEFORMBANK_H

#include "de/waveform.h"
#include <de/infobank.h>

namespace de {

class File;

/**
 * Bank containing Waveform instances loaded from files.
 *
 * @ingroup audio
 */
class LIBGUI_PUBLIC WaveformBank : public InfoBank
{
public:
    /**
     * Constructs a new audio waveform bank.
     *
     * @param flags  Bank behavior.
     */
    WaveformBank(const Flags &flags = DisableHotStorage);

    void add(const DotPath &id, const String &waveformFilePath);
    void addFromInfo(const File &file);

    const Waveform &waveform(const DotPath &id) const;

protected:
    ISource *newSourceFromInfo(const String &id);
    IData *loadFromSource(ISource &source);
    IData *newData();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_WAVEFORMBANK_H
