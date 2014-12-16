/** @file material.cpp  Logical material resource.
 *
 * @authors Copyright © 2009-2014 Daniel Swanson <danij@dengine.net>
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

#include "resource/material.h"

#include <QFlag>
#include <QtAlgorithms>
#include <de/Log>
#include <doomsday/console/cmd.h>

#include "dd_main.h"
#include "MaterialManifest"
#ifdef __CLIENT__
#  include "MaterialAnimator"
#endif

#include "resource/materialdetaillayer.h"
#include "resource/materialtexturelayer.h"
#include "resource/materialshinelayer.h"

namespace internal
{
    enum MaterialFlag
    {
        //Unused1      = MATF_UNUSED1,
        DontDraw     = MATF_NO_DRAW,  ///< Map surfaces using the material should never be drawn.
        SkyMasked    = MATF_SKYMASK,  ///< Apply sky masking for map surfaces using the material.

        Valid        = 0x8,           ///< Marked as @em valid.
        DefaultFlags = Valid
    };
    Q_DECLARE_FLAGS(MaterialFlags, MaterialFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(MaterialFlags)
}
using namespace internal;
using namespace de;

int Material::Layer::stageCount() const
{
    return _stages.count();
}

Material::Layer::~Layer()
{
    qDeleteAll(_stages);
}

Material::Layer::Stage &Material::Layer::stage(int index) const
{
    if(stageCount())
    {
        index = de::wrap(index, 0, _stages.count());
        return *_stages[index];
    }
    /// @throw MissingStageError  No stages are defined.
    throw MissingStageError("Material::Layer::stage", "Layer has no stages");
}

String Material::Layer::describe() const
{
    return "abstract Layer";
}

String Material::Layer::description() const
{
    int const numStages = stageCount();
    String str = _E(b) + describe() + _E(.) + " (" + String::number(numStages) + " stage" + DENG2_PLURAL_S(numStages) + "):";
    for(int i = 0; i < numStages; ++i)
    {
        str += String("\n  [%1] ").arg(i, 2) + _E(>) + stage(i).description() + _E(<);
    }
    return str;
}

// ------------------------------------------------------------------------------------

#ifdef __CLIENT__

DENG2_PIMPL_NOREF(Material::Decoration)
{
    Material *material = nullptr;  ///< Owning Material.
    Vector2i patternSkip;          ///< Pattern skip intervals.
    Vector2i patternOffset;        ///< Pattern skip interval offsets.
};

Material::Decoration::Decoration(Vector2i const &patternSkip, Vector2i const &patternOffset)
    : d(new Instance)
{
    d->patternSkip   = patternSkip;
    d->patternOffset = patternOffset;
}

Material::Decoration::~Decoration()
{
    qDeleteAll(_stages);
}

Material &Material::Decoration::material()
{
    DENG2_ASSERT(d->material);
    return *d->material;
}

Material const &Material::Decoration::material() const
{
    DENG2_ASSERT(d->material);
    return *d->material;
}

void Material::Decoration::setMaterial(Material *newOwner)
{
    d->material = newOwner;
}

Vector2i const &Material::Decoration::patternSkip() const
{
    return d->patternSkip;
}

Vector2i const &Material::Decoration::patternOffset() const
{
    return d->patternOffset;
}

int Material::Decoration::stageCount() const
{
    return _stages.count();
}

Material::Decoration::Stage &Material::Decoration::stage(int index) const
{
    if(stageCount())
    {
        index = de::wrap(index, 0, _stages.count());
        return *_stages[index];
    }
    /// @throw MissingStageError  No stages are defined.
    throw MissingStageError("Material::Decoration::stage", "Decoration has no stages");
}

String Material::Decoration::describe() const
{
    return "abstract Decoration";
}

String Material::Decoration::description() const
{
    int const numStages = stageCount();
    String str = _E(b) + describe() + _E(.) + " (" + String::number(numStages) + " stage" + DENG2_PLURAL_S(numStages) + "):";
    for(int i = 0; i < numStages; ++i)
    {
        str += String("\n  [%1] ").arg(i, 2) + _E(>) + stage(i).description() + _E(<);
    }
    return str;
}

#endif  // __CLIENT__

// ------------------------------------------------------------------------------------

DENG2_PIMPL(Material)
, DENG2_OBSERVES(Texture, Deletion)
, DENG2_OBSERVES(Texture, DimensionsChange)
{
    MaterialManifest *manifest = nullptr;  ///< Source manifest (always valid, not owned).
    Vector2i dimensions;                   ///< World dimensions in map coordinate space units.
    MaterialFlags flags { DefaultFlags };
    AudioEnvironmentId audioEnvironment { AE_NONE };

    /// Layers (owned), from bottom-most to top-most draw order.
    QList<Layer *> layers;

#ifdef __CLIENT__
    /// Decorations (owned), to be projected into the world (relative to a Surface).
    QList<Decoration *> decorations;

    /// Set of draw-context animators (owned).
    QList<MaterialAnimator *> animators;
#endif

    Instance(Public *i) : Base(i) {}

    ~Instance()
    {
#ifdef __CLIENT__
        self.clearAllAnimators();
        self.clearAllDecorations();
#endif
        self.clearAllLayers();
    }

    inline bool haveValidDimensions() const {
        return dimensions.x > 0 && dimensions.y > 0;
    }

    MaterialTextureLayer *firstTextureLayer() const
    {
        for(Layer *layer : layers)
        {
            if(layer->is<MaterialDetailLayer>()) continue;
            if(layer->is<MaterialShineLayer>())         continue;

            if(auto *texLayer = layer->maybeAs<MaterialTextureLayer>())
            {
                return texLayer;
            }
        }
        return nullptr;
    }

    /**
     * Determines which texture we would be interested in obtaining our world dimensions
     * from if our own dimensions are undefined.
     */
    Texture *inheritDimensionsTexture() const
    {
        if(auto const *texLayer = firstTextureLayer())
        {
            if(texLayer->stageCount() >= 1)
            {
                return texLayer->stage(0).texture();
            }
        }
        return nullptr;
    }

    /**
     * Determines whether the world dimensions are now defined and if so cancels future
     * notifications about changes to texture dimensions.
     */
    void maybeCancelTextureDimensionsChangeNotification()
    {
        // Both dimensions must still be undefined.
        if(haveValidDimensions()) return;

        Texture *inheritanceTexture = inheritDimensionsTexture();
        if(!inheritanceTexture) return;

        inheritanceTexture->audienceForDimensionsChange -= this;
        // Thusly, we are no longer interested in deletion notification either.
        inheritanceTexture->audienceForDeletion -= this;
    }

    // Observes Texture DimensionsChange.
    void textureDimensionsChanged(Texture const &texture)
    {
        DENG2_ASSERT(!haveValidDimensions()); // Sanity check.
        self.setDimensions(texture.dimensions());
    }

    // Observes Texture Deletion.
    void textureBeingDeleted(Texture const &texture)
    {
        // If here it means the texture we were planning to inherit dimensions from is
        // being deleted and therefore we won't be able to.

        DENG2_ASSERT(!haveValidDimensions()); // Sanity check.
        DENG2_ASSERT(inheritDimensionsTexture() == &texture); // Sanity check.

        // Clear the association so we don't try to cancel notifications later.
        firstTextureLayer()->stage(0).setTexture(nullptr);

#if !defined(DENG2_DEBUG)
        DENG2_UNUSED(texture);
#endif
    }

