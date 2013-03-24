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
#include "r_util.h" // R_NameForBlendMode
#include <de/math.h>
#include <QtAlgorithms>

#include "resource/material.h"

using namespace de;

Material::Layer::Layer()
{}

Material::Layer::~Layer()
{
    qDeleteAll(_stages);
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
        layer->_stages.push_back(Stage::fromDef(layerDef.stages[i]));
    }
    return layer;
}

int Material::Layer::stageCount() const
{
    return _stages.count();
}

int Material::Layer::addStage(Material::Layer::Stage const &stageToCopy)
{
    _stages.push_back(new Stage(stageToCopy));
    return _stages.count() - 1;
}

Material::Layer::Stages const &Material::Layer::stages() const
{
    return _stages;
}

Material::DetailLayer::DetailLayer()
{}

Material::DetailLayer::~DetailLayer()
{
    qDeleteAll(_stages);
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
    catch(TextureScheme::NotFoundError const &)
    {} // Ignore this error.
    return 0;
}

Material::DetailLayer::Stage *Material::DetailLayer::Stage::fromDef(ded_detail_stage_t const &def)
{
    Texture *texture = findTextureForDetailLayerStage(def);

    return new Stage(texture, def.tics, def.variance,
                     def.scale, def.strength, def.maxDistance);
}

Material::DetailLayer *Material::DetailLayer::fromDef(ded_detailtexture_t const &layerDef)
{
    DetailLayer *layer = new DetailLayer();
    // Only the one stage.
    layer->_stages.push_back(Stage::fromDef(layerDef.stage));
    return layer;
}

int Material::DetailLayer::stageCount() const
{
    return _stages.count();
}

int Material::DetailLayer::addStage(Material::DetailLayer::Stage const &stageToCopy)
{
    _stages.push_back(new Stage(stageToCopy));
    return _stages.count() - 1;
}

Material::DetailLayer::Stages const &Material::DetailLayer::stages() const
{
    return _stages;
}

Material::ShineLayer::ShineLayer()
{}

Material::ShineLayer::~ShineLayer()
{
    qDeleteAll(_stages);
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
    catch(TextureScheme::NotFoundError const &)
    {} // Ignore this error.
    return 0;
}

Material::ShineLayer::Stage *Material::ShineLayer::Stage::fromDef(ded_shine_stage_t const &def)
{
    Texture *texture     = findTextureForShineLayerStage(def, false/*not mask*/);
    Texture *maskTexture = findTextureForShineLayerStage(def, true/*mask*/);

    return new Stage(texture, def.tics, def.variance, maskTexture,
                     def.blendMode, def.shininess, Vector3f(def.minColor),
                     Vector2f(def.maskWidth, def.maskHeight));
}

Material::ShineLayer *Material::ShineLayer::fromDef(ded_reflection_t const &layerDef)
{
    ShineLayer *layer = new ShineLayer();
    // Only the one stage.
    layer->_stages.push_back(Stage::fromDef(layerDef.stage));
    return layer;
}

int Material::ShineLayer::stageCount() const
{
    return _stages.count();
}

int Material::ShineLayer::addStage(Material::ShineLayer::Stage const &stageToCopy)
{
    _stages.push_back(new Stage(stageToCopy));
    return _stages.count() - 1;
}

Material::ShineLayer::Stages const &Material::ShineLayer::stages() const
{
    return _stages;
}

#ifdef __CLIENT__

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
                     upTexture, downTexture, sidesTexture, flareTexture, sysFlareIdx);
}

String Material::Decoration::Stage::LightLevels::asText() const
{
    return String("(min:%1 max:%2)")
               .arg(min, 0, 'g', 2)
               .arg(max, 0, 'g', 2);
}

Material::Decoration::Decoration()
    : _patternSkip(0, 0), _patternOffset(0, 0)
{}

Material::Decoration::Decoration(Vector2i const &_patternSkip, Vector2i const &_patternOffset)
    : _patternSkip(_patternSkip), _patternOffset(_patternOffset)
{}

Material::Decoration::~Decoration()
{
    qDeleteAll(_stages);
}

