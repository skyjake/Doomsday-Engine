/** @file texture.cpp  Logical texture resource.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
#include "resource/texture.h"

#include "dd_main.h" // App_ResourceSystem()
#include "con_main.h"

#include "resource/resourcesystem.h"
#include "resource/compositetexture.h"
#include "TextureManifest"

#include <de/Error>
#include <de/Log>
#include <de/memory.h>
#include <QMap>

using namespace de;

typedef QMap<Texture::AnalysisId, void *> Analyses;

DENG2_PIMPL(Texture)
{
    TextureManifest &manifest;
    Flags flags;

#ifdef __CLIENT__
    /// Set of (render-) context variants.
    Variants variants;
#endif

    /// User data associated with this texture.
    void *userData;

    /// World dimensions in map coordinate space units.
    Vector2i dimensions;

    /// World origin offset in map coordinate space units.
    Vector2i origin;

    /// Image analysis data, used for various purposes according to context.
    Analyses analyses;

    Instance(Public *i, TextureManifest &_manifest)
        : Base(i)
        , manifest(_manifest)
        , userData(0)
    {}

    ~Instance()
    {
        self.clearAnalyses();
#ifdef __CLIENT__
        self.clearVariants();
#endif
    }

    /// Notify iterested parties of a change in world dimensions.
    void notifyDimensionsChanged()
    {
        DENG2_FOR_PUBLIC_AUDIENCE(DimensionsChange, i) i->textureDimensionsChanged(self);
    }
};

Texture::Texture(TextureManifest &manifest) : d(new Instance(this, manifest))
{
    setFlags(manifest.flags());
    setDimensions(manifest.logicalDimensions());
    setOrigin(manifest.origin());
}

Texture::~Texture()
{
    DENG2_FOR_AUDIENCE(Deletion, i) i->textureBeingDeleted(*this);

#ifdef __CLIENT__
    /// @todo ResourceSystem should observe Deletion.
    App_ResourceSystem().releaseGLTexturesFor(*this);
#endif

    if(!manifest().schemeName().compareWithoutCase("Textures"))
    {
        CompositeTexture *pcTex = reinterpret_cast<CompositeTexture *>(userDataPointer());
        if(pcTex) delete pcTex;
    }

    delete d;
}

TextureManifest &Texture::manifest() const
{
    return d->manifest;
}

void Texture::setUserDataPointer(void *newUserData)
{
    if(d->userData && newUserData)
    {
        LOG_AS("Texture::setUserDataPointer");
        LOG_DEBUG("User data already present for \"%s\" [%p], will be replaced.")
            << d->manifest.composeUri() << de::dintptr(this);
    }
    d->userData = newUserData;
}

void *Texture::userDataPointer() const
{
    return d->userData;
}

Vector2i const &Texture::dimensions() const
{
    return d->dimensions;
}

void Texture::setDimensions(Vector2i const &_newDimensions)
{
    Vector2i newDimensions = _newDimensions.max(Vector2i(0, 0));
    if(d->dimensions != newDimensions)
    {
        d->dimensions = newDimensions;
        d->notifyDimensionsChanged();
    }
}

void Texture::setWidth(int newWidth)
{
    if(d->dimensions.x != newWidth)
    {
        d->dimensions.x = newWidth;
        d->notifyDimensionsChanged();
    }
}

void Texture::setHeight(int newHeight)
{
    if(d->dimensions.y != newHeight)
    {
        d->dimensions.y = newHeight;
        d->notifyDimensionsChanged();
    }
}

Vector2i const &Texture::origin() const
{
    return d->origin;
}

void Texture::setOrigin(Vector2i const &newOrigin)
{
    if(d->origin != newOrigin)
    {
        d->origin = newOrigin;
    }
}

#ifdef __CLIENT__

uint Texture::variantCount() const
{
    return uint(d->variants.count());
}

Texture::Variant *Texture::chooseVariant(ChooseVariantMethod method,
    TextureVariantSpec const &spec, bool canCreate)
{
    foreach(Variant *variant, d->variants)
    {
        TextureVariantSpec const &cand = variant->spec();
        switch(method)
        {
        case MatchSpec:
            if(&cand == &spec)
            {
                // This is the one we're looking for.
                return variant;
            }
            break;

        case FuzzyMatchSpec:
            if(cand == spec)
            {
                // This will do fine.
                return variant;
            }
            break;
        }
    }

#ifdef DENG_DEBUG
    // 07/04/2011 dj: The "fuzzy selection" features are yet to be implemented.
    // As such, the following should NOT return a valid variant iff the rest of
    // this subsystem has been implemented correctly.
    //
    // Presently this is used as a sanity check.
    if(method == MatchSpec)
    {
        DENG_ASSERT(!chooseVariant(FuzzyMatchSpec, spec));
    }
#endif

    if(!canCreate) return 0;

    d->variants.push_back(new Variant(*this, spec));
    return d->variants.back();
}

Texture::Variant *Texture::prepareVariant(TextureVariantSpec const &spec)
{
    Variant *variant = chooseVariant(MatchSpec, spec, true /*can create*/);
    DENG2_ASSERT(variant);
    variant->prepare();
    return variant;
}

