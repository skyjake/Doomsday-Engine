/**
 * @file texturevariant.cpp
 * Logical texture variant. @ingroup resource
 *
 * @authors Copyright &copy; 2013 Daniel Swanson <danij@dengine.net>
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

namespace de {

Texture::Variant::Variant(Texture &generalCase,
    texturevariantspecification_t &spec, TexSource source)
    : texture(generalCase),
      texSource(source),
      flags(0),
      glTexName(0),
      s(0),
      t(0),
      varSpec(&spec)
{}

texturevariantspecification_t &Texture::Variant::spec() const
{
    return *varSpec;
}

void Texture::Variant::setSource(TexSource newSource)
{
    texSource = newSource;
}

void Texture::Variant::flagMasked(bool yes)
{
    if(yes) flags |= Masked; else flags &= ~Masked;
}

void Texture::Variant::flagUploaded(bool yes)
{
    if(yes) flags |= Uploaded; else flags &= ~Uploaded;
}

bool Texture::Variant::isPrepared() const
{
    return isUploaded() && 0 != glTexName;
}

void Texture::Variant::coords(float *outS, float *outT) const
{
    if(outS) *outS = s;
    if(outT) *outT = t;
}

void Texture::Variant::setCoords(float newS, float newT)
{
    s = newS;
    t = newT;
}

void Texture::Variant::setGLName(uint newGLName)
{
    glTexName = newGLName;
}

} // namespace de
