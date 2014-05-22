/** @file rendersystem.h  Render subsystem.
 *
 * @authors Copyright © 2013-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_RENDERSYSTEM_H
#define CLIENT_RENDERSYSTEM_H

#include <QtAlgorithms>
#include <QList>
#include <QVector>
#include <de/GLBuffer>
#include <de/GLShaderBank>
#include <de/ImageBank>
#include <de/Log>
#include <de/System>
#include <de/Vector>
#include "settingsregister.h"

class DrawLists;
class Material;

/**
 * Pooled work buffer set.
 *
 * @ingroup render
 */
template <typename Type>
class BufferPoolT
{
public:
    typedef Type ValueType;
    typedef QVector<ValueType> Values;

public:
    BufferPoolT() {}
    ~BufferPoolT() { clear(); }

    void clear()
    {
        qDeleteAll(items);
        items.clear();
    }

    void reset()
    {
        clear();
        release( alloc(24) ); // Alloc an initial buffer and mark as unused.
    }

    ValueType *alloc(int numElements)
    {
        numElements = de::max(4, numElements); // Useful minimum.

        Item *found = 0;
        for(int i = 0; i < items.count(); ++i)
        {
            if(items[i]->inUse) continue;

            if(items[i]->buffer.size() >= numElements)
            {
                // Use this one.
                found = items[i];
                break;
            }
        }

        if(!found)
        {
            // Add another.
            found = new Item(numElements);
            items << found;
        }

        found->inUse = true;
        return found->buffer.data();
    }

    void release(ValueType *data)
    {
        if(!data) return;
        if(Item *item = findItemForBufferData(data))
        {
            item->inUse = false;
            return;
        }
        LOGDEV_GL_WARNING("BufferPool::release: Dangling ptr!");
    }

    void devPrint()
    {
        LOGDEV_GL_MSG("BufferPool {%p} %i buffers:")
                << this << items.count();
        for(int i = 0; i < items.count(); ++i)
        {
            Item *item = items[i];
            LOGDEV_GL_MSG(_E(m) "%-4i size:%i %s")
                    << i << item->buffer.size()
                    << (item->inUse? "Used" : "Unused");
        }
    }

private:
    struct Item
    {
        bool inUse;
        Values buffer;
        Item(int numElements) : inUse(false), buffer(numElements)
        {}
    };

    Item *findItemForBufferData(ValueType *data)
    {
        if(!data) return 0;
        for(int i = 0; i < items.count(); ++i)
        {
            if(items[i]->buffer.data() == data)
            {
                return items[i];
            }
        }
        return 0;
    }

    typedef QList<Item *> Items;
    Items items;
};

/**
 * Vertex buffer for the map renderer. @todo Replace with GLBuffer
 */
template <typename VertexType>
class VBufT : public QVector<VertexType>
{
public:
    typedef VertexType Type;
    typedef de::duint16 Index;
    typedef QVector<Index> Indices;

public:
    VBufT() : _curElem(0) {}

    void clear() { // shadow QVector::clear()
        _curElem = 0;
        QVector::clear();
    }

    void reset() {
        _curElem = 0;
    }

    Index reserveElements(Index numElements, Index *indices = 0) {
        Index const base = _curElem;
        _curElem += de::max(numElements, Index(0));
        if(_curElem > size())
        {
            resize(_curElem);
        }
        if(indices)
        {
            for(int i = 0; i < numElements; ++i)
            {
                indices[i] = base + i;
            }
        }
        return base;
    }

    inline Index reserveElements(Index numElements, Indices &indices) {
        return reserveElements(numElements, indices.data());
    }

private:
    Index _curElem;
};

/**
 * World vertex buffer for RenderSystem.
 *
 * @ingroup render
 */
struct WorldVBuf : public VBufT<de::Vertex3Tex3Rgba>
{
    // Texture coordinate array indices:
    enum TC {
        PrimaryTex,  ///< Primary coords.
        InterTex,    ///< Blend-target coords.
        ModTex       ///< Modulation coords.
    };
};

/**
 * Renderer subsystems, draw lists, etc...
 *
 * @ingroup render
 */
class RenderSystem : public de::System
{
public:
    // Pooled work buffers:
    typedef BufferPoolT<WorldVBuf::Index> IndicePool;

public:
    RenderSystem();

    de::GLShaderBank &shaders();
    de::ImageBank &images();

    SettingsRegister &settings();
    SettingsRegister &appearanceSettings();

    /**
     * Returns the special "missing" material to be used where a surface lacks one.
     */
    Material &missingMaterial() const;

    /**
     * Provides access to the central world vertex buffer.
     */
    WorldVBuf &worldVBuf();

    void clearDrawLists();

    void resetDrawLists();

    DrawLists &drawLists();

    /**
     * Returns the vertex indice pool.
     */
    IndicePool &indicePool() const;

    /**
     * @note Should be called at the start of each map.
     */
    void resetPools();

    void printPoolInfo();

    // System.
    void timeChanged(de::Clock const &);

public:
    /**
     * Register the console commands, variables, etc..., of this module.
     */
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

#endif // CLIENT_RENDERSYSTEM_H
