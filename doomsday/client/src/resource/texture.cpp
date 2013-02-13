/** @file texture.cpp Logical Texture.
 *
 * @authors Copyright © 2003-2013 Jaakko Ker√§nen <jaakko.keranen@iki.fi>
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

#include <cctype>
#include <cstring>

#include "de_base.h"
#include "gl/gl_texmanager.h"
#include "resource/compositetexture.h"
#include <de/Error>
#include <de/Log>
#include <QMap>

#include "Texture"

typedef QMap<de::Texture::AnalysisId, void *> Analyses;

namespace de {

DENG2_PIMPL(Texture)
{
    /// Manifest derived to yield the texture.
    TextureManifest &manifest;

    Texture::Flags flags;

    /// List of variants (e.g., color translations).
    Texture::Variants variants;

    /// User data associated with this texture.
    void *userData;

    /// World dimensions in map coordinate space units.
    QSize dimensions;

    /// World origin offset in map coordinate space units.
    QPoint origin;

    /// Image analysis data, used for various purposes according to context.
    Analyses analyses;

    Instance(Public &a, TextureManifest &_manifest)
        : Base(a), manifest(_manifest), userData(0)
    {}

    ~Instance()
    {
        self.clearAnalyses();
        clearVariants();
    }

    void clearVariants()
    {
        while(!variants.isEmpty())
        {
            Texture::Variant *variant = variants.takeFirst();
#if _DEBUG
            uint glName = variant->glName();
            if(glName)
            {
                LOG_AS("Texture::clearVariants")
                LOG_WARNING("GLName (%i) still set for a variant of \"%s\" [%p]. Perhaps it wasn't released?")
                    << glName << manifest.composeUri() << (void *)this;
                GL_PrintTextureVariantSpecification(&variant->spec());
            }
#endif
            delete variant;
        }
    }
};

Texture::Texture(TextureManifest &manifest) : d(new Instance(*this, manifest))
{}

Texture::~Texture()
{
    GL_ReleaseGLTexturesByTexture(reinterpret_cast<texture_s *>(this));

    TextureScheme const &scheme = d->manifest.scheme();
    if(!scheme.name().compareWithoutCase("Textures"))
    {
        CompositeTexture *pcTex = reinterpret_cast<CompositeTexture *>(userDataPointer());
        if(pcTex) delete pcTex;
    }

    clearAnalyses();
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

uint Texture::variantCount() const
{
    return uint(d->variants.count());
}

QSize const &Texture::dimensions() const
{
    return d->dimensions;
}

void Texture::setDimensions(QSize const &newDimensions)
{
    d->dimensions = newDimensions;
}

void Texture::setWidth(int newWidth)
{
    d->dimensions.setWidth(newWidth);
}

void Texture::setHeight(int newHeight)
{
    d->dimensions.setHeight(newHeight);
}

QPoint const &Texture::origin() const
{
    return d->origin;
}

void Texture::setOrigin(QPoint const &newOrigin)
{
    d->origin = newOrigin;
}

Texture::Variant *Texture::chooseVariant(ChooseVariantMethod method,
    texturevariantspecification_t const &spec, bool canCreate)
{
    foreach(Variant *variant, d->variants)
    {
        texturevariantspecification_t const &cand = variant->spec();
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
            if(TextureVariantSpec_Compare(&cand, &spec))
            {
                // This will do fine.
                return variant;
            }
            break;
        }
    }
#if _DEBUG
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

Texture::Variants const &Texture::variants() const
{
    return d->variants;
}

void Texture::clearVariants()
{
    d->clearVariants();
}

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

void Texture::setFlags(Texture::Flags flagsToChange, bool set)
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

} // namespace de
