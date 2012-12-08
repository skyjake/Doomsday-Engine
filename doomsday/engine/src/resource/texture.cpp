/**
 * @file texture.cpp
 * Logical texture. @ingroup resource
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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

Texture::Texture(TextureManifest &_manifest, Flags _flags, void *_userData)
    : flags(_flags), manifest_(_manifest), variants(), userData(_userData), dimensions_()
{
    std::memset(analyses, 0, sizeof(analyses));
}

Texture::Texture(TextureManifest &_manifest, QSize const &size, Flags _flags, void *_userData)
    : flags(_flags), manifest_(_manifest), variants(), userData(_userData), dimensions_()
{
    std::memset(analyses, 0, sizeof(analyses));
    setDimensions(size);
}

Texture::~Texture()
{
    GL_ReleaseGLTexturesByTexture(reinterpret_cast<texture_s *>(this));

    /// @todo Texture should employ polymorphism.
    TextureScheme const &scheme = manifest_.scheme();
    if(!scheme.name().compareWithoutCase("Textures"))
    {
        CompositeTexture* pcTex = reinterpret_cast<CompositeTexture *>(userDataPointer());
        if(pcTex) delete pcTex;
    }
    else if(!scheme.name().compareWithoutCase("Sprites"))
    {
        patchtex_t* pTex = reinterpret_cast<patchtex_t *>(userDataPointer());
        if(pTex) M_Free(pTex);
    }
    else if(!scheme.name().compareWithoutCase("Patches"))
    {
        patchtex_t* pTex = reinterpret_cast<patchtex_t *>(userDataPointer());
        if(pTex) M_Free(pTex);
    }

    clearAnalyses();
    clearVariants();
}

TextureManifest &Texture::manifest() const
{
    return manifest_;
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
    DENG2_FOR_EACH(Variants, i, variants)
    {
#if _DEBUG
        uint glName = (*i)->glName();
        if(glName)
        {
            LOG_AS("Texture::clearVariants")
            LOG_WARNING("GLName (%i) still set for a variant of \"%s\" [%p]. Perhaps it wasn't released?")
                << glName << manifest_.composeUri() << (void *)this;
            GL_PrintTextureVariantSpecification((*i)->spec());
        }
#endif
        delete *i;
    }
    variants.clear();
}

void Texture::setUserDataPointer(void *newUserData)
{
    if(userData && newUserData)
    {
        LOG_AS("Texture::setUserDataPointer");
        LOG_DEBUG("User data already present for \"%s\" [%p], will be replaced.")
            << manifest_.composeUri() << de::dintptr(this);
    }
    userData = newUserData;
}

void *Texture::userDataPointer() const
{
    return userData;
}

uint Texture::variantCount() const
{
    return uint(variants.size());
}

TextureVariant &Texture::addVariant(TextureVariant &variant)
{
    variants.push_back(&variant);
    return *variants.back();
}

void Texture::flagCustom(bool yes)
{
    if(yes) flags |= Custom; else flags &= ~Custom;
    /// @todo Update any Materials (and thus Surfaces) which reference this.
}

int Texture::width() const
{
    return dimensions_.width();
}

void Texture::setWidth(int newWidth)
{
    dimensions_.setWidth(newWidth);
    /// @todo Update any Materials (and thus Surfaces) which reference this.
}

int Texture::height() const
{
    return dimensions_.height();
}

void Texture::setHeight(int newHeight)
{
    dimensions_.setHeight(newHeight);
    /// @todo Update any Materials (and thus Surfaces) which reference this.
}

void Texture::setDimensions(QSize const &newDimensions)
{
    dimensions_ = newDimensions;
    /// @todo Update any Materials (and thus Surfaces) which reference this.
}

void *Texture::analysisDataPointer(texture_analysisid_t analysisId) const
{
    DENG2_ASSERT(VALID_TEXTURE_ANALYSISID(analysisId));
    return analyses[analysisId];
}

void Texture::setAnalysisDataPointer(texture_analysisid_t analysisId, void *data)
{
    DENG2_ASSERT(VALID_TEXTURE_ANALYSISID(analysisId));
    if(analyses[analysisId] && data)
    {
#if _DEBUG
        LOG_AS("Texture::attachAnalysis");
        LOG_DEBUG("Image analysis (id:%i) already present for \"%s\" (replaced).")
            << int(analysisId) << manifest_.composeUri();
#endif
    }
    analyses[analysisId] = data;
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

TextureVariant *Texture_AddVariant(Texture *tex, TextureVariant *variant)
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

boolean Texture_IsCustom(Texture const *tex)
{
    SELF_CONST(tex);
    return CPP_BOOL(self->isCustom());
}

void Texture_FlagCustom(Texture *tex, boolean yes)
{
    SELF(tex);
    self->flagCustom(bool(yes));
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
    int (*callback)(TextureVariant *variant, void *parameters), void *parameters)
{
    SELF(tex);
    int result = 0;
    if(callback)
    {
        DENG2_FOR_EACH_CONST(de::Texture::Variants, i, self->variantList())
        {
            de::TextureVariant *variant = const_cast<de::TextureVariant *>(*i);
            result = callback(reinterpret_cast<TextureVariant *>(variant), parameters);
            if(result) break;
        }
    }
    return result;
}