#ifdef __CLIENT__
    MaterialAnimator *findAnimator(MaterialVariantSpec const &spec, bool canCreate = false)
    {
        for(MaterialAnimator const *animator : animators)
        {
            if(animator->variantSpec().compare(spec))
            {
                return const_cast<MaterialAnimator *>(animator);  // This will do fine.
            }
        }

        if(!canCreate) return nullptr;

        animators.append(new MaterialAnimator(self, spec));
        return animators.back();
    }
#endif // __CLIENT__

    DENG2_PIMPL_AUDIENCE(Deletion)
    DENG2_PIMPL_AUDIENCE(DimensionsChange)
};

DENG2_AUDIENCE_METHOD(Material, Deletion)
DENG2_AUDIENCE_METHOD(Material, DimensionsChange)

Material::Material(MaterialManifest &manifest)
    : MapElement(DMU_MATERIAL)
    , d(new Instance(this))
{
    d->manifest = &manifest;
}

Material::~Material()
{
    d->maybeCancelTextureDimensionsChangeNotification();

    DENG2_FOR_AUDIENCE2(Deletion, i) i->materialBeingDeleted(*this);
}

MaterialManifest &Material::manifest() const
{
    DENG2_ASSERT(d->manifest);
    return *d->manifest;
}

Vector2i const &Material::dimensions() const
{
    return d->dimensions;
}

