/**
 * @file texture.c
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

#include <ctype.h>
#include <cstring>

#include "de_base.h"
#include "gl_texmanager.h"
#include "texturevariant.h"
#include "texture.h"
#include <de/Error>
#include <de/Log>
#include <de/LegacyCore>
#include <de/memory.h>

de::Texture::Texture(textureid_t bindId, void* _userData)
    : flags(), primaryBindId(bindId), variants(), userData(_userData), dimensions()
{
    memset(analyses, 0, sizeof(analyses));
}

de::Texture::Texture(textureid_t bindId, const Size2Raw& size, void* _userData)
    : flags(), primaryBindId(bindId), variants(), userData(_userData), dimensions()
{
    memset(analyses, 0, sizeof(analyses));
    setSize(size);
}

de::Texture::~Texture()
{
    clearVariants();
}

void de::Texture::clearVariants()
{
    DENG2_FOR_EACH(i, variants, Variants::iterator)
    {
#if _DEBUG
        unsigned int glName = (*i)->glName();
        if(glName)
        {
            textureid_t textureId = Textures_Id(reinterpret_cast<struct texture_s*>(this));
            Uri* uri = Textures_ComposeUri(textureId);
            ddstring_t* path = Uri_ToString(uri);
            LOG_AS("Texture::clearVariants")
            LOG_WARNING("GLName (%i) still set for a variant of \"%s\" (id:%i). Perhaps it wasn't released?")
                << glName << Str_Text(path) << int(textureId);
            GL_PrintTextureVariantSpecification((*i)->spec());
            Str_Delete(path);
            Uri_Delete(uri);
        }
#endif
        delete *i;
    }
    variants.clear();
}

void de::Texture::setPrimaryBind(textureid_t bindId)
{
    primaryBindId = bindId;
}

void de::Texture::setUserDataPointer(void* newUserData)
{
    if(userData)
    {
        textureid_t textureId = Textures_Id(reinterpret_cast<struct texture_s*>(this));
        LOG_AS("Texture::setUserDataPointer");
        LOG_DEBUG("User data is already present for [%p id:%i], it will be replaced.")
            << (void*)this, int(textureId);
    }
    userData = newUserData;
}

void* de::Texture::userDataPointer() const
{
    return userData;
}

uint de::Texture::variantCount() const
{
    return uint(variants.size());
}

de::TextureVariant& de::Texture::addVariant(de::TextureVariant& variant)
{
    variants.push_back(&variant);
    return *variants.back();
}

void de::Texture::flagCustom(bool yes)
{
    if(yes) flags |= Custom; else flags &= ~Custom;
    /// @todo Update any Materials (and thus Surfaces) which reference this.
}

int de::Texture::width() const
{
    return dimensions.width;
}

void de::Texture::setWidth(int newWidth)
{
    dimensions.width = newWidth;
    /// @todo Update any Materials (and thus Surfaces) which reference this.
}

int de::Texture::height() const
{
    return dimensions.height;
}

void de::Texture::setHeight(int newHeight)
{
    dimensions.height = newHeight;
    /// @todo Update any Materials (and thus Surfaces) which reference this.
}

void de::Texture::setSize(const Size2Raw& newSize)
{
    dimensions.width  = newSize.width;
    dimensions.height = newSize.height;
    /// @todo Update any Materials (and thus Surfaces) which reference this.
}

void* de::Texture::analysis(texture_analysisid_t analysisId) const
{
    DENG2_ASSERT(VALID_TEXTURE_ANALYSISID(analysisId));
    return analyses[analysisId];
}

void de::Texture::setAnalysisDataPointer(texture_analysisid_t analysisId, void* data)
{
    DENG2_ASSERT(VALID_TEXTURE_ANALYSISID(analysisId));
    if(analyses[analysisId])
    {
#if _DEBUG
        textureid_t textureId = Textures_Id(reinterpret_cast<struct texture_s*>(this));
        Uri* uri = Textures_ComposeUri(textureId);
        ddstring_t* path = Uri_ToString(uri);
        LOG_AS("Texture::attachAnalysis");
        LOG_WARNING("Image analysis #%i already present for \"%s\", will replace.")
            << int(analysisId), Str_Text(path);
        Str_Delete(path);
        Uri_Delete(uri);
#endif
    }
    analyses[analysisId] = data;
}

/**
 * C Wrapper API:
 */

