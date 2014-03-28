/** @file waveform.cpp  Audio waveform.
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

#include "de/Waveform"
#include <de/FixedByteArray>
#include <de/Reader>

namespace de {

namespace internal {

struct WAVChunk : public IReadable
{
    Block id;
    duint32 size;

    WAVChunk() : id(4), size(0) {}
    void operator << (Reader &from) {
        from.readBytes(4, id) >> size;
    }
};

struct WAVFormat : public IReadable
{
    duint16 formatTag;
    duint16 channels;
    duint32 sampleRate;
    duint32 averageBytesPerSecond;
    duint16 blockAlign;
    duint16 bitsPerSample;

    WAVFormat()
        : formatTag(0)
        , channels(0)
        , sampleRate(0)
        , averageBytesPerSecond(0)
        , blockAlign(0)
        , bitsPerSample(0) {}

    void operator << (Reader &from)
    {
        from >> formatTag
             >> channels
             >> sampleRate
             >> averageBytesPerSecond
             >> blockAlign
             >> bitsPerSample;
    }
};

} // namespace internal
using namespace internal;

DENG2_PIMPL(Waveform)
, DENG2_OBSERVES(File, Deletion)
{
    audio::Format format;
    Block sampleData;
    File const *source;
    duint channelCount;
    duint bitsPerSample;
    dsize sampleCount;
    duint sampleRate;

    Instance(Public *i)
        : Base(i)
        , format(audio::RawPCMLittleEndian)
        , source(0)
        , channelCount (0)
        , bitsPerSample(0)
        , sampleCount  (0)
        , sampleRate   (0.0)
    {}

    ~Instance()
    {
        setSource(0);
    }

    void setSource(File const *src)
    {
        if(source) source->audienceForDeletion() -= this;
        source = src;
        if(source) source->audienceForDeletion() += this;
    }

    void clear()
    {
        setSource(0);
        format = audio::RawPCMLittleEndian;
        sampleData.clear();
        channelCount  = 0;
        bitsPerSample = 0;
        sampleCount   = 0;
        sampleRate    = 0.0;
    }

    void load(File const &src)
    {
        if(!src.name().fileNameExtension().compareWithoutCase(".wav"))
        {
            // We know how to read WAV files.
            loadWAV(Block(src));
        }
        else
        {
            // Let's assume it's a compressed audio format.
            format = audio::Compressed;
        }

        setSource(&src);
    }

    static bool recognizeWAV(IByteArray const &data)
    {
        Block magic(4);
        data.get(0, magic.data(), 4);
        if(magic != "RIFF") return false;
        data.get(8, magic.data(), 4);
        return (magic == "WAVE");
    }

    /**
     * Loads a sequence of audio samples in WAV format.
     *
     * @param data  Block containing WAV format data.
     */
    void loadWAV(Block const &data)
    {
        if(!recognizeWAV(data))
        {
            throw LoadError("Waveform::loadWAV", "WAV identifier not found");
        }

        Reader reader(data);
        reader.seek(12); // skip past header

        WAVFormat wav;
        while(reader.remainingSize() >= 8)
        {
            WAVChunk chunk;
            reader >> chunk;

            if(chunk.id == "fmt ") // Format chunk.
            {
                reader >> wav;

                // Check limitations.
                if(wav.formatTag != 1 /* PCM samples */)
                {
                    throw UnsupportedFormatError("Waveform::loadWAV",
                                                 "Only PCM samples supported");
                }

                channelCount  = wav.channels;
                sampleRate    = wav.sampleRate;
                bitsPerSample = wav.bitsPerSample;
            }
            else if(chunk.id == "data") // Sample data chunk.
            {
                sampleCount = chunk.size / wav.blockAlign;
                sampleData.resize(chunk.size);
                reader.readPresetSize(sampleData); // keep it little endian
            }
            else
            {
                // It's an unknown chunk.
                reader.seek(chunk.size);
            }
        }

        format = audio::RawPCMLittleEndian;
    }

    void fileBeingDeleted(File const &delFile)
    {
        if(source == &delFile)
        {
            // Could read file contents to memory if the waveform data
            // is still needed?
            source = 0;
        }
    }
};

Waveform::Waveform() : d(new Instance(this))
{}

void Waveform::clear()
{
    d->clear();
}

void Waveform::load(File const &file)
{
    d->clear();
    d->load(file);
}

audio::Format Waveform::format() const
{
    return d->format;
}

Block Waveform::sampleData() const
{
    return d->sampleData;
}

File const *Waveform::sourceFile() const
{
    return d->source;
}

duint Waveform::channelCount() const
{
    return d->channelCount;
}

duint Waveform::bitsPerSample() const
{
    return d->bitsPerSample;
}

dsize Waveform::sampleCount() const
{
    return d->sampleCount;
}

duint Waveform::sampleRate() const
{
    return d->sampleRate;
}

TimeDelta Waveform::duration() const
{
    return d->sampleRate * d->sampleCount;
}

} // namespace de
