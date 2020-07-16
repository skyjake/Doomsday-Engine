/** @file gldrawqueue.cpp  Utility for managing and drawing semi-static GL buffers.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/gldrawqueue.h"
#include "de/glinfo.h"
#include "de/glprogram.h"
#include "de/glshader.h"
#include "de/glstate.h"
#include "de/glsubbuffer.h"
#include "de/gluniform.h"

namespace de {

#ifdef DE_DEBUG
extern int GLDrawQueue_queuedElems;
int        GLDrawQueue_queuedElems = 0;
#endif

DE_PIMPL_NOREF(GLDrawQueue)
{
    GLProgram *       currentProgram = nullptr;
    const GLBuffer *  currentBuffer  = nullptr;
    GLBuffer::Indices indices;
    List<GLBuffer *>  indexBuffers;
    int               indexBufferPos = 0;

    dsize batchIndex = 0;

    /// @todo These uniforms should be configurable.

    Vec4f defaultColor;
    std::unique_ptr<GLUniform> uBatchColors;

    Vec4f defaultScissor;
    GLUniform uBatchScissors { "uScissorRect", GLUniform::Vec4Array, GLShader::MAX_BATCH_UNIFORMS };

    float defaultSaturation = 1.f;
    GLUniform uBatchSaturation { "uSaturation", GLUniform::FloatArray, GLShader::MAX_BATCH_UNIFORMS };

    ~Impl()
    {
        deleteAll(indexBuffers);
    }

    GLBuffer &nextIndexBuffer()
    {
        if (indexBufferPos == indexBuffers.sizei())
        {
            // Allocate a new one.
            indexBuffers << new GLBuffer;
        }
        indexBufferPos++;
        return *indexBuffers.last();
    }

    void unsetProgram()
    {
        if (currentProgram)
        {
            if (uBatchColors)
            {
                currentProgram->unbind(*uBatchColors);
                uBatchColors.reset();

                currentProgram->unbind(uBatchScissors);
                currentProgram->unbind(uBatchSaturation);
            }
            currentProgram = nullptr;
        }
    }

    void restoreBatchValues()
    {
        if (uBatchColors)
        {
            uBatchColors   ->set(batchIndex, defaultColor);
            uBatchScissors  .set(batchIndex, defaultScissor);
            uBatchSaturation.set(batchIndex, defaultSaturation);
        }
    }
};

GLDrawQueue::GLDrawQueue()
    : d(new Impl)
{}

void GLDrawQueue::setProgram(GLProgram &program,
                             const Block &batchUniformName,
                             GLUniform::Type batchUniformType)
{
    if (d->currentProgram && d->currentProgram != &program)
    {
        flush();
    }
    d->unsetProgram();

    d->currentProgram = &program;

    if (batchUniformName)
    {
        d->uBatchColors.reset(
            new GLUniform(batchUniformName, batchUniformType, GLShader::MAX_BATCH_UNIFORMS));
        program << *d->uBatchColors;

        // Other batch variables.
        program << d->uBatchScissors;
        program << d->uBatchSaturation;
    }
}

int GLDrawQueue::batchIndex() const
{
    return int(d->batchIndex);
}

void GLDrawQueue::setBatchColor(const Vec4f &color)
{
    if (d->uBatchColors)
    {
        d->uBatchColors->set(d->batchIndex, color);
    }
    d->defaultColor = color;
}

void GLDrawQueue::setBatchSaturation(float saturation)
{
    d->uBatchSaturation.set(d->batchIndex, saturation);
    d->defaultSaturation = saturation;
}

void GLDrawQueue::setBatchScissorRect(const Vec4f &scissor)
{
    d->uBatchScissors.set(d->batchIndex, scissor);
    d->defaultScissor = scissor;
}

void GLDrawQueue::setBuffer(const GLBuffer &buffer)
{
    if (d->currentBuffer && &buffer != d->currentBuffer)
    {
        flush();
    }
    d->currentBuffer = &buffer;
}

void GLDrawQueue::enqueueDraw(const GLSubBuffer &buffer)
{
    DE_ASSERT(&buffer.hostBuffer() == d->currentBuffer);
    DE_ASSERT(d->currentProgram);

    if (buffer.size() == 0) return;

    // Stitch together with the previous strip.
    if (d->indices.size() > 0)
    {
        d->indices << d->indices.last();
        d->indices << buffer.hostRange().start;
    }

    for (duint16 i = buffer.hostRange().start;
                 i < buffer.hostRange().start + buffer.size(); ++i)
    {
        d->indices << i;
    }

    if (d->uBatchColors)
    {
        d->batchIndex++;
        if (d->batchIndex == GLShader::MAX_BATCH_UNIFORMS)
        {
            flush();
        }
        d->restoreBatchValues();
    }

#ifdef DE_DEBUG
    GLDrawQueue_queuedElems = d->indices.sizei();
#endif
}

void GLDrawQueue::flush()
{
    DE_ASSERT(d->currentProgram);

    if (d->currentBuffer)
    {
#ifdef DE_DEBUG
        GLDrawQueue_queuedElems = 0;
#endif
        GLState::current().apply();

        const dsize batchCount = d->batchIndex;

        /*qDebug() << "[GLDrawQueue] Flushing" << d->indices.size() << "elements"
                 << "consisting of" << batchCount << "batches";*/

        GLBuffer &indexBuffer = d->nextIndexBuffer();
        indexBuffer.setIndices(gfx::TriangleStrip, d->indices, gfx::Stream);
        d->indices.clear();

        if (d->uBatchColors)
        {
            d->uBatchColors   ->setUsedElementCount(batchCount);
            d->uBatchScissors  .setUsedElementCount(batchCount);
            d->uBatchSaturation.setUsedElementCount(batchCount);
        }

        d->currentProgram->beginUse();
        d->currentBuffer ->drawWithIndices(indexBuffer);
        d->currentProgram->endUse();

        d->indices.clear();
    }
    d->currentBuffer = nullptr;
    d->batchIndex = 0;

    // Keep using the latest scissor.
    d->restoreBatchValues();
}

void GLDrawQueue::beginFrame()
{
    d->indexBufferPos = 0;
}

} // namespace de