void Material::setDimensions(Vector2i const &newDimensions)
{
    Vector2i const newDimensionsClamped = newDimensions.max(Vector2i(0, 0));
    if(d->dimensions != newDimensionsClamped)
    {
        d->dimensions = newDimensionsClamped;
        d->maybeCancelTextureDimensionsChangeNotification();

        // Notify interested parties.
        DENG2_FOR_AUDIENCE2(DimensionsChange, i) i->materialDimensionsChanged(*this);
    }
}

void Material::setHeight(int newHeight)
{
    setDimensions(Vector2i(width(), newHeight));
}

void Material::setWidth(int newWidth)
{
    setDimensions(Vector2i(newWidth, height()));
}

bool Material::isDrawable() const
{
    return d->flags.testFlag(DontDraw) == false;
}

bool Material::isSkyMasked() const
{
    return d->flags.testFlag(SkyMasked);
}

bool Material::isValid() const
{
    return d->flags.testFlag(Valid);
}

void Material::markDontDraw(bool yes)
{
    if(yes)  d->flags |=  DontDraw;
    else     d->flags &= ~DontDraw;
}

void Material::markSkyMasked(bool yes)
{
    if(yes)  d->flags |=  SkyMasked;
    else     d->flags &= ~SkyMasked;
}

void Material::markValid(bool yes)
{
    if(yes)  d->flags |=  Valid;
    else     d->flags &= ~Valid;
}

AudioEnvironmentId Material::audioEnvironment() const
{
    return (isDrawable()? d->audioEnvironment : AE_NONE);
}

void Material::setAudioEnvironment(AudioEnvironmentId newEnvironment)
{
    d->audioEnvironment = newEnvironment;
}

void Material::clearAllLayers()
{
    d->maybeCancelTextureDimensionsChangeNotification();

    qDeleteAll(d->layers); d->layers.clear();
}

int Material::layerCount() const
{
    return d->layers.count();
}

void Material::addLayerAt(Layer *layer, int position)
{
    if(!layer) return;
    if(d->layers.contains(layer)) return;

    position = de::clamp(0, position, layerCount());

    d->maybeCancelTextureDimensionsChangeNotification();

    d->layers.insert(position, layer);

    if(!d->haveValidDimensions())
    {
        if(Texture *tex = d->inheritDimensionsTexture())
        {
            tex->audienceForDeletion         += d;
            tex->audienceForDimensionsChange += d;
        }
    }
}

Material::Layer &Material::layer(int index) const
{
    if(index >= 0 && index < layerCount()) return *d->layers[index];
    /// @throw Material::MissingLayerError  Invalid layer reference.
    throw MissingLayerError("Material::layer", "Unknown layer #" + String::number(index));
}

Material::Layer *Material::layerPtr(int index) const
{
    if(index >= 0 && index < layerCount()) return d->layers[index];
    return nullptr;
}

#ifdef __CLIENT__

int Material::decorationCount() const
{
    return d->decorations.count();
}

LoopResult Material::forAllDecorations(std::function<LoopResult (Decoration &)> func) const
{
    for(Decoration *decor : d->decorations)
    {
        if(auto result = func(*decor)) return result;
    }
    return LoopContinue;
}

