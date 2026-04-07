/** @file waveform.h  Audio waveform.
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

#ifndef LIBGUI_WAVEFORM_H
#define LIBGUI_WAVEFORM_H

#include "libgui.h"
#include <de/time.h>
#include <de/file.h>

namespace de {

namespace audio /// Audio related enumerations and constants.
{
    enum Format {
        PCMLittleEndian,
        Compressed
    };
}

/**
 * Audio waveform consisting of a sequence of audio samples in raw form or in some
 * compressed format. The sample data may be stored in memory or might be streamed from a
 * File.
 *
 * @ingroup audio
 */
class LIBGUI_PUBLIC Waveform
{
public:
    /// Failed to load audio data from source. @ingroup errors
    DE_ERROR(LoadError);

    /// Format of the source data is not supported. @ingroup errors
    DE_SUB_ERROR(LoadError, UnsupportedFormatError);

public:
    Waveform();

    void clear();

    /**
     * Loads an audio waveform from a file.
     *
     * @param file  File to load from.
     */
    void load(const File &file);

    audio::Format format() const;

    /**
     * Provides the sample data of the audio waveform in a memory buffer. For compressed
     * formats, the returned data is the contents of the source file.
     *
     * @return Block of audio samples.
     */
    Block sampleData() const;

    /**
     * Returns the File this Waveform has been loaded from.
     *
     * @return File with source data.
     */
    const File *sourceFile() const;

    duint channelCount() const;

    /**
     * Bits per sample on a channel.
     */
    duint bitsPerSample() const;

    dsize sampleCount() const;

    /**
     * Number of samples to play per second.
     */
    duint sampleRate() const;

    /**
     * Playing duration of the audio waveform, assuming sample count and sample rate
     * are known.
     */
    TimeSpan duration() const;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_WAVEFORM_H
