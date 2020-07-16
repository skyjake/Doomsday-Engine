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

#include "de/glatlasbuffer.h"
#include "de/glsubbuffer.h"

#include <de/list.h>

namespace de {

static const dsize MIN_RANGE_SIZE = 16;

DE_PIMPL_NOREF(GLAtlasBuffer)
{
    struct HostBuffer
    {
        GLBuffer buffer;
        List<Rangeui16> availableRanges;

        void addAvailable(const Rangeui16 &range)
        {
            DE_ASSERT(dsize(range.size()) >= MIN_RANGE_SIZE);
            availableRanges.append(range);
            std::sort(availableRanges.begin(),
                      availableRanges.end(),
                      [](const Rangeui16 &a, const Rangeui16 &b) { return a.size() < b.size(); });
        }

        Rangeui16 findBestAvailable(dsize forSize)
        {
            for (auto i = availableRanges.begin(); i != availableRanges.end(); ++i)
            {
                if (i->size() >= int(forSize))
                {
                    Rangeui16 chosen = *i;
                    availableRanges.erase(i);
                    if (chosen.size() - forSize >= MIN_RANGE_SIZE)
                    {
                        // Split the extra part into a new available range.
                        addAvailable(Rangeui16(duint16(chosen.start + forSize), chosen.end));
                    }
                    return chosen;
                }
            }
            return Rangeui16();
        }
    };
    List<HostBuffer *> hostBuffers;
    gfx::Usage usage = gfx::Static;
    dsize elementSize = 0;
    duint16 maxElementCount = 0;
    internal::AttribSpecs format;

    ~Impl()
    {
        deinit();
    }

    HostBuffer &allocNewHostBuffer()
    {
        DE_ASSERT(maxElementCount > 0);
        DE_ASSERT(elementSize > 0);

        auto *host = new HostBuffer;
        host->availableRanges << Rangeui16(0, maxElementCount);
        host->buffer.setFormat(format);
        host->buffer.setUninitializedData(elementSize * maxElementCount, usage);
        hostBuffers.append(host);
        //qDebug() << "[GLAtlasBuffer] Now have" << hostBuffers.size() << "buffers";
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
        deleteAll(hostBuffers);
        hostBuffers.clear();
    }

    GLSubBuffer *allocSubBuffer(duint16 elementCount)
    {
        GLSubBuffer *sub = nullptr;
        for (auto *host : hostBuffers)
        {
            const auto bestRange = host->findBestAvailable(elementCount);
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
        DE_ASSERT_FAIL("GLAtlasBuffer::releaseSubBuffer not implemented");
    }
};

GLAtlasBuffer::GLAtlasBuffer(const internal::AttribSpecs &vertexFormat)
    : d(new Impl)
{
    d->format = vertexFormat;
    d->elementSize = vertexFormat.first->stride;
}

void GLAtlasBuffer::setMaxElementCount(dsize maxElementCount)
{
    DE_ASSERT(maxElementCount < 0x10000); // 16-bit indexing
    d->maxElementCount = duint16(maxElementCount);
}

void GLAtlasBuffer::setUsage(gfx::Usage usage)
{
    d->usage = usage;
}

void GLAtlasBuffer::clear()
{
    d->clear();
}

GLSubBuffer *GLAtlasBuffer::alloc(dsize elementCount)
{
    DE_ASSERT(elementCount > 0);
    DE_ASSERT(elementCount < 0x10000);
    return d->allocSubBuffer(duint16(elementCount));
}

void GLAtlasBuffer::release(GLSubBuffer &buf)
{
    d->releaseSubBuffer(buf);
}

} // namespace de