#define TOINTERNAL(inst) \
    (inst) != 0? reinterpret_cast<de::Texture*>(inst) : NULL

#define TOINTERNAL_CONST(inst) \
    (inst) != 0? reinterpret_cast<const de::Texture*>(inst) : NULL

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    de::Texture* self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    const de::Texture* self = TOINTERNAL_CONST(inst)

Texture* Texture_NewWithSize(textureid_t bindId, const Size2Raw* size, void* userData)
{
    if(!size)
        LegacyCore_FatalError("Texture_NewWithSize: Attempted with invalid size argument (=NULL).");
    return reinterpret_cast<Texture*>(new de::Texture(bindId, *size, userData));
}

Texture* Texture_New(textureid_t bindId, void* userData)
{
    return reinterpret_cast<Texture*>(new de::Texture(bindId, userData));
}

void Texture_Delete(Texture* tex)
{
    if(tex)
    {
        SELF(tex);
        delete self;
    }
}

textureid_t Texture_PrimaryBind(const Texture* tex)
{
    SELF_CONST(tex);
    return self->primaryBind();
}

void Texture_SetPrimaryBind(Texture* tex, textureid_t bindId)
{
    SELF(tex);
    self->setPrimaryBind(bindId);
}

void* Texture_UserDataPointer(const Texture* tex)
{
    SELF_CONST(tex);
    return self->userDataPointer();
}

void Texture_SetUserDataPointer(Texture* tex, void* newUserData)
{
    SELF(tex);
    self->setUserDataPointer(newUserData);
}

void Texture_ClearVariants(Texture* tex)
{
    SELF(tex);
    self->clearVariants();
}

uint Texture_VariantCount(const Texture* tex)
{
    SELF_CONST(tex);
    return self->variantCount();
}

TextureVariant* Texture_AddVariant(Texture* tex, TextureVariant* variant)
{
    SELF(tex);
    if(!variant)
    {
        LOG_AS("Texture_AddVariant");
        LOG_WARNING("Invalid variant argument, ignoring.");
        return variant;
    }
    self->addVariant(*reinterpret_cast<de::TextureVariant*>(variant));
    return variant;
}

void* Texture_AnalysisDataPointer(const Texture* tex, texture_analysisid_t analysis)
{
    SELF_CONST(tex);
    return self->analysis(analysis);
}

void Texture_SetAnalysisDataPointer(Texture* tex, texture_analysisid_t analysis, void* data)
{
    SELF(tex);
    self->setAnalysisDataPointer(analysis, data);
}

boolean Texture_IsCustom(const Texture* tex)
{
    SELF_CONST(tex);
    return CPP_BOOL(self->isCustom());
}

void Texture_FlagCustom(Texture* tex, boolean yes)
{
    SELF(tex);
    self->flagCustom(bool(yes));
}

void Texture_SetSize(Texture* tex, const Size2Raw* newSize)
{
    SELF(tex);
    if(!newSize)
        LegacyCore_FatalError("Texture_SetSize: Attempted with invalid newSize argument (=NULL).");
    self->setSize(*newSize);
}

int Texture_Width(const Texture* tex)
{
    SELF_CONST(tex);
    return self->width();
}

void Texture_SetWidth(Texture* tex, int newWidth)
{
    SELF(tex);
    self->setWidth(newWidth);
}

int Texture_Height(const Texture* tex)
{
    SELF_CONST(tex);
    return self->height();
}

void Texture_SetHeight(Texture* tex, int newHeight)
{
    SELF(tex);
    self->setHeight(newHeight);
}

int Texture_IterateVariants(struct texture_s* tex,
    int (*callback)(struct texturevariant_s* variant, void* parameters), void* parameters)
{
    SELF(tex);
    int result = 0;
    if(callback)
    {
        DENG2_FOR_EACH(i, self->variantList(), de::Texture::Variants::const_iterator)
        {
            de::TextureVariant* variant = const_cast<de::TextureVariant*>(*i);
            result = callback(reinterpret_cast<TextureVariant*>(variant), parameters);
            if(result) break;
        }
    }
    return result;
}
