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
#include "doomsday/res/textures.h"
#include "doomsday/res/resources.h"
#include "doomsday/world/materials.h"
#include "doomsday/world/materialmanifest.h"
#include "doomsday/world/texturemateriallayer.h"
#include "doomsday/world/detailtexturemateriallayer.h"
#include "doomsday/world/shinetexturemateriallayer.h"

#include <de/log.h>

using namespace de;

namespace world {

Material::Layer::~Layer()
{
    deleteAll(_stages);
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
    const int numStages = stageCount();
    String str = _E(b) + describe() + _E(.) + " (" + String::asText(numStages) + " stage" + DE_PLURAL_S(numStages) + "):";
    for (int i = 0; i < numStages; ++i)
    {
        str += Stringf("\n  [%2i] ", i) + _E(>) + stage(i).description() + _E(<);
    }
    return str;
}

// ------------------------------------------------------------------------------------

DE_PIMPL(Material)
, DE_OBSERVES(res::Texture, Deletion)
, DE_OBSERVES(res::Texture, DimensionsChange)
{
    MaterialManifest * manifest = nullptr; // Source manifest (always valid, not owned).
    Vec2ui             dimensions;         // World dimensions in map coordinate space units.
    AudioEnvironmentId audioEnvironment = AE_NONE;

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        deleteAll(self()._layers);
        //self().clearAllLayers();
    }

    inline bool haveValidDimensions() const {
        return dimensions.x > 0 && dimensions.y > 0;
    }

    TextureMaterialLayer *firstTextureLayer() const
    {
        for (Layer *layer : self()._layers)
        {
            if (is<DetailTextureMaterialLayer>(layer)) continue;
            if (is<ShineTextureMaterialLayer> (layer)) continue;

            if (auto *texLayer = maybeAs<TextureMaterialLayer>(layer))
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
        if (const auto *texLayer = firstTextureLayer())
        {
            if (texLayer->stageCount() >= 1)
            {
                try
                {
                    return &res::Textures::get().texture(texLayer->stage(0).texture);
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
    void textureDimensionsChanged(const res::Texture &texture)
    {
        DE_ASSERT(!haveValidDimensions()); // Sanity check.
        self().setDimensions(texture.dimensions());
    }

    // Observes Texture Deletion.
    void textureBeingDeleted(const res::Texture &texture)
    {
        // If here it means the texture we were planning to inherit dimensions from is
        // being deleted and therefore we won't be able to.

        DE_ASSERT(!haveValidDimensions()); // Sanity check.
        DE_ASSERT(inheritDimensionsTexture() == &texture); // Sanity check.

        /// @todo kludge: Clear the association so we don't try to cancel notifications later.
        firstTextureLayer()->stage(0).texture = res::Uri();

#if !defined(DE_DEBUG)
        DE_UNUSED(texture);
#endif
    }

    DE_PIMPL_AUDIENCE(Deletion)
    DE_PIMPL_AUDIENCE(DimensionsChange)
};

DE_AUDIENCE_METHOD(Material, Deletion)
DE_AUDIENCE_METHOD(Material, DimensionsChange)

Material::Material(MaterialManifest &manifest)
    : MapElement(DMU_MATERIAL)
    , d(new Impl(this))
{
    d->manifest = &manifest;
}

Material::~Material()
{
    d->maybeCancelTextureDimensionsChangeNotification();

    DE_NOTIFY(Deletion, i) i->materialBeingDeleted(*this);
}

MaterialManifest &Material::manifest() const
{
    DE_ASSERT(d->manifest);
    return *d->manifest;
}

const Vec2ui &Material::dimensions() const
{
    return d->dimensions;
}

void Material::setDimensions(const Vec2ui &newDimensions)
{
    if (d->dimensions != newDimensions)
    {
        d->dimensions = newDimensions;
        d->maybeCancelTextureDimensionsChangeNotification();

        // Notify interested parties.
        DE_NOTIFY(DimensionsChange, i) i->materialDimensionsChanged(*this);
    }
}

void Material::setHeight(int newHeight)
{
    setDimensions(Vec2ui(width(), newHeight));
}

bool Material::isAnimated() const
{
    return hasAnimatedTextureLayers();
}

void Material::setWidth(int newWidth)
{
    setDimensions(Vec2ui(newWidth, height()));
}

void Material::markDontDraw(bool yes)
{
    applyFlagOperation(_flags, DontDraw, yes);
}

void Material::markSkyMasked(bool yes)
{
    applyFlagOperation(_flags, SkyMasked, yes);
}

void Material::markValid(bool yes)
{
    applyFlagOperation(_flags, Valid, yes);
}

void Material::clearAllLayers()
{
    d->maybeCancelTextureDimensionsChangeNotification();

    deleteAll(_layers);
    _layers.clear();
}

bool Material::hasAnimatedTextureLayers() const
{
    for (const Layer *layer : _layers)
    {
        if (   !is<DetailTextureMaterialLayer>(layer)
            && !is<ShineTextureMaterialLayer>(layer))
        {
            if (layer->isAnimated())
                return true;
        }
    }
    return false;
}

void Material::addLayerAt(Layer *layer, int position)
{
    if (!layer) return;
    if (_layers.contains(layer)) return;

    position = de::clamp(0, position, layerCount());

    d->maybeCancelTextureDimensionsChangeNotification();

    _layers.insert(position, layer);

    if (!d->haveValidDimensions())
    {
        if (res::Texture *tex = d->inheritDimensionsTexture())
        {
            tex->audienceForDeletion         += d;
            tex->audienceForDimensionsChange += d;
        }
    }
}

AudioEnvironmentId Material::audioEnvironment() const
{
    return (isDrawable()? d->audioEnvironment : AE_NONE);
}

void Material::setAudioEnvironment(AudioEnvironmentId newEnvironment)
{
    d->audioEnvironment = newEnvironment;
}

/*Material::Layer &Material::layer(int index) const
{
    /// @throw Material::MissingLayerError  Invalid layer reference.
    throw MissingLayerError("Material::layer", "Unknown layer #" + String::asText(index));
}*/

String Material::describe() const
{
    return "Material \"" + manifest().composeUri().asText() + "\"";
}

String Material::description() const
{
    String str = String(_E(l) "Dimensions: ") + _E(.) + (d->haveValidDimensions()? dimensions().asText() : "unknown (not yet prepared)")
               + _E(l) + " Source: "     + _E(.) + manifest().sourceDescription()
               + _E(l) + "\nDrawable: "  + _E(.) + DE_BOOL_YESNO(isDrawable())
               + _E(l) + " SkyMasked: "  + _E(.) + DE_BOOL_YESNO(isSkyMasked());

    // Add the layer config:
    for (const Layer *layer : _layers)
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
        short f = short(_flags);
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
    DE_UNUSED(src);

    res::Uri search = res::Uri::fromUserInput(&argv[1], argc - 1);
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
    catch (const Resources::MissingResourceManifestError &er)
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