Material::Decoration *Material::Decoration::fromDef(ded_material_decoration_t const &def)
{
    Decoration *dec = new Decoration(Vector2i(def.patternSkip),
                                     Vector2i(def.patternOffset));
    for(int i = 0; i < def.stageCount.num; ++i)
    {
        dec->_stages.push_back(Stage::fromDef(def.stages[i]));
    }
    return dec;
}

Material::Decoration *Material::Decoration::fromDef(ded_decoration_t const &def)
{
    Decoration *dec = new Decoration(Vector2i(def.patternSkip),
                                     Vector2i(def.patternOffset));
    // Only the one stage.
    dec->_stages.push_back(Stage::fromDef(def.stage));
    return dec;
}

Vector2i const &Material::Decoration::patternSkip() const
{
    return _patternSkip;
}

Vector2i const &Material::Decoration::patternOffset() const
{
    return _patternOffset;
}

int Material::Decoration::stageCount() const
{
    return _stages.count();
}

Material::Decoration::Stages const &Material::Decoration::stages() const
{
    return _stages;
}

#endif // __CLIENT__

DENG2_PIMPL(Material)
{
    /// Manifest derived to yield the material.
    MaterialManifest &manifest;

#ifdef __CLIENT__
    /// Set of context animation states.
    Material::Animations animations;
    bool animationsAreDirty;

    /// Set of use-case/context variant instances.
    Material::Variants variants;
#endif

    /// Environment audio class.
    AudioEnvironmentClass envClass;

    /// World dimensions in map coordinate space units.
    Vector2i dimensions;

    /// @ref materialFlags
    Material::Flags flags;

    /// Layers.
    Material::Layers layers;
    Material::DetailLayer *detailLayer;
    Material::ShineLayer *shineLayer;

#ifdef __CLIENT__
    /// Decorations (will be projected into the map relative to a surface).
    Material::Decorations decorations;
#endif

    /// @c false= the material is no longer valid.
    bool valid;

    Instance(Public *i, MaterialManifest &_manifest)
        : Base(i),
          manifest(_manifest),
#ifdef __CLIENT__
         animationsAreDirty(true),
#endif
         envClass(AEC_UNKNOWN),
         flags(0),
         detailLayer(0),
         shineLayer(0),
         valid(true)
    {}

    ~Instance()
    {
#ifdef __CLIENT__
        self.clearVariants();
        self.clearDecorations();
#endif
        self.clearLayers();

#ifdef __CLIENT__
        clearAnimations();
#endif
    }

#ifdef __CLIENT__

    void clearAnimations()
    {
        // Context variants will be invalid after this, so clear them.
        self.clearVariants();

        QMutableMapIterator<MaterialContextId, Animation *> iter(animations);
        while(iter.hasNext())
        {
            Animation *animation = iter.next().value();
            delete animation;
            iter.remove();
        }
        animationsAreDirty = true;
    }

    void rebuildAnimations()
    {
        if(!animationsAreDirty) return;

        clearAnimations();

        // Create a new animation state for each render (usage) context.
        /// @todo If the material is not animated; don't create Animations.
        for(int rc = int(FirstMaterialContextId); rc <= int(LastMaterialContextId); ++rc)
        {
            MaterialContextId context = MaterialContextId(rc);
            animations.insert(context, new Animation(*thisPublic, context));
        }
        animationsAreDirty = false;
    }

#endif // __CLIENT__

    /// Notify interested parties of a change in world dimensions.
    void notifyDimensionsChanged()
    {
        /// @todo Replace with a de::Observers-based mechanism.
        R_UpdateMapSurfacesOnMaterialChange(&self);
    }

    /// Returns @c true iff both world dimension axes are defined.
    inline bool haveValidDimensions() const
    {
        return dimensions.x > 0 && dimensions.y > 0;
    }

    /**
     * Determines which texture we would be interested in obtaining our
     * world dimensions from if our own dimensions are undefined.
     */
    Texture *inheritDimensionsTexture()
    {
        // We're interested in the texture bound to the primary layer.
        if(!layers.count() || !layers[0]->stageCount()) return 0;
        return layers[0]->stages()[0]->texture;
    }

    /**
     * Determines whether the world dimensions are now defined and if so
     * cancels future notifications about changes to texture dimensions.
     */
    void maybeCancelTextureDimensionsChangeNotification()
    {
        // Both dimensions must still be undefined.
        if(haveValidDimensions()) return;

        Texture *inheritanceTexture = inheritDimensionsTexture();
        if(!inheritanceTexture) return;

        inheritanceTexture->audienceForDimensionsChange -= self;
        // Thusly, we are no longer interested in deletion notification either.
        inheritanceTexture->audienceForDeletion -= self;
    }
};

