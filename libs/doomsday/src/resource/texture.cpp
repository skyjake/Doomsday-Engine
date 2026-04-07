/** @file texture.cpp  Logical texture resource.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/res/texture.h"
#include "doomsday/res/resources.h"
#include "doomsday/res/composite.h"
#include "doomsday/res/texturemanifest.h"
#include "doomsday/res/textures.h"
#include "doomsday/console/cmd.h"

#include <de/error.h>
#include <de/log.h>
#include <de/legacy/memory.h>
#include <de/keymap.h>

using namespace de;

namespace res {

typedef Hash<int /*Texture::AnalysisId*/, void *> Analyses;

DE_PIMPL(Texture)
{
    TextureManifest &manifest;
    Flags flags;

    /// User data associated with this texture.
    void *userData;

    /// World dimensions in map coordinate space units.
    Vec2ui dimensions;

    /// World origin offset in map coordinate space units.
    Vec2i origin;

    /// Image analysis data, used for various purposes according to context.
    Analyses analyses;

    Impl(Public *i, TextureManifest &_manifest)
        : Base(i)
        , manifest(_manifest)
        , userData(0)
    {}

    ~Impl()
    {
        self().clearAnalyses();
    }

    /// Notify iterested parties of a change in world dimensions.
    void notifyDimensionsChanged()
    {
        DE_NOTIFY_PUBLIC_VAR(DimensionsChange, i) i->textureDimensionsChanged(self());
    }
};

Texture::Texture(TextureManifest &manifest) : d(new Impl(this, manifest))
{
    setFlags(manifest.flags());
    setDimensions(manifest.logicalDimensions());
    setOrigin(manifest.origin());
}

Texture::~Texture()
{
    DE_NOTIFY_VAR(Deletion, i) i->textureBeingDeleted(*this);

    if (!manifest().schemeName().compareWithoutCase("Textures"))
    {
        Composite *pcTex = reinterpret_cast<Composite *>(userDataPointer());
        if (pcTex) delete pcTex;
    }
}

TextureManifest &Texture::manifest() const
{
    return d->manifest;
}

void Texture::setUserDataPointer(void *newUserData)
{
    if (d->userData && newUserData)
    {
        LOG_AS("Texture::setUserDataPointer");
        LOGDEV_RES_MSG("User data already present for \"%s\" %p, will be replaced")
            << d->manifest.composeUri() << this;
    }
    d->userData = newUserData;
}

void *Texture::userDataPointer() const
{
    return d->userData;
}

const Vec2ui &Texture::dimensions() const
{
    return d->dimensions;
}

void Texture::setDimensions(const Vec2ui &newDimensions)
{
    if (d->dimensions != newDimensions)
    {
        d->dimensions = newDimensions;
        d->notifyDimensionsChanged();
    }
}

void Texture::setWidth(duint newWidth)
{
    if (d->dimensions.x != newWidth)
    {
        d->dimensions.x = newWidth;
        d->notifyDimensionsChanged();
    }
}

void Texture::setHeight(duint newHeight)
{
    if (d->dimensions.y != newHeight)
    {
        d->dimensions.y = newHeight;
        d->notifyDimensionsChanged();
    }
}

const Vec2i &Texture::origin() const
{
    return d->origin;
}

void Texture::setOrigin(const Vec2i &newOrigin)
{
    if (d->origin != newOrigin)
    {
        d->origin = newOrigin;
    }
}

void Texture::release(/*TextureVariantSpec *spec*/)
{}

void Texture::clearAnalyses()
{
    for (const auto &i : d->analyses)
    {
        M_Free(i.second);
    }
    d->analyses.clear();
}

void *Texture::analysisDataPointer(AnalysisId analysisId) const
{
    const auto &analyses = d.getConst()->analyses;
    auto found = analyses.find(analysisId);
    if (found != analyses.end())
    {
        return found->second;
    }
    return nullptr;
}

void Texture::setAnalysisDataPointer(AnalysisId analysisId, void *newData)
{
    LOG_AS("Texture::attachAnalysis");
    void *existingData = analysisDataPointer(analysisId);
    if (existingData)
    {
#ifdef DE_DEBUG
        if (newData)
        {
            LOGDEV_RES_VERBOSE("Image analysis (id:%i) already present for \"%s\", will be replaed")
                << int(analysisId) << d->manifest.composeUri();
        }
#endif
        M_Free(existingData);
    }
    d->analyses.insert(analysisId, newData);
}

Flags Texture::flags() const
{
    return d->flags;
}

void Texture::setFlags(Flags flagsToChange, FlagOp operation)
{
    applyFlagOperation(d->flags, flagsToChange, operation);
}

String Texture::description() const
{
    String str = Stringf("Texture " _E(b) "%s" _E(.), manifest().composeUri().asText().c_str());
#ifdef DE_DEBUG
    str += Stringf(" [addr:%p]", this);
#endif
    str += _E(l) " Dimensions:" _E(.)
        +  (width() == 0 && height() == 0? String("unknown (not yet prepared)")
                                         : dimensions().asText())
        +  _E(l) " Source:" _E(.) + manifest().sourceDescription();
    return str;
}

D_CMD(InspectTexture)
{
    DE_UNUSED(src);

    res::Uri search = res::Uri::fromUserInput(&argv[1], argc - 1);

    if (!search.scheme().isEmpty() &&
        !Resources::get().textures().isKnownTextureScheme(search.scheme()))
    {
        LOG_RES_WARNING("Unknown scheme %s") << search.scheme();
        return false;
    }

    try
    {
        TextureManifest &manifest = Resources::get().textures().textureManifest(search);
        if (manifest.hasTexture())
        {
            //Texture &texture = ;
            //String variantCountText;

            LOG_RES_MSG("%s") << manifest.texture().description();

            /*LOG_RES_MSG("Texture " _E(b) "%s" _E(.) "%s"
                        "\n" _E(l) "Dimensions: " _E(.) _E(i) "%s" _E(.)
                        " " _E(l) "Source: "     _E(.) _E(i) "%s")
                << manifest.composeUri()
                << variantCountText
                << (texture.width() == 0 &&
                    texture.height() == 0? String("unknown (not yet prepared)")
                                         : texture.dimensions().asText())
                << manifest.sourceDescription();*/
        }
        else
        {
            LOG_RES_MSG("%s") << manifest.description();
        }
        return true;
    }
    catch (const Resources::MissingResourceManifestError &er)
    {
        LOG_RES_WARNING("%s.") << er.asText();
    }
    return false;
}

void Texture::consoleRegister() // static
{
    C_CMD("inspecttexture", "ss",   InspectTexture)
    C_CMD("inspecttexture", "s",    InspectTexture)
}

} // namespace res
