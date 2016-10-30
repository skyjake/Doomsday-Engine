/** @file material.cpp  World material.
 *
 * @authors Copyright © 2009-2015 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/world/material.h"

#include "doomsday/console/cmd.h"
#include "doomsday/res/Textures"
#include "doomsday/resource/resources.h"
#include "doomsday/world/materials.h"
#include "doomsday/world/materialmanifest.h"
#include "doomsday/world/texturemateriallayer.h"
#include "doomsday/world/detailtexturemateriallayer.h"
#include "doomsday/world/shinetexturemateriallayer.h"

#include <QFlag>
#include <QtAlgorithms>
#include <de/Log>

using namespace de;

namespace world {

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
    if (!stageCount())
    {
        /// @throw MissingStageError  No stages are defined.
        throw MissingStageError("Material::Layer::stage", "Layer has no stages");
    }
    return *_stages[de::wrap(index, 0, _stages.count())];
}

int Material::Layer::nextStageIndex(int index) const
{
    if (!stageCount()) return -1;
    return de::wrap(index + 1, 0, _stages.count());
}

String Material::Layer::describe() const
{
    return "abstract Layer";
}

String Material::Layer::description() const
{
    int const numStages = stageCount();
    String str = _E(b) + describe() + _E(.) + " (" + String::number(numStages) + " stage" + DENG2_PLURAL_S(numStages) + "):";
    for (int i = 0; i < numStages; ++i)
    {
        str += String("\n  [%1] ").arg(i, 2) + _E(>) + stage(i).description() + _E(<);
    }
    return str;
}

// ------------------------------------------------------------------------------------

DENG2_PIMPL(Material)
, DENG2_OBSERVES(res::Texture, Deletion)
, DENG2_OBSERVES(res::Texture, DimensionsChange)
{
    MaterialManifest *manifest = nullptr;  ///< Source manifest (always valid, not owned).
    Vector2ui dimensions;                  ///< World dimensions in map coordinate space units.
    internal::MaterialFlags flags = internal::DefaultFlags;

    /// Layers (owned), from bottom-most to top-most draw order.
    QList<Layer *> layers;

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        self.clearAllLayers();
    }

    inline bool haveValidDimensions() const {
        return dimensions.x > 0 && dimensions.y > 0;
    }

    TextureMaterialLayer *firstTextureLayer() const
    {
        for (Layer *layer : layers)
        {
            if (layer->is<DetailTextureMaterialLayer>()) continue;
            if (layer->is<ShineTextureMaterialLayer>())  continue;

            if (auto *texLayer = layer->maybeAs<TextureMaterialLayer>())
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
    res::Texture *inheritDimensionsTexture() const
    {
        if (auto const *texLayer = firstTextureLayer())
        {
            if (texLayer->stageCount() >= 1)
            {
                try
                {
                    return &res::Textures::get().texture(de::Uri(texLayer->stage(0).gets("texture"), RC_NULL));
                }
                catch (res::TextureManifest::MissingTextureError &)
                {}
                catch (Resources::MissingResourceManifestError &)
                {}
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
        if (haveValidDimensions()) return;

        res::Texture *inheritanceTexture = inheritDimensionsTexture();
        if (!inheritanceTexture) return;

        inheritanceTexture->audienceForDimensionsChange -= this;
        // Thusly, we are no longer interested in deletion notification either.
        inheritanceTexture->audienceForDeletion -= this;
    }

    // Observes Texture DimensionsChange.
    void textureDimensionsChanged(res::Texture const &texture)
    {
        DENG2_ASSERT(!haveValidDimensions()); // Sanity check.
        self.setDimensions(texture.dimensions());
    }

    // Observes Texture Deletion.
    void textureBeingDeleted(res::Texture const &texture)
    {
        // If here it means the texture we were planning to inherit dimensions from is
        // being deleted and therefore we won't be able to.

        DENG2_ASSERT(!haveValidDimensions()); // Sanity check.
        DENG2_ASSERT(inheritDimensionsTexture() == &texture); // Sanity check.

        /// @todo kludge: Clear the association so we don't try to cancel notifications later.
        firstTextureLayer()->stage(0).set("texture", "");

#if !defined(DENG2_DEBUG)
        DENG2_UNUSED(texture);
#endif
    }

    DENG2_PIMPL_AUDIENCE(Deletion)
    DENG2_PIMPL_AUDIENCE(DimensionsChange)
};

DENG2_AUDIENCE_METHOD(Material, Deletion)
DENG2_AUDIENCE_METHOD(Material, DimensionsChange)

Material::Material(MaterialManifest &manifest)
    : MapElement(DMU_MATERIAL)
    , d(new Impl(this))
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

Vector2ui const &Material::dimensions() const
{
    return d->dimensions;
}

void Material::setDimensions(Vector2ui const &newDimensions)
{
    if (d->dimensions != newDimensions)
    {
        d->dimensions = newDimensions;
        d->maybeCancelTextureDimensionsChangeNotification();

        // Notify interested parties.
        DENG2_FOR_AUDIENCE2(DimensionsChange, i) i->materialDimensionsChanged(*this);
    }
}

void Material::setHeight(int newHeight)
{
    setDimensions(Vector2ui(width(), newHeight));
}

void Material::setWidth(int newWidth)
{
    setDimensions(Vector2ui(newWidth, height()));
}

bool Material::isDrawable() const
{
    return d->flags.testFlag(internal::DontDraw) == false;
}

bool Material::isSkyMasked() const
{
    return d->flags.testFlag(internal::SkyMasked);
}

bool Material::isValid() const
{
    return d->flags.testFlag(internal::Valid);
}

void Material::markDontDraw(bool yes)
{
    if (yes) d->flags |=  internal::DontDraw;
    else     d->flags &= ~internal::DontDraw;
}

void Material::markSkyMasked(bool yes)
{
    if (yes) d->flags |=  internal::SkyMasked;
    else     d->flags &= ~internal::SkyMasked;
}

void Material::markValid(bool yes)
{
    if (yes) d->flags |=  internal::Valid;
    else     d->flags &= ~internal::Valid;
}

void Material::clearAllLayers()
{
    d->maybeCancelTextureDimensionsChangeNotification();

    qDeleteAll(d->layers); d->layers.clear();
}

bool Material::hasAnimatedTextureLayers() const
{
    for (Layer const *layer : d->layers)
    {
        if (   !layer->is<DetailTextureMaterialLayer>()
            && !layer->is<ShineTextureMaterialLayer>())
        {
            if (layer->isAnimated())
                return true;
        }
    }
    return false;
}

int Material::layerCount() const
{
    return d->layers.count();
}

void Material::addLayerAt(Layer *layer, int position)
{
    if (!layer) return;
    if (d->layers.contains(layer)) return;

    position = de::clamp(0, position, layerCount());

    d->maybeCancelTextureDimensionsChangeNotification();

    d->layers.insert(position, layer);

    if (!d->haveValidDimensions())
    {
        if (res::Texture *tex = d->inheritDimensionsTexture())
        {
            tex->audienceForDeletion         += d;
            tex->audienceForDimensionsChange += d;
        }
    }
}

Material::Layer &Material::layer(int index) const
{
    if (index >= 0 && index < layerCount()) return *d->layers[index];
    /// @throw Material::MissingLayerError  Invalid layer reference.
    throw MissingLayerError("Material::layer", "Unknown layer #" + String::number(index));
}

Material::Layer *Material::layerPtr(int index) const
{
    if (index >= 0 && index < layerCount()) return d->layers[index];
    return nullptr;
}

String Material::describe() const
{
    return "Material \"" + manifest().composeUri().asText() + "\"";
}

String Material::description() const
{
    String str = String(_E(l) "Dimensions: ") + _E(.) + (d->haveValidDimensions()? dimensions().asText() : "unknown (not yet prepared)")
               + _E(l) + " Source: "     + _E(.) + manifest().sourceDescription()
               + _E(l) + "\nDrawable: "  + _E(.) + DENG2_BOOL_YESNO(isDrawable())
               + _E(l) + " SkyMasked: "  + _E(.) + DENG2_BOOL_YESNO(isSkyMasked());

    // Add the layer config:
    for (Layer const *layer : d->layers)
    {
        str += "\n" + layer->description();
    }

    return str;
}

int Material::property(DmuArgs &args) const
{
    switch (args.prop)
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

    de::Uri search = de::Uri::fromUserInput(&argv[1], argc - 1);
    if (!search.scheme().isEmpty() &&
        !world::Materials::get().isKnownMaterialScheme(search.scheme()))
    {
        LOG_SCR_WARNING("Unknown scheme \"%s\"") << search.scheme();
        return false;
    }

    try
    {
        MaterialManifest &manifest = world::Materials::get().materialManifest(search);
        if (Material *material = manifest.materialPtr())
        {
            LOG_SCR_MSG(_E(D)_E(b) "%s\n" _E(.)_E(.)) << material->describe() << material->description();
        }
        else
        {
            LOG_SCR_MSG(manifest.description());
        }
        return true;
    }
    catch (Resources::MissingResourceManifestError const &er)
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

} // namespace world
