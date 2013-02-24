/** @file material.cpp Logical Material.
 *
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "def_main.h"

#include "api_map.h"
#include "audio/s_environ.h"
#include "map/r_world.h"

#include <de/math.h>
#include <QtAlgorithms>

#include "resource/material.h"

using namespace de;

Material::Layer::Layer()
{}

Material::Layer::~Layer()
{
    qDeleteAll(stages_);
}

static Texture *findTextureForLayerStage(ded_material_layer_stage_t const &def)
{
    try
    {
        if(def.texture)
        {
            return &App_Textures().find(*reinterpret_cast<de::Uri *>(def.texture)).texture();
        }
    }
    catch(TextureManifest::MissingTextureError const &)
    {} // Ignore this error.
    catch(Textures::NotFoundError const &)
    {} // Ignore this error.
    return 0;
}

Material::Layer::Stage *Material::Layer::Stage::fromDef(ded_material_layer_stage_t const &def)
{
    Texture *texture = findTextureForLayerStage(def);
    return new Stage(texture, def.tics, def.variance, def.glowStrength,
                     def.glowStrengthVariance, Vector2f(def.texOrigin));
}

Material::Layer *Material::Layer::fromDef(ded_material_layer_t const &layerDef)
{
    Layer *layer = new Layer();
    for(int i = 0; i < layerDef.stageCount.num; ++i)
    {
        layer->stages_.push_back(Stage::fromDef(layerDef.stages[i]));
    }
    return layer;
}

int Material::Layer::stageCount() const
{
    return stages_.count();
}

Material::Layer::Stages const &Material::Layer::stages() const
{
    return stages_;
}

Material::DetailLayer::DetailLayer()
{}

Material::DetailLayer::~DetailLayer()
{
    qDeleteAll(stages_);
}

static Texture *findTextureForDetailLayerStage(ded_detail_stage_t const &def)
{
    try
    {
        if(def.texture)
        {
            return &App_Textures().scheme("Details")
                        .findByResourceUri(*reinterpret_cast<de::Uri *>(def.texture)).texture();
        }
    }
    catch(TextureManifest::MissingTextureError const &)
    {} // Ignore this error.
    catch(Textures::Scheme::NotFoundError const &)
    {} // Ignore this error.
    return 0;
}

Material::DetailLayer::Stage *Material::DetailLayer::Stage::fromDef(ded_detail_stage_t const &def)
{
    Texture *texture = findTextureForDetailLayerStage(def);

    return new Stage(def.tics, def.variance, texture,
                     def.scale, def.strength, def.maxDistance);
}

Material::DetailLayer *Material::DetailLayer::fromDef(ded_detailtexture_t const &layerDef)
{
    DetailLayer *layer = new DetailLayer();
    // Only the one stage.
    layer->stages_.push_back(Stage::fromDef(layerDef.stage));
    return layer;
}

int Material::DetailLayer::stageCount() const
{
    return stages_.count();
}

Material::DetailLayer::Stages const &Material::DetailLayer::stages() const
{
    return stages_;
}

Material::ShineLayer::ShineLayer()
{}

Material::ShineLayer::~ShineLayer()
{
    qDeleteAll(stages_);
}

static Texture *findTextureForShineLayerStage(ded_shine_stage_t const &def, bool findMask)
{
    try
    {
        if(findMask)
        {
            if(def.maskTexture)
            {
                return &App_Textures().scheme("Masks")
                            .findByResourceUri(*reinterpret_cast<de::Uri *>(def.maskTexture)).texture();
            }
        }
        else
        {
            if(def.texture)
            {
                return &App_Textures().scheme("Reflections")
                            .findByResourceUri(*reinterpret_cast<de::Uri *>(def.texture)).texture();
            }
        }
    }
    catch(TextureManifest::MissingTextureError const &)
    {} // Ignore this error.
    catch(Textures::Scheme::NotFoundError const &)
    {} // Ignore this error.
    return 0;
}

Material::ShineLayer::Stage *Material::ShineLayer::Stage::fromDef(ded_shine_stage_t const &def)
{
    Texture *texture     = findTextureForShineLayerStage(def, false/*not mask*/);
    Texture *maskTexture = findTextureForShineLayerStage(def, true/*mask*/);

    return new Stage(def.tics, def.variance, texture, maskTexture,
                     def.blendMode, def.shininess, Vector3f(def.minColor),
                     QSizeF(def.maskWidth, def.maskHeight));
}