Texture::Variants const &Texture::variants() const
{
    return d->variants;
}

void Texture::clearVariants()
{
    while(!d->variants.isEmpty())
    {
        Texture::Variant *variant = d->variants.takeFirst();

#ifdef DENG_DEBUG
        if(variant->glName())
        {
            String textualVariantSpec = variant->spec().asText();

            LOG_AS("Texture::clearVariants")
            LOG_WARNING("GLName (%i) still set for a variant of \"%s\" [%p]. Perhaps it wasn't released?")
                << variant->glName() << d->manifest.composeUri()
                << de::dintptr(this) << textualVariantSpec;
        }
#endif

        delete variant;
    }
}

#endif // __CLIENT__

void Texture::clearAnalyses()
{
    foreach(void *data, d->analyses)
    {
        M_Free(data);
    }
    d->analyses.clear();
}

void *Texture::analysisDataPointer(AnalysisId analysisId) const
{
    if(!d->analyses.contains(analysisId)) return 0;
    return d->analyses.value(analysisId);
}

void Texture::setAnalysisDataPointer(AnalysisId analysisId, void *newData)
{
    LOG_AS("Texture::attachAnalysis");
    void *existingData = analysisDataPointer(analysisId);
    if(existingData)
    {
#if _DEBUG
        if(newData)
        {
            LOG_DEBUG("Image analysis (id:%i) already present for \"%s\", (will replace).")
                << int(analysisId) << d->manifest.composeUri();
        }
#endif
        M_Free(existingData);
    }
    d->analyses.insert(analysisId, newData);
}

Texture::Flags Texture::flags() const
{
    return d->flags;
}

void Texture::setFlags(Texture::Flags flagsToChange, FlagOp operation)
{
    applyFlagOperation(d->flags, flagsToChange, operation);
}

String Texture::description() const
{
    String str = String("Texture \"%1\"").arg(manifest().composeUri().asText());
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

D_CMD(InspectTexture)
{
    DENG2_UNUSED(src);

    de::Uri search = de::Uri::fromUserInput(&argv[1], argc - 1);

    if(!search.scheme().isEmpty() &&
       !App_ResourceSystem().knownTextureScheme(search.scheme()))
    {
        LOG_WARNING("Unknown scheme %s") << search.scheme();
        return false;
    }

    try
    {
        TextureManifest &manifest = App_ResourceSystem().textureManifest(search);
        if(manifest.hasTexture())
        {
            Texture &texture = manifest.texture();
            String variantCountText;
#ifdef __CLIENT__
            variantCountText = de::String(" (x%1)").arg(texture.variantCount());
#endif

            LOG_MSG("Texture " _E(1) "%s" _E(.) "%s"
                    "\n" _E(l) "Dimensions: " _E(.) _E(i) "%s" _E(.)
                     " " _E(l) "Source: "     _E(.) _E(i) "%s")
                << manifest.composeUri()
                << variantCountText
                << (texture.width() == 0 &&
                    texture.height() == 0? String("unknown (not yet prepared)")
                                         : texture.dimensions().asText())
                << manifest.sourceDescription();

#if defined(__CLIENT__) && defined(DENG_DEBUG)
            if(texture.variantCount())
            {
                // Print variant specs.
                LOG_MSG(_E(R));

                int variantIdx = 0;
                foreach(TextureVariant *variant, texture.variants())
                {
                    Vector2f coords;
                    variant->glCoords(&coords.x, &coords.y);

                    String textualVariantSpec = variant->spec().asText();

                    LOG_MSG(_E(D) "Variant #%i:" _E(.)
                            " " _E(l) "Source: " _E(.) _E(i) "%s" _E(.)
                            " " _E(l) "Masked: " _E(.) _E(i) "%s" _E(.)
                            " " _E(l) "GLName: " _E(.) _E(i) "%i" _E(.)
                            " " _E(l) "Coords: " _E(.) _E(i) "%s" _E(.)
                            _E(R)
                            "\n" _E(1) "Specification:" _E(.) "%s")
                        << variantIdx
                        << variant->sourceDescription()
                        << (variant->isMasked()? "yes":"no")
                        << variant->glName()
                        << coords.asText()
                        << textualVariantSpec;

                    ++variantIdx;
                }
            }
#endif // __CLIENT__ && DENG_DEBUG
        }
        else
        {
            LOG_MSG("%s") << manifest.description();
        }
        return true;
    }
    catch(ResourceSystem::MissingManifestError const &er)
    {
        LOG_WARNING("%s.") << er.asText();
    }
    return false;
}

void Texture::consoleRegister() // static
{
    C_CMD("inspecttexture", "ss",   InspectTexture)
    C_CMD("inspecttexture", "s",    InspectTexture)
}
