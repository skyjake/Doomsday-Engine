/** @file glatlasbuffer.cpp  GLBuffer from where GLSubBuffers are allocated.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/GLAtlasBuffer"
#include "de/GLSubBuffer"

#include <QList>

namespace de {

static dsize const MIN_RANGE_SIZE = 16;

DENG2_PIMPL_NOREF(GLAtlasBuffer)
{
    struct HostBuffer
    {
        GLBuffer buffer;
        QList<Rangeui16> availableRanges;

        void addAvailable(Rangeui16 const &range)
        {
            DENG2_ASSERT(range.size() >= MIN_RANGE_SIZE);
            availableRanges.append(range);
            qSort(availableRanges.begin(), availableRanges.end(),
                  [] (Rangeui16 const &a, Rangeui16 const &b) { return a.size() < b.size(); });
        }

        Rangeui16 findBestAvailable(dsize forSize)
        {
            for (auto i = availableRanges.begin(); i != availableRanges.end(); ++i)
            {
                if (i->size() >= forSize)
                {
                    Rangeui16 chosen = *i;
                    availableRanges.erase(i);
                    if (chosen.size() - forSize >= MIN_RANGE_SIZE)
                    {
                        // Split the extra part into a new available range.
                        addAvailable(Rangeui16(chosen.start + forSize, chosen.end));
                    }
                    return chosen;
                }
            }
            return Rangeui16();
        }
    };
    QList<HostBuffer *> hostBuffers;
    gl::Usage usage = gl::Static;
    dsize elementSize = 0;
    duint16 maxElementCount = 0;
    internal::AttribSpecs format;

    ~Impl()
    {
        deinit();
    }

    HostBuffer &allocNewHostBuffer()
    {
        DENG2_ASSERT(maxElementCount > 0);
        DENG2_ASSERT(elementSize > 0);

        auto *host = new HostBuffer;
        host->availableRanges << Rangeui16(0, maxElementCount);
        host->buffer.setFormat(format);
        host->buffer.setUninitializedData(elementSize * maxElementCount, usage);
        hostBuffers.append(host);
        qDebug() << "[GLAtlasBuffer] Now have" << hostBuffers.size() << "buffers";
        return *host;
    }

    void clear()
    {
        for (auto *host : hostBuffers)
        {
            host->availableRanges.clear();
            host->availableRanges << Rangeui16(0, maxElementCount);
        }
    }

    void deinit()
    {
        qDeleteAll(hostBuffers);
        hostBuffers.clear();
    }

    GLSubBuffer *allocSubBuffer(duint16 elementCount)
    {
        GLSubBuffer *sub = nullptr;
        for (auto *host : hostBuffers)
        {
            auto const bestRange = host->findBestAvailable(elementCount);
            if (!bestRange.isEmpty())
            {
                sub = new GLSubBuffer(bestRange, host->buffer);
                break;
            }
        }
        if (!sub)
        {
            auto &empty = allocNewHostBuffer();
            sub = new GLSubBuffer(empty.findBestAvailable(elementCount), empty.buffer);
        }
        sub->setFormat(format);
        return sub;
    }

    void releaseSubBuffer(GLSubBuffer &)
    {
        /// @todo Implement if needed.
        DENG2_ASSERT(false);
    }
};

GLAtlasBuffer::GLAtlasBuffer(internal::AttribSpecs const &vertexFormat)
    : d(new Impl)
{
    d->format = vertexFormat;
    d->elementSize = vertexFormat.first->stride;
}

void GLAtlasBuffer::setMaxElementCount(dsize maxElementCount)
{
    DENG2_ASSERT(maxElementCount < 0x10000); // 16-bit indexing
    d->maxElementCount = duint16(maxElementCount);
}

void GLAtlasBuffer::setUsage(gl::Usage usage)
{
    d->usage = usage;
}

void GLAtlasBuffer::clear()
{
    d->clear();
}

GLSubBuffer *GLAtlasBuffer::alloc(dsize elementCount)
{
    DENG2_ASSERT(elementCount > 0);
    DENG2_ASSERT(elementCount < 0x10000);
    return d->allocSubBuffer(duint16(elementCount));
}

void GLAtlasBuffer::release(GLSubBuffer &buf)
{
    d->releaseSubBuffer(buf);
}

} // namespace de
