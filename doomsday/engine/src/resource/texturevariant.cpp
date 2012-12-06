/**
 * @file texturevariant.cpp
 * Logical texture variant. @ingroup resource
 *
 * @authors Copyright &copy; 2012 Daniel Swanson <danij@dengine.net>
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
#include "resource/texture.h"
#include "resource/texturevariant.h"
#include <de/LegacyCore>

namespace de {

TextureVariant::TextureVariant(Texture &generalCase,
    texturevariantspecification_t &spec, TexSource source)
    : texture(generalCase),
      texSource(source),
      flags(0),
      glTexName(0),
      s(0),
      t(0),
      varSpec(&spec)
{}

void TextureVariant::setSource(TexSource newSource)
{
    texSource = newSource;
}

void TextureVariant::flagMasked(bool yes)
{
    if(yes) flags |= Masked; else flags &= ~Masked;
}

void TextureVariant::flagUploaded(bool yes)
{
    if(yes) flags |= Uploaded; else flags &= ~Uploaded;
}

bool TextureVariant::isPrepared() const
{
    return isUploaded() && 0 != glTexName;
}

void TextureVariant::coords(float *outS, float *outT) const
{
    if(outS) *outS = s;
    if(outT) *outT = t;
}

void TextureVariant::setCoords(float newS, float newT)
{
    s = newS;
    t = newT;
}

void TextureVariant::setGLName(uint newGLName)
{
    glTexName = newGLName;
}

} // namespace de

/**
 * C Wrapper API:
 */

#define TOINTERNAL(inst) \
    (inst) != 0? reinterpret_cast<de::TextureVariant *>(inst) : NULL

#define TOINTERNAL_CONST(inst) \
    (inst) != 0? reinterpret_cast<de::TextureVariant const *>(inst) : NULL

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    de::TextureVariant *self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    de::TextureVariant const *self = TOINTERNAL_CONST(inst)

TextureVariant *TextureVariant_New(Texture *generalCase,
    texturevariantspecification_t *spec, TexSource source)
{
    if(!generalCase) LegacyCore_FatalError("TextureVariant_New: Attempted with invalid generalCase argument (=NULL).");
    if(!spec) LegacyCore_FatalError("TextureVariant_New: Attempted with invalid spec argument (=NULL).");

    return reinterpret_cast<TextureVariant*>(new de::TextureVariant(reinterpret_cast<de::Texture &>(*generalCase), *spec, source));
}

void TextureVariant_Delete(TextureVariant *tex)
{
    if(tex)
    {
        SELF(tex);
        delete self;
    }
}

struct texture_s *TextureVariant_GeneralCase(TextureVariant const *tex)
{
    SELF_CONST(tex);
    return reinterpret_cast<texture_s *>(&self->generalCase());
}

TexSource TextureVariant_Source(TextureVariant const *tex)
{
    SELF_CONST(tex);
    return self->source();
}

void TextureVariant_SetSource(TextureVariant *tex, TexSource source)
{
    SELF(tex);
    self->setSource(source);
}

boolean TextureVariant_IsMasked(TextureVariant const *tex)
{
    SELF_CONST(tex);
    return CPP_BOOL(self->isMasked());
}

void TextureVariant_FlagMasked(TextureVariant *tex, boolean yes)
{
    SELF(tex);
    self->flagMasked(bool(yes));
}

boolean TextureVariant_IsUploaded(TextureVariant const *tex)
{
    SELF_CONST(tex);
    return CPP_BOOL(self->isUploaded());
}

void TextureVariant_FlagUploaded(TextureVariant *tex, boolean yes)
{
    SELF(tex);
    self->flagUploaded(bool(yes));
}

boolean TextureVariant_IsPrepared(TextureVariant const *tex)
{
    SELF_CONST(tex);
    return CPP_BOOL(self->isPrepared());
}

void TextureVariant_Coords(TextureVariant const *tex, float *s, float *t)
{
    SELF_CONST(tex);
    self->coords(s, t);
}

void TextureVariant_SetCoords(TextureVariant *tex, float s, float t)
{
    SELF(tex);
    self->setCoords(s, t);
}

texturevariantspecification_t *TextureVariant_Spec(TextureVariant const *tex)
{
    SELF_CONST(tex);
    return self->spec();
}

uint TextureVariant_GLName(TextureVariant const *tex)
{
    SELF_CONST(tex);
    return self->glName();
}

void TextureVariant_SetGLName(TextureVariant *tex, uint glName)
{
    SELF(tex);
    self->setGLName(glName);
}
