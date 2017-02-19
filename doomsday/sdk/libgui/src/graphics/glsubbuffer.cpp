/** @file glsubbuffer.cpp  Sub-range of a larger GLBuffer.
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

#include "de/GLSubBuffer"

namespace de {

DENG2_PIMPL_NOREF(GLSubBuffer)
{
    GLBuffer *host = nullptr;
    Rangeui16 hostRange;
    dsize size = 0;
    internal::AttribSpecs format;
    int batchIndexOffset = -1;
};

GLSubBuffer::GLSubBuffer(Rangeui16 const &hostRange, GLBuffer &hostBuffer)
    : d(new Impl)
{
    //d->reservedSize = reservedSize;
    d->host = &hostBuffer;
    d->hostRange = hostRange;
}

dsize GLSubBuffer::size() const
{
    return d->size;
}

void GLSubBuffer::clear()
{
    d->size = 0;
}

void GLSubBuffer::setVertices(dsize count, void const *data)
{
    DENG2_ASSERT(count <= d->hostRange.size());

    dsize const elementSize = d->format.first->stride;
    d->host->setData(elementSize * d->hostRange.start, data,
                     elementSize * count);
    d->size = count;
}

void GLSubBuffer::setBatchVertices(int batchIndex, dsize elementCount, void *data)
{
    DENG2_ASSERT(d->batchIndexOffset >= 0);

    dsize const elementSize = d->format.first->stride;
    duint8 *elems = reinterpret_cast<duint8 *>(data);
    for (dsize i = 0; i < elementCount; ++i)
    {
        *reinterpret_cast<float *>(elems + d->batchIndexOffset + i * elementSize)
                = float(batchIndex);
    }
    setVertices(elementCount, data);
}

GLBuffer &GLSubBuffer::hostBuffer() const
{
    return *d->host;
}

void GLSubBuffer::setHostBuffer(GLBuffer &hostBuffer)
{
    d->host = &hostBuffer;
}

Rangeui16 const &GLSubBuffer::hostRange() const
{
    return d->hostRange;
}

void GLSubBuffer::setFormat(internal::AttribSpecs const &format)
{
    d->format = format;
    d->host->setFormat(format);

    auto const *specs = format.first;
    for (dsize i = 0; i < format.second; ++i)
    {
        if (specs[i].semantic == internal::AttribSpec::Index)
        {
            d->batchIndexOffset = int(specs[i].startOffset);
        }
    }
}

void GLSubBuffer::setHostRange(Rangeui16 const &range)
{
    d->hostRange = range;
}

} // namespace de