Material::Material(MaterialManifest &manifest)
    : MapElement(DMU_MATERIAL), d(new Instance(this, manifest))
{}

Material::~Material()
{
    d->maybeCancelTextureDimensionsChangeNotification();

    DENG2_FOR_AUDIENCE(Deletion, i) i->materialBeingDeleted(*this);
}

MaterialManifest &Material::manifest() const
{
    return d->manifest;
}

Vector2i const &Material::dimensions() const
{
    return d->dimensions;
}

void Material::setDimensions(Vector2i const &_newDimensions)
{
    Vector2i newDimensions = _newDimensions.max(Vector2i(0, 0));
    if(d->dimensions != newDimensions)
    {
        d->dimensions = newDimensions;
        d->maybeCancelTextureDimensionsChangeNotification();

        d->notifyDimensionsChanged();
    }
}

void Material::setWidth(int newWidth)
{
    if(d->dimensions.x != newWidth)
    {
        d->dimensions.x = newWidth;
        d->maybeCancelTextureDimensionsChangeNotification();

        d->notifyDimensionsChanged();
    }
}

void Material::setHeight(int newHeight)
{
    if(d->dimensions.y != newHeight)
    {
        d->dimensions.y = newHeight;
        d->maybeCancelTextureDimensionsChangeNotification();

        d->notifyDimensionsChanged();
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
    return d->detailLayer != 0;
}

bool Material::isShiny() const
{
    return d->shineLayer != 0;
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

void Material::clearLayers()
{
    d->maybeCancelTextureDimensionsChangeNotification();

#ifdef __CLIENT__
    d->animationsAreDirty = true;
#endif

    qDeleteAll(d->layers);
    d->layers.clear();

    if(d->detailLayer)
    {
        delete d->detailLayer; d->detailLayer = 0;
    }
    if(d->shineLayer)
    {
        delete d->shineLayer; d->shineLayer = 0;
    }
}

Material::Layer *Material::newLayer(ded_material_layer_t const *def)
{
#ifdef __CLIENT__
    d->animationsAreDirty = true;
#endif

    Layer *newLayer = def? Layer::fromDef(*def) : new Layer();
    d->layers.push_back(newLayer);

    // Are we interested in inheriting dimensions from the layer's texture?
    if(!d->haveValidDimensions() && d->layers.count() == 1)
    {
        Texture *inheritanceTexture = d->inheritDimensionsTexture();
        if(inheritanceTexture)
        {
            inheritanceTexture->audienceForDimensionsChange += this;
            // Thusly, we are also interested in deletion notification.
            inheritanceTexture->audienceForDeletion += this;
        }
    }
    return newLayer;
}

Material::DetailLayer *Material::newDetailLayer(ded_detailtexture_t const *def)
{
#ifdef __CLIENT__
    d->animationsAreDirty = true;
#endif

    DetailLayer *newLayer = def? DetailLayer::fromDef(*def) : new DetailLayer();
    if(d->detailLayer) delete d->detailLayer;
    d->detailLayer = newLayer;
    return newLayer;
}

Material::ShineLayer *Material::newShineLayer(ded_reflection_t const *def)
{
#ifdef __CLIENT__
    d->animationsAreDirty = true;
#endif

    ShineLayer *newLayer = def? ShineLayer::fromDef(*def) : new ShineLayer();
    if(d->shineLayer) delete d->shineLayer;
    d->shineLayer = newLayer;
    return newLayer;
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

void Material::textureDimensionsChanged(Texture const &texture)
{
    DENG2_ASSERT(!d->haveValidDimensions()); // Sanity check.
    setDimensions(texture.dimensions());
}

void Material::textureBeingDeleted(de::Texture const &texture)
{
    // If here it means the texture we were planning to inherit dimensions
    // from is being deleted and therefore we won't be able to.

    DENG2_ASSERT(!d->haveValidDimensions()); // Sanity check.
    DENG2_ASSERT(d->inheritDimensionsTexture() == &texture); // Sanity check.

    // Clear the association so we don't try to cancel notifications later.
    d->layers[0]->stages()[0]->texture = 0;

#if !defined(DENG2_DEBUG)
    DENG2_UNUSED(texture);
#endif
}

#ifdef __CLIENT__

void Material::addDecoration(Material::Decoration &decor)
{
    if(d->decorations.contains(&decor)) return;

    d->decorations.push_back(&decor);
    d->animationsAreDirty = true;
}

Material::Decorations const &Material::decorations() const
{
    return d->decorations;
}

void Material::clearDecorations()
{
    if(!isDecorated()) return;

    qDeleteAll(d->decorations);
    d->decorations.clear();
    d->animationsAreDirty = true;
}

Material::Animation &Material::animation(MaterialContextId context) const
{
    d->rebuildAnimations();

    Animations::const_iterator found = d->animations.find(context);
    if(found != d->animations.end())
    {
        return *found.value();
    }
    /// @throw MissingAnimationError No animation exists for the specifed context.
    throw MissingAnimationError("Material::animation", QString("No animation for context %1").arg(int(context)));
}

Material::Animations const &Material::animations() const
{
    d->rebuildAnimations();
    return d->animations;
}

Material::Variants const &Material::variants() const
{
    // If an animation state rebuild is necessary, the context variants will need
    // to be rebuilt also.
    d->rebuildAnimations();
    return d->variants;
}

Material::Variant *Material::chooseVariant(Material::VariantSpec const &spec, bool canCreate)
{
    foreach(Variant *variant, variants())
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
    while(!d->variants.isEmpty())
    {
         delete d->variants.takeFirst();
    }
    d->variants.clear();
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

String Material::description() const
{
    String str = String("Material \"%1\"").arg(manifest().composeUri().asText());
#ifdef DENG_DEBUG
    str += String(" [%2]").arg(de::dintptr(this));
#endif
    str += " Dimensions:"
        +  (width() == 0 && height() == 0? String("unknown (not yet prepared)")
                                         : dimensions().asText())
        +  " Source:" + manifest().sourceDescription();
#ifdef __CLIENT__
    str += String(" x%1").arg(variantCount());
#endif
    return str;
}

String Material::synopsis() const
{
    String str = String("Drawable:%1 EnvClass:\"%2\"")
                   .arg(isDrawable()? "yes" : "no")
                   .arg(audioEnvironment() == AEC_UNKNOWN? "N/A" : S_AudioEnvironmentName(audioEnvironment()));
#ifdef __CLIENT__
    str += String(" Decorated:%1").arg(isDecorated()? "yes" : "no");
#endif
    str += String("\nDetailed:%1 Glowing:%2 Shiny:%3 SkyMasked:%4")
               .arg(isDetailed()     ? "yes" : "no")
               .arg(hasGlow()        ? "yes" : "no")
               .arg(isShiny()        ? "yes" : "no")
               .arg(isSkyMasked()    ? "yes" : "no");

    // Add the layer config:
    for(int i = 0; i < layers().count(); ++i)
    {
        Material::Layer const &lDef = *(layers()[i]);
        int const stageCount = lDef.stageCount();

        str += String("\nLayer #%1 (%2 %3):")
                   .arg(i).arg(stageCount).arg(stageCount == 1? "Stage" : "Stages");

        for(int k = 0; k < stageCount; ++k)
        {
            Material::Layer::Stage const &sDef = *(lDef.stages()[k]);
            String path = sDef.texture? sDef.texture->manifest().composeUri().asText()
                                      : QString("(prev)");

            str += String("\n  #%1: Texture:\"%2\" Tics:%3 (~%4)"
                          "\n      Offset:%5 Glow:%6 (~%7)")
                       .arg(k)
                       .arg(path)
                       .arg(sDef.tics)
                       .arg(sDef.variance,             0, 'g', 2)
                       .arg(sDef.texOrigin.asText())
                       .arg(sDef.glowStrength,         0, 'g', 2)
                       .arg(sDef.glowStrengthVariance, 0, 'g', 2);
        }
    }

    // Add the detail layer config:
    if(isDetailed())
    {
        Material::DetailLayer const &lDef = detailLayer();
        int const stageCount = lDef.stageCount();

        str += String("\nDetailLayer #0 (%1 %2):")
                   .arg(stageCount).arg(stageCount == 1? "Stage" : "Stages");

        for(int i = 0; i < stageCount; ++i)
        {
            Material::DetailLayer::Stage const &sDef = *(lDef.stages()[i]);
            String path = sDef.texture? sDef.texture->manifest().composeUri().asText()
                                      : QString("(prev)");

            str += String("\n  #%1: Texture:\"%2\" Tics:%3 (~%4)"
                          "\n       Scale:%5 Strength:%6 MaxDistance:%7")
                       .arg(i)
                       .arg(path)
                       .arg(sDef.tics)
                       .arg(sDef.variance,    0, 'g', 2)
                       .arg(sDef.scale,       0, 'g', 2)
                       .arg(sDef.strength,    0, 'g', 2)
                       .arg(sDef.maxDistance, 0, 'g', 2);
        }
    }

    // Add the shine layer config:
    if(isShiny())
    {
        Material::ShineLayer const &lDef = shineLayer();
        int const stageCount = lDef.stageCount();

        str += String("\nShineLayer #0 (%1 %2):")
                   .arg(stageCount).arg(stageCount == 1? "Stage" : "Stages");

        for(int i = 0; i < stageCount; ++i)
        {
            Material::ShineLayer::Stage const &sDef = *(lDef.stages()[i]);
            String path = sDef.texture? sDef.texture->manifest().composeUri().asText()
                                      : QString("(prev)");
            String maskPath = sDef.maskTexture? sDef.maskTexture->manifest().composeUri().asText()
                                              : QString("(none)");

            str += String("\n  #%1: Texture:\"%2\" MaskTexture:\"%3\" Tics:%4 (~%5)"
                          "\n      Shininess:%6 BlendMode:%7 MaskDimensions:%8"
                          "\n      MinColor:%9")
                       .arg(i)
                       .arg(path)
                       .arg(maskPath)
                       .arg(sDef.tics)
                       .arg(sDef.variance, 0, 'g', 2)
                       .arg(sDef.shininess, 0, 'g', 2)
                       .arg(R_NameForBlendMode(sDef.blendMode))
                       .arg(sDef.maskDimensions.asText())
                       .arg(sDef.minColor.asText());
        }
    }

#ifdef __CLIENT__
    // Add the decoration config:
    if(isDecorated())
    {
        for(int i = 0; i < decorations().count(); ++i)
        {
            MaterialDecoration const *lDef = decorations()[i];
            int const stageCount = lDef->stageCount();

            str += String("\nDecoration #%1 (%2 %3):")
                       .arg(i).arg(stageCount).arg(stageCount == 1? "Stage" : "Stages");

            for(int k = 0; k < stageCount; ++k)
            {
                MaterialDecoration::Stage const &sDef = *lDef->stages()[k];

                str += String("\n  #%1: Tics:%2 (~%3) Offset:%4 Elevation:%5"
                              "\n      Color:%6 Radius:%7 HaloRadius:%8"
                              "\n      LightLevels:%9")
                           .arg(k)
                           .arg(sDef.tics)
                           .arg(sDef.variance, 0, 'g', 2)
                           .arg(sDef.pos.asText())
                           .arg(sDef.elevation, 0, 'g', 2)
                           .arg(sDef.color.asText())
                           .arg(sDef.radius, 0, 'g', 2)
                           .arg(sDef.haloRadius, 0, 'g', 2)
                           .arg(sDef.lightLevels.asText());
            }
        }
    }
#endif

    return str;
}