Material::ShineLayer *Material::ShineLayer::fromDef(ded_reflection_t const &layerDef)
{
    ShineLayer *layer = new ShineLayer();
    // Only the one stage.
    layer->stages_.push_back(Stage::fromDef(layerDef.stage));
    return layer;
}

int Material::ShineLayer::stageCount() const
{
    return stages_.count();
}

Material::ShineLayer::Stages const &Material::ShineLayer::stages() const
{
    return stages_;
}

Material::Decoration::Stage *Material::Decoration::Stage::fromDef(ded_decorlight_stage_t const &def)
{
    Texture *upTexture    = R_FindTextureByResourceUri("Lightmaps", reinterpret_cast<de::Uri *>(def.up));
    Texture *downTexture  = R_FindTextureByResourceUri("Lightmaps", reinterpret_cast<de::Uri *>(def.down));
    Texture *sidesTexture = R_FindTextureByResourceUri("Lightmaps", reinterpret_cast<de::Uri *>(def.sides));

    Texture *flareTexture = 0;
    int sysFlareIdx = def.sysFlareIdx;

    if(def.flare && !Uri_IsEmpty(def.flare))
    {
        de::Uri const *resourceUri = reinterpret_cast<de::Uri *>(def.flare);

        // Select a system flare by numeric identifier?
        if(resourceUri->path().length() == 1 &&
           resourceUri->path().toStringRef().first().isDigit())
        {
            sysFlareIdx = resourceUri->path().toStringRef().first().digitValue();
        }
        else
        {
            flareTexture = R_FindTextureByResourceUri("Flaremaps", resourceUri);
        }
    }

    return new Stage(def.tics, def.variance, Vector2f(def.pos), def.elevation,
                     Vector3f(def.color), def.radius, def.haloRadius,
                     Stage::LightLevels(def.lightLevels),
                     upTexture, downTexture, sidesTexture, flareTexture, def.sysFlareIdx);
}

Material::Decoration::Decoration()
    : patternSkip_(0, 0), patternOffset_(0, 0)
{}

Material::Decoration::Decoration(Vector2i const &_patternSkip, Vector2i const &_patternOffset)
    : patternSkip_(_patternSkip), patternOffset_(_patternOffset)
{}

Material::Decoration::~Decoration()
{
    qDeleteAll(stages_);
}

Material::Decoration *Material::Decoration::fromDef(ded_material_decoration_t &def)
{
    Decoration *dec = new Decoration(Vector2i(def.patternSkip),
                                     Vector2i(def.patternOffset));
    for(int i = 0; i < def.stageCount.num; ++i)
    {
        dec->stages_.push_back(Stage::fromDef(def.stages[i]));
    }
    return dec;
}

Material::Decoration *Material::Decoration::fromDef(ded_decoration_t &def)
{
    Decoration *dec = new Decoration(Vector2i(def.patternSkip),
                                     Vector2i(def.patternOffset));
    // Only the one stage.
    dec->stages_.push_back(Stage::fromDef(def.stage));
    return dec;
}

Vector2i const &Material::Decoration::patternSkip() const
{
    return patternSkip_;
}

Vector2i const &Material::Decoration::patternOffset() const
{
    return patternOffset_;
}

int Material::Decoration::stageCount() const
{
    return stages_.count();
}

Material::Decoration::Stages const &Material::Decoration::stages() const
{
    return stages_;
}