/// @todo Update client side MaterialAnimators?
void Material::addDecoration(Decoration *decor)
{
    if(!decor || d->decorations.contains(decor)) return;

    decor->setMaterial(this);
    d->decorations.append(decor);
}

void Material::clearAllDecorations()
{
    qDeleteAll(d->decorations); d->decorations.clear();
}

int Material::animatorCount() const
{
    return d->animators.count();
}

bool Material::hasAnimator(MaterialVariantSpec const &spec)
{
    return d->findAnimator(spec) != nullptr;
}

MaterialAnimator &Material::getAnimator(MaterialVariantSpec const &spec)
{
    return *d->findAnimator(spec, true/*create*/);
}

LoopResult Material::forAllAnimators(std::function<LoopResult (MaterialAnimator &)> func) const
{
    for(MaterialAnimator *animator : d->animators)
    {
        if(auto result = func(*animator)) return result;
    }
    return LoopContinue;
}

void Material::clearAllAnimators()
{
    qDeleteAll(d->animators); d->animators.clear();
}

#endif  // __CLIENT__

String Material::describe() const
{
    return "Material \"" + manifest().composeUri().asText() + "\"";
}

String Material::description() const
{
    String str = String(_E(l) "Dimensions: ") + _E(.) + (d->haveValidDimensions()? dimensions().asText() : "unknown (not yet prepared)")
               + _E(l) + " Source: "     + _E(.) + manifest().sourceDescription()
#ifdef __CLIENT__
               + _E(b) + " x"  + String::number(animatorCount()) + _E(.)
#endif
               + _E(l) + "\nDrawable: "  + _E(.) + DENG2_BOOL_YESNO(isDrawable())
#ifdef __CLIENT__
               + _E(l) + " EnvClass: \"" + _E(.) + (audioEnvironment() == AE_NONE? "N/A" : S_AudioEnvironmentName(audioEnvironment())) + "\""
#endif
               + _E(l) + " SkyMasked: "  + _E(.) + DENG2_BOOL_YESNO(isSkyMasked());

    // Add the layer config:
    for(Layer const *layer : d->layers)
    {
        str += "\n" + layer->description();
    }

#ifdef __CLIENT__
    // Add the decoration config:
    for(Decoration const *decor : d->decorations)
    {
        str += "\n" + decor->description();
    }
#endif

    return str;
}

int Material::property(DmuArgs &args) const
{
    switch(args.prop)
    {
    case DMU_FLAGS: {
        short f = d->flags;
        args.setValue(DMT_MATERIAL_FLAGS, &f, 0);
        break; }

    case DMU_HEIGHT: {
        int h = d->dimensions.y;
        args.setValue(DMT_MATERIAL_HEIGHT, &h, 0);
        break; }

    case DMU_WIDTH: {
        int w = d->dimensions.x;
        args.setValue(DMT_MATERIAL_WIDTH, &w, 0);
        break; }

    default:
        return MapElement::property(args);
    }
    return false; // Continue iteration.
}

D_CMD(InspectMaterial)
{
    DENG2_UNUSED(src);
    ResourceSystem &resSys = App_ResourceSystem();

    de::Uri search = de::Uri::fromUserInput(&argv[1], argc - 1);
    if(!search.scheme().isEmpty() &&
       !resSys.knownMaterialScheme(search.scheme()))
    {
        LOG_SCR_WARNING("Unknown scheme \"%s\"") << search.scheme();
        return false;
    }

    try
    {
        MaterialManifest &manifest = resSys.materialManifest(search);
        if(Material *material = manifest.materialPtr())
        {
            LOG_SCR_MSG(_E(D)_E(b) "%s\n" _E(.)_E(.)) << material->describe() << material->description();
        }
        else
        {
            LOG_SCR_MSG(manifest.description());
        }
        return true;
    }
    catch(ResourceSystem::MissingManifestError const &er)
    {
        LOG_SCR_WARNING("%s") << er.asText();
    }
    return false;
}

void Material::consoleRegister() // static
{
    C_CMD("inspectmaterial",    "ss",   InspectMaterial)
    C_CMD("inspectmaterial",    "s",    InspectMaterial)
}
