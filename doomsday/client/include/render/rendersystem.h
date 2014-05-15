/** @file rendersystem.h  Render subsystem.
 *
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include <QVector>
#include <de/System>
#include <de/Vector>
#include <de/GLBuffer>
#include <de/GLShaderBank>
#include <de/ImageBank>
#include "settingsregister.h"

class DrawLists;

/**
 * Vertex buffer for the map renderer. @todo Replace with GLBuffer
 */
template <typename VertexType>
class VBufT : public QVector<VertexType>
{
public:
    typedef VertexType Type;

public:
    VBufT() : _curElem(0) {}

    void clear() { // shadow QVector::clear()
        _curElem = 0;
        QVector::clear();
    }

    void reset() {
        _curElem = 0;
    }

    int reserveElements(int numElements) {
        int const base = _curElem;
        _curElem += de::max(numElements, 0);
        if(_curElem > size())
        {
            resize(_curElem);
        }
        return base;
    }

private:
    int _curElem;
};

/**
 * World vertex buffer for RenderSystem.
 *
 * @ingroup render
 */
struct WorldVBuf : public VBufT<de::Vertex3Tex3Rgba>
{
    // Texture coordinate array indices:
    enum {
        TCA_MAIN,   ///< Main texture.
        TCA_BLEND,  ///< Blendtarget texture.
        TCA_LIGHT   ///< Dynlight texture.
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
    RenderSystem();

    de::GLShaderBank &shaders();
    de::ImageBank &images();

    SettingsRegister &settings();
    SettingsRegister &appearanceSettings();

    /**
     * Provides access to the central world vertex buffer.
     */
    WorldVBuf &buffer();

    void clearDrawLists();

    void resetDrawLists();

    DrawLists &drawLists();

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