DENG2_PIMPL(Material)
{
    /// Manifest derived to yield the material.
    MaterialManifest &manifest;

#ifdef __CLIENT__
    /// Set of use-case/context variant instances.
    Material::Variants variants;
#endif

    /// Environment audio class.
    AudioEnvironmentClass envClass;

    /// World dimensions in map coordinate space units.
    QSize dimensions;

    /// @see materialFlags
    Material::Flags flags;

    /// Layers.
    Material::Layers layers;
    Material::DetailLayer *detailLayer;
    Material::ShineLayer *shineLayer;

    /// Decorations (will be projected into the map relative to a surface).
    Material::Decorations decorations;

    /// @c false= the material is no longer valid.
    bool valid;

    Instance(Public *i, MaterialManifest &_manifest) : Base(i),
        manifest(_manifest),
        envClass(AEC_UNKNOWN),
        flags(0),
        detailLayer(0),
        shineLayer(0),
        valid(true)
    {}

    ~Instance()
    {
#ifdef __CLIENT__
        clearVariants();
#endif
        clearDecorations();
        clearLayers();
    }

#ifdef __CLIENT__
    void clearVariants()
    {
        while(!variants.isEmpty())
        {
             delete variants.takeFirst();
        }
        variants.clear();
    }
#endif // __CLIENT__

    void clearLayers()
    {
        qDeleteAll(layers);
        layers.clear();

        if(detailLayer) delete detailLayer;
        if(shineLayer) delete shineLayer;
    }

    void clearDecorations()
    {
        qDeleteAll(decorations);
        decorations.clear();
    }
};

Material::Material(MaterialManifest &_manifest, ded_material_t const *def)
    : de::MapElement(DMU_MATERIAL), d(new Instance(this, _manifest))
{
    DENG_ASSERT(def);
    if(def->flags & MATF_NO_DRAW) d->flags |= NoDraw;
    if(def->flags & MATF_SKYMASK) d->flags |= SkyMask;
    d->dimensions = QSize(de::max(0, def->width), de::max(0, def->height));

    for(int i = 0; i < DED_MAX_MATERIAL_LAYERS; ++i)
    {
        Layer *layer = Layer::fromDef(def->layers[i]);
        d->layers.push_back(layer);

        foreach(Layer::Stage *stageDef, layer->stages())
        {
            if(!stageDef->texture) continue;

            de::Uri textureUri(stageDef->texture->manifest().composeUri());
            ded_detailtexture_t *detailDef = Def_GetDetailTex(reinterpret_cast<uri_s *>(&textureUri)/*, UNKNOWN VALUE, manifest.isCustom()*/);
            if(detailDef)
            {
                // Time to allocate the detail layer?
                if(!d->detailLayer)
                {
                    d->detailLayer = DetailLayer::fromDef(*detailDef);
                }
                // Add stages.
            }

            ded_reflection_t *reflectionDef = Def_GetReflection(reinterpret_cast<uri_s *>(&textureUri)/*, UNKNOWN VALUE, manifest.isCustom()*/);
            if(reflectionDef)
            {
                // Time to allocate the shine layer?
                if(!d->shineLayer)
                {
                    d->shineLayer = ShineLayer::fromDef(*reflectionDef);
                }
                // Add stages.
            }
        }
    }
}

Material::~Material()
{
    delete d;
}

MaterialManifest &Material::manifest() const
{
    return d->manifest;
}

void Material::ticker(timespan_t time)
{
#ifdef __CLIENT__
    foreach(Variant *variant, d->variants)
    {
        variant->ticker(time);
    }
#else
    DENG2_UNUSED(time);
#endif
}

QSize const &Material::dimensions() const
{
    return d->dimensions;
}

void Material::setDimensions(QSize const &newDimensions)
{
    if(d->dimensions != newDimensions)
    {
        d->dimensions = newDimensions;
        R_UpdateMapSurfacesOnMaterialChange(this);
    }
}

void Material::setWidth(int newWidth)
{
    if(d->dimensions.width() != newWidth)
    {
        d->dimensions.setWidth(newWidth);
        R_UpdateMapSurfacesOnMaterialChange(this);
    }
}

