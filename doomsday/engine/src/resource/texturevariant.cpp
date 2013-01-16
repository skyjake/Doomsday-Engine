/** @file texturevariant.cpp Logical Texture Variant.
 *
 * @author Copyright &copy; 2011-2013 Daniel Swanson <danij@dengine.net>
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

//#include "de_base.h"
#include "resource/texture.h"

namespace de {

struct Texture::Variant::Instance
{
    /// Superior Texture of which this is a derivative.
    Texture &texture;

    /// Specification used to derive this variant.
    texturevariantspecification_t &spec;

    /// Variant flags.
    Texture::Variant::Flags flags;

    /// Source of this texture.
    TexSource texSource;

    /// Name of the associated GL texture object.
    uint glTexName;

    /// Prepared coordinates for the bottom right of the texture minus border.
    float s, t;

    Instance(Texture &generalCase, texturevariantspecification_t &spec)
        : texture(generalCase), spec(spec),
          flags(0), texSource(TEXS_NONE),
          glTexName(0), s(0), t(0)
    {}
};

Texture::Variant::Variant(Texture &generalCase,
    texturevariantspecification_t &spec, TexSource source)
{
    d = new Instance(generalCase, spec);
    setSource(source);
}

Texture::Variant::~Variant()
{
    delete d;
}

Texture &Texture::Variant::generalCase() const
{
    return d->texture;
}

texturevariantspecification_t &Texture::Variant::spec() const
{
    return d->spec;
}

TexSource Texture::Variant::source() const
{
    return d->texSource;
}

void Texture::Variant::setSource(TexSource newSource)
{
    d->texSource = newSource;
}

bool Texture::Variant::isMasked() const
{
    return !!(d->flags & Masked);
}

void Texture::Variant::flagMasked(bool yes)
{
    if(yes) d->flags |= Masked;
    else    d->flags &= ~Masked;
}

void Texture::Variant::coords(float *outS, float *outT) const
{
    if(outS) *outS = d->s;
    if(outT) *outT = d->t;
}

void Texture::Variant::setCoords(float newS, float newT)
{
    d->s = newS;
    d->t = newT;
}

bool Texture::Variant::isUploaded() const
{
    return !!(d->flags & Uploaded);
}

void Texture::Variant::flagUploaded(bool yes)
{
    if(yes) d->flags |= Uploaded;
    else    d->flags &= ~Uploaded;
}

bool Texture::Variant::isPrepared() const
{
    return isUploaded() && d->glTexName;
}

uint Texture::Variant::glName() const
{
    return d->glTexName;
}

void Texture::Variant::setGLName(uint newGLName)
{
    d->glTexName = newGLName;
}

} // namespace de
