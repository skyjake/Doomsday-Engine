/** @file gl_model.cpp  3D Model Renderable.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de_platform.h"
#include "gl/gl_model.h"

#include "Texture"
#include "TextureManifest"
#include <de/Range>
#include <QtAlgorithms>

using namespace de;

bool Model::DetailLevel::hasVertex(int number) const
{
    return model._vertexUsage.testBit(number * model.lodCount() + level);
}

void Model::Frame::bounds(Vector3f &retMin, Vector3f &retMax) const
{
    retMin = min;
    retMax = max;
}

float Model::Frame::horizontalRange(float *top, float *bottom) const
{
    *top    = max.y;
    *bottom = min.y;
    return max.y - min.y;
}

DENG2_PIMPL(Model)
{
    uint modelId; ///< Unique id of the model (in the repository).
    Flags flags;
    Skins skins;
    Frames frames;

    Instance(Public *i)
        : Base(i)
        , modelId(0)
    {}

    ~Instance()
    {
        self.clearAllFrames();
    }
};

Model::Model(uint modelId, Flags flags)
    : _numVertices(0)
    , d(new Instance(this))
{
    setModelId(modelId);
    setFlags(flags, de::ReplaceFlags);
}

uint Model::modelId() const
{
    return d->modelId;
}

void Model::setModelId(uint newModelId)
{
    d->modelId = newModelId;
}

Model::Flags Model::flags() const
{
    return d->flags;
}

void Model::setFlags(Model::Flags flagsToChange, FlagOp operation)
{
    applyFlagOperation(d->flags, flagsToChange, operation);
}

int Model::frameNumber(String name) const
{
    if(!name.isEmpty())
    {
        for(int i = 0; i < d->frames.count(); ++i)
        {
            Frame *frame = d->frames.at(i);
            if(!frame->name.compareWithoutCase(name))
                return i;
        }
    }
    return -1; // Not found.
}

Model::Frame &Model::frame(int number) const
{
    if(hasFrame(number))
    {
        return *d->frames.at(number);
    }
    throw MissingFrameError("Model::frame", "Invalid frame number " + String::number(number) + ", valid range is " + Rangei(0, d->frames.count()).asText());
}

void Model::addFrame(Frame *newFrame)
{
    if(!newFrame) return;
    if(!d->frames.contains(newFrame))
    {
        d->frames.append(newFrame);
    }
}

Model::Frames const &Model::frames() const
{
    return d->frames;
}

void Model::clearAllFrames()
{
    qDeleteAll(d->frames);
    d->frames.clear();
}

int Model::skinNumber(String name) const
{
    if(!name.isEmpty())
    {
        for(int i = 0; i < d->skins.count(); ++i)
        {
            Skin const &skin = d->skins.at(i);
            if(!skin.name.compareWithoutCase(name))
                return i;
        }
    }
    return -1; // Not found.
}

Model::Skin &Model::skin(int number) const
{
    if(hasSkin(number))
    {
        return const_cast<Skin &>(d->skins.at(number));
    }
    throw MissingSkinError("Model::skin", "Invalid skin number " + String::number(number) + ", valid range is " + Rangei(0, d->skins.count()).asText());
}

Model::Skin &Model::newSkin(String name)
{
    if(int index = skinNumber(name) > 0)
    {
        return skin(index);
    }
    d->skins.append(ModelSkin(name));
    return d->skins.last();
}

Model::Skins const &Model::skins() const
{
    return d->skins;
}

void Model::clearAllSkins()
{
    d->skins.clear();
}

Model::DetailLevel &Model::lod(int level) const
{
    if(hasLod(level))
    {
        return *_lods.at(level);
    }
    throw MissingDetailLevelError("Model::lod", "Invalid detail level " + String::number(level) + ", valid range is " + Rangei(0, _lods.count()).asText());
}

Model::DetailLevels const &Model::lods() const
{
    return _lods;
}

Model::Primitives const &Model::primitives() const
{
    return lod(0).primitives;
}

int Model::vertexCount() const
{
    return _numVertices;
}