void Material::setHeight(int newHeight)
{
    if(d->dimensions.height() != newHeight)
    {
        d->dimensions.setHeight(newHeight);
        R_UpdateMapSurfacesOnMaterialChange(this);
    }
}

Material::Flags Material::flags() const
{
    return d->flags;
}

void Material::setFlags(Material::Flags flagsToChange, bool set)
{
    if(set)
    {
        d->flags |= flagsToChange;
    }
    else
    {
        d->flags &= ~flagsToChange;
    }
}

bool Material::isAnimated() const
{
    foreach(Layer *layer, d->layers)
    {
        if(layer->isAnimated()) return true;
    }
    return false; // Not at all.
}

bool Material::isDetailed() const
{
    return !!d->detailLayer;
}

bool Material::isShiny() const
{
    return !!d->shineLayer;
}

bool Material::hasGlow() const
{
    foreach(Layer *layer, d->layers)
    foreach(Layer::Stage *stage, layer->stages())
    {
        if(stage->glowStrength > .0001f) return true;
    }
    return false;
}

AudioEnvironmentClass Material::audioEnvironment() const
{
    if(isDrawable()) return d->envClass;
    return AEC_UNKNOWN;
}

void Material::setAudioEnvironment(AudioEnvironmentClass envClass)
{
    d->envClass = envClass;
}

Material::Layers const &Material::layers() const
{
    return d->layers;
}

Material::DetailLayer const &Material::detailLayer() const
{
    if(isDetailed())
    {
        DENG_ASSERT(d->detailLayer);
        return *d->detailLayer;
    }
    /// @throw Material::UnknownLayerError Invalid layer reference.
    throw UnknownLayerError("Material::detailLayer", "Material has no details layer");
}

Material::ShineLayer const &Material::shineLayer() const
{
    if(isShiny())
    {
        DENG_ASSERT(d->shineLayer);
        return *d->shineLayer;
    }
    /// @throw Material::UnknownLayerError Invalid layer reference.
    throw UnknownLayerError("Material::shineLayer", "Material has no shine layer");
}

void Material::addDecoration(Material::Decoration &decor)
{
    d->decorations.push_back(&decor);
}

Material::Decorations const &Material::decorations() const
{
    return d->decorations;
}

#ifdef __CLIENT__

Material::Variants const &Material::variants() const
{
    return d->variants;
}

Material::Variant *Material::chooseVariant(Material::VariantSpec const &spec, bool canCreate)
{
    foreach(Variant *variant, d->variants)
    {
        VariantSpec const &cand = variant->spec();
        if(cand.compare(spec))
        {
            // This will do fine.
            return variant;
        }
    }

    if(!canCreate) return 0;

    d->variants.push_back(new Variant(*this, spec));
    return d->variants.back();
}

void Material::clearVariants()
{
    d->clearVariants();
}

#endif // __CLIENT__

int Material::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_FLAGS: {
        short flags_ = flags();
        DMU_GetValue(DMT_MATERIAL_FLAGS, &flags_, &args, 0);
        break; }

    case DMU_WIDTH: {
        int width_ = width();
        DMU_GetValue(DMT_MATERIAL_WIDTH, &width_, &args, 0);
        break; }

    case DMU_HEIGHT: {
        int height_ = height();
        DMU_GetValue(DMT_MATERIAL_HEIGHT, &height_, &args, 0);
        break; }

    default:
        /// @throw UnknownPropertyError  The requested property does not exist.
        throw UnknownPropertyError("Material::property", QString("Property '%1' is unknown").arg(DMU_Str(args.prop)));
    }
    return false; // Continue iteration.
}

int Material::setProperty(setargs_t const &args)
{
    /// @throw WritePropertyError  The requested property is not writable.
    throw WritePropertyError("Material::setProperty", QString("Property '%1' is not writable").arg(DMU_Str(args.prop)));
}

bool Material::isValid() const
{
    return d->valid;
}

void Material::markValid(bool yes)
{
    if(d->valid == yes) return;
    d->valid = yes;
}
