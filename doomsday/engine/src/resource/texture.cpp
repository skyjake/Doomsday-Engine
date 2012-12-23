/** @file texture.cpp Logical Texture.
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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
#include "resource/texturevariant.h"
#include "resource/texture.h"

#include <de/Error>
#include <de/Log>
#include <de/LegacyCore>
#include <de/memory.h>

namespace de {

struct Texture::Instance
{
    Texture::Flags flags;

    /// Unique identifier of the primary binding in the owning collection.
    TextureManifest &manifest;

    /// List of variants (e.g., color translations).
    Texture::Variants variants;

    /// User data associated with this texture.
    void *userData;

    /// World dimensions in map coordinate space units.
    QSize dimensions;

    /// World origin offset in map coordinate space units.
    QPoint origin;

    /// Table of analyses object ptrs, used for various purposes depending
    /// on the variant specification.
    void *analyses[TEXTURE_ANALYSIS_COUNT];

    Instance(TextureManifest &_manifest, void *_userData)
        : manifest(_manifest), userData(_userData)
    {
        std::memset(analyses, 0, sizeof(analyses));
    }
};

Texture::Texture(TextureManifest &_manifest, void *_userData)
{
    d = new Instance(_manifest, _userData);
}

Texture::~Texture()
{
    GL_ReleaseGLTexturesByTexture(reinterpret_cast<texture_s *>(this));

    /// @todo Texture should employ polymorphism.
    TextureScheme const &scheme = d->manifest.scheme();
    if(!scheme.name().compareWithoutCase("Textures"))
    {
        CompositeTexture *pcTex = reinterpret_cast<CompositeTexture *>(userDataPointer());
        if(pcTex) delete pcTex;
    }

    clearAnalyses();
    clearVariants();
}

TextureManifest &Texture::manifest() const
{
    return d->manifest;
}

void Texture::clearAnalyses()
{
    for(uint i = uint(TEXTURE_ANALYSIS_FIRST); i < uint(TEXTURE_ANALYSIS_COUNT); ++i)
    {
        texture_analysisid_t analysis = texture_analysisid_t(i);
        void* data = analysisDataPointer(analysis);
        if(data) M_Free(data);
        setAnalysisDataPointer(analysis, 0);
    }
}

void Texture::clearVariants()
{
    DENG2_FOR_EACH(Variants, i, d->variants)
    {
#if _DEBUG
        uint glName = (*i)->glName();
        if(glName)
        {
            LOG_AS("Texture::clearVariants")
            LOG_WARNING("GLName (%i) still set for a variant of \"%s\" [%p]. Perhaps it wasn't released?")
                << glName << d->manifest.composeUri() << (void *)this;
            GL_PrintTextureVariantSpecification((*i)->spec());
        }
#endif
        delete *i;
    }
    d->variants.clear();
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
    return uint(d->variants.size());
}

TextureVariant &Texture::addVariant(TextureVariant &variant)
{
    d->variants.push_back(&variant);
    return *d->variants.back();
}

int Texture::width() const
{
    return d->dimensions.width();
}

void Texture::setWidth(int newWidth)
{
    d->dimensions.setWidth(newWidth);
    /// @todo Update any Materials (and thus Surfaces) which reference this.
}

int Texture::height() const
{
    return d->dimensions.height();
}

QSize const &Texture::dimensions() const
{
    return d->dimensions;
}

void Texture::setHeight(int newHeight)
{
    d->dimensions.setHeight(newHeight);
    /// @todo Update any Materials (and thus Surfaces) which reference this.
}

void Texture::setDimensions(QSize const &newDimensions)
{
    d->dimensions = newDimensions;
    /// @todo Update any Materials (and thus Surfaces) which reference this.
}

QPoint const &Texture::origin() const
{
    return d->origin;
}

void Texture::setOrigin(QPoint const &newOrigin)
{
    d->origin = newOrigin;
}

Texture::Variants const &Texture::variantList() const
{
    return d->variants;
}

void *Texture::analysisDataPointer(texture_analysisid_t analysisId) const
{
    DENG2_ASSERT(VALID_TEXTURE_ANALYSISID(analysisId));
    return d->analyses[analysisId];
}

void Texture::setAnalysisDataPointer(texture_analysisid_t analysisId, void *data)
{
    DENG2_ASSERT(VALID_TEXTURE_ANALYSISID(analysisId));
    if(d->analyses[analysisId] && data)
    {
#if _DEBUG
        LOG_AS("Texture::attachAnalysis");
        LOG_DEBUG("Image analysis (id:%i) already present for \"%s\" (replaced).")
            << int(analysisId) << d->manifest.composeUri();
#endif
    }
    d->analyses[analysisId] = data;
}

Texture::Flags const &Texture::flags() const
{
    return d->flags;
}

Texture::Flags &Texture::flags()
{
    return d->flags;
}

} // namespace de

/**
 * C Wrapper API:
 */

#define TOINTERNAL(inst) \
    (inst) != 0? reinterpret_cast<de::Texture *>(inst) : NULL

#define TOINTERNAL_CONST(inst) \
    (inst) != 0? reinterpret_cast<const de::Texture *>(inst) : NULL

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    de::Texture *self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    de::Texture const *self = TOINTERNAL_CONST(inst)

void *Texture_UserDataPointer(Texture const *tex)
{
    SELF_CONST(tex);
    return self->userDataPointer();
}

void Texture_SetUserDataPointer(Texture *tex, void *newUserData)
{
    SELF(tex);
    self->setUserDataPointer(newUserData);
}

void Texture_ClearVariants(Texture *tex)
{
    SELF(tex);
    self->clearVariants();
}

uint Texture_VariantCount(Texture const *tex)
{
    SELF_CONST(tex);
    return self->variantCount();
}

struct texturevariant_s *Texture_AddVariant(Texture *tex, struct texturevariant_s *variant)
{
    SELF(tex);
    if(!variant)
    {
        LOG_AS("Texture_AddVariant");
        LOG_WARNING("Invalid variant argument, ignoring.");
        return variant;
    }
    self->addVariant(*reinterpret_cast<de::TextureVariant *>(variant));
    return variant;
}

void *Texture_AnalysisDataPointer(Texture const *tex, texture_analysisid_t analysisId)
{
    SELF_CONST(tex);
    return self->analysisDataPointer(analysisId);
}

void Texture_SetAnalysisDataPointer(Texture *tex, texture_analysisid_t analysis, void *data)
{
    SELF(tex);
    self->setAnalysisDataPointer(analysis, data);
}

int Texture_Width(Texture const *tex)
{
    SELF_CONST(tex);
    return self->width();
}

void Texture_SetWidth(Texture *tex, int newWidth)
{
    SELF(tex);
    self->setWidth(newWidth);
}

int Texture_Height(Texture const *tex)
{
    SELF_CONST(tex);
    return self->height();
}

void Texture_SetHeight(Texture *tex, int newHeight)
{
    SELF(tex);
    self->setHeight(newHeight);
}

int Texture_IterateVariants(Texture *tex,
    int (*callback)(struct texturevariant_s *variant, void *parameters), void *parameters)
{
    SELF(tex);
    int result = 0;
    if(callback)
    {
        DENG2_FOR_EACH_CONST(de::Texture::Variants, i, self->variantList())
        {
            de::TextureVariant *variant = const_cast<de::TextureVariant *>(*i);
            result = callback(reinterpret_cast<texturevariant_s *>(variant), parameters);
            if(result) break;
        }
    }
    return result;
}
