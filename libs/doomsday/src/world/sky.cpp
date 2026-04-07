/** @file sky.cpp  Sky behavior logic for the world system.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/world/sky.h"

#include <doomsday/res/texturemanifest.h>
#include <doomsday/world/materials.h>
#include <cmath>
#include <de/log.h>

DE_STATIC_STRING(DEFAULT_SKY_SPHERE_MATERIAL, "Textures:SKY1")

using namespace de;

namespace world {

const int Sky::NUM_LAYERS = 2;

DE_PIMPL_NOREF(Sky::Layer)
{
    bool active         = false;
    bool masked         = false;
    Material *material = nullptr;
    float offset       = 0;
    float fadeOutLimit = 0;

    Sky &sky;
    Impl(Sky &sky) : sky(sky) {}

    DE_PIMPL_AUDIENCE(ActiveChange)
    DE_PIMPL_AUDIENCE(MaskedChange)
    DE_PIMPL_AUDIENCE(MaterialChange)
};

DE_AUDIENCE_METHOD(Sky::Layer, ActiveChange)
DE_AUDIENCE_METHOD(Sky::Layer, MaskedChange)
DE_AUDIENCE_METHOD(Sky::Layer, MaterialChange)

Sky::Layer::Layer(Sky &sky, Material *material) : d(new Impl(sky))
{
    setMaterial(material);
}

Sky &Sky::Layer::sky() const
{
    return d->sky;
}

bool Sky::Layer::isActive() const
{
    return d->active;
}

void Sky::Layer::setActive(bool yes)
{
    if(d->active != yes)
    {
        d->active = yes;
        DE_NOTIFY(ActiveChange, i) i->skyLayerActiveChanged(*this);
    }
}

bool Sky::Layer::isMasked() const
{
    return d->masked;
}

void Sky::Layer::setMasked(bool yes)
{
    if(d->masked != yes)
    {
        d->masked = yes;
        DE_NOTIFY(MaskedChange, i) i->skyLayerMaskedChanged(*this);
    }
}

Material *Sky::Layer::material() const
{
    return d->material;
}

void Sky::Layer::setMaterial(Material *newMaterial)
{
    if(d->material != newMaterial)
    {
        d->material = newMaterial;
        DE_NOTIFY(MaterialChange, i) i->skyLayerMaterialChanged(*this);
    }
}

float Sky::Layer::offset() const
{
    return d->offset;
}

void Sky::Layer::setOffset(float newOffset)
{
    d->offset = newOffset;
}

float Sky::Layer::fadeOutLimit() const
{
    return d->fadeOutLimit;
}

void Sky::Layer::setFadeoutLimit(float newLimit)
{
    d->fadeOutLimit = newLimit;
}

DE_PIMPL(Sky)
{
    using Layers = List<std::unique_ptr<Layer>>;

    Layers layers;

    const Record *def    = nullptr; ///< Sky definition.
    float height        = 1;
    float horizonOffset = 0;

    Impl(Public *i) : Base(i)
    {
        for (int i = 0; i < NUM_LAYERS; ++i)
        {
            layers.emplace_back(new Layer(self()));
        }
    }

    ~Impl()
    {
        DE_NOTIFY_PUBLIC(Deletion, i) i->skyBeingDeleted(self());
    }

    DE_PIMPL_AUDIENCE(Deletion)
    DE_PIMPL_AUDIENCE(HeightChange)
    DE_PIMPL_AUDIENCE(HorizonOffsetChange)
};

DE_AUDIENCE_METHOD(Sky, Deletion)
DE_AUDIENCE_METHOD(Sky, HeightChange)
DE_AUDIENCE_METHOD(Sky, HorizonOffsetChange)

Sky::Sky(const defn::Sky *definition)
    : MapElement(DMU_SKY)
    , d(new Impl(this))
{
    configure(definition);
}

void Sky::configure(const defn::Sky *def)
{
    LOG_AS("Sky");

    // Remember the definition for this configuration (if any).
    d->def = def? def->accessedRecordPtr() : 0;

    setHeight(def? def->getf("height") : DEFAULT_SKY_HEIGHT);
    setHorizonOffset(def? def->getf("horizonOffset") : DEFAULT_SKY_HORIZON_OFFSET);

    for(int i = 0; i < d->layers.count(); ++i)
    {
        const Record *lyrDef = def? &def->layer(i) : 0;
        Layer &lyr = *d->layers[i];

        lyr.setMasked(lyrDef? (lyrDef->geti("flags") & SLF_MASK) : false);
        lyr.setOffset(lyrDef? lyrDef->getf("offset") : DEFAULT_SKY_SPHERE_XOFFSET);
        lyr.setFadeoutLimit(lyrDef ? lyrDef->getf("colorLimit") : DEFAULT_SKY_SPHERE_FADEOUT_LIMIT);

        const res::Uri matUri = lyrDef ? res::makeUri(lyrDef->gets("material"))
                                       : res::makeUri(DEFAULT_SKY_SPHERE_MATERIAL());
        world::Material *mat = 0;
        try
        {
            mat = world::Materials::get().materialPtr(matUri);
        }
        catch(const world::MaterialManifest::MissingMaterialError &er)
        {
            // Log if a material is specified but otherwise ignore this error.
            if(lyrDef)
            {
                LOG_RES_WARNING(er.asText() + ". Unknown material \"%s\" in definition layer %i, using default")
                        << matUri << i;
            }
        }
        lyr.setMaterial(mat);

        lyr.setActive(lyrDef? (lyrDef->geti("flags") & SLF_ENABLE) : i == 0);
    }
}

const Record *Sky::def() const
{
    return d->def;
}

int Sky::layerCount() const
{
    return d->layers.count();
}

bool Sky::hasLayer(int layerIndex) const
{
    return !d->layers.isEmpty() && layerIndex < d->layers.count();
}

Sky::Layer *Sky::layerPtr(int layerIndex) const
{
    if (hasLayer(layerIndex)) return d->layers.at(layerIndex).get();
    return nullptr;
}

Sky::Layer &Sky::layer(int layerIndex)
{
    if (Layer *layer = layerPtr(layerIndex)) return *layer;
    /// @throw MissingLayerError Unknown layerIndex specified,
    throw MissingLayerError("Sky::layer", stringf("Unknown layer #%i", layerIndex));
}

const Sky::Layer &Sky::layer(int layerIndex) const
{
    if (Layer *layer = layerPtr(layerIndex)) return *layer;
    /// @throw MissingLayerError Unknown layerIndex specified,
    throw MissingLayerError("Sky::layer", stringf("Unknown layer #%i", layerIndex));
}

LoopResult Sky::forAllLayers(const std::function<LoopResult (Layer &)>& func)
{
    for (auto &layer : d->layers)
    {
        if (auto result = func(*layer)) return result;
    }
    return LoopContinue;
}

LoopResult Sky::forAllLayers(const std::function<LoopResult (const Layer &)>& func) const
{
    for (const auto &layer : d->layers)
    {
        if (auto result = func(*layer)) return result;
    }
    return LoopContinue;
}

float Sky::height() const
{
    return d->height;
}

void Sky::setHeight(float newHeight)
{
    newHeight = de::clamp(0.f, newHeight, 1.f);
    if(!de::fequal(d->height, newHeight))
    {
        d->height = newHeight;
        DE_NOTIFY(HeightChange, i) i->skyHeightChanged(*this);
    }
}

float Sky::horizonOffset() const
{
    return d->horizonOffset;
}

void Sky::setHorizonOffset(float newOffset)
{
    if(!de::fequal(d->horizonOffset, newOffset))
    {
        d->horizonOffset = newOffset;
        DE_NOTIFY(HorizonOffsetChange, i) i->skyHorizonOffsetChanged(*this);
    }
}

int Sky::property(DmuArgs &args) const
{
    LOG_AS("Sky");

    switch(args.prop)
    {
    case DMU_FLAGS: {
        int flags = 0;
        if(layer(0).isActive()) flags |= SKYF_LAYER0_ENABLED;
        if(layer(1).isActive()) flags |= SKYF_LAYER1_ENABLED;

        args.setValue(DDVT_INT, &flags, 0);
        break; }

    case DMU_HEIGHT:
        args.setValue(DDVT_FLOAT, &d->height, 0);
        break;

    /*case DMU_HORIZONOFFSET:
        args.setValue(DDVT_FLOAT, &d->horizonOffset, 0);
        break;*/

    default: return MapElement::property(args);
    }

    return false; // Continue iteration.
}

int Sky::setProperty(const DmuArgs &args)
{
    LOG_AS("Sky");

    switch(args.prop)
    {
    case DMU_FLAGS: {
        int flags = 0;
        if(layer(0).isActive()) flags |= SKYF_LAYER0_ENABLED;
        if(layer(1).isActive()) flags |= SKYF_LAYER1_ENABLED;

        args.value(DDVT_INT, &flags, 0);

        layer(0).setActive(flags & SKYF_LAYER0_ENABLED);
        layer(1).setActive(flags & SKYF_LAYER1_ENABLED);
        break; }

    case DMU_HEIGHT: {
        float newHeight = d->height;
        args.value(DDVT_FLOAT, &newHeight, 0);

        setHeight(newHeight);
        break; }

    /*case DMU_HORIZONOFFSET: {
        float newOffset = d->horizonOffset;
        args.value(DDVT_FLOAT, &d->horizonOffset, 0);

        setHorizonOffset(newOffset);
        break; }*/

    default: return MapElement::setProperty(args);
    }

    return false; // Continue iteration.
}

}  // namespace world
