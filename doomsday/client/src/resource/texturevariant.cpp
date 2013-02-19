/** @file texturevariant.cpp Logical Texture Variant.
 *
 * @authors Copyright © 2011-2013 Daniel Swanson <danij@dengine.net>
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

#include "Texture"

/**
 * @todo Magnification, Anisotropic filter level and GL texture wrap modes
 * will be handled through dynamic changes to GL's texture environment state.
 * Consequently they should be ignored here.
 */
static int compareVariantSpecifications(variantspecification_t const &a,
    variantspecification_t const &b)
{
    /// @todo We can be a bit cleverer here...
    if(a.context != b.context) return 0;
    if(a.flags != b.flags) return 0;
    if(a.wrapS != b.wrapS || a.wrapT != b.wrapT) return 0;
    //if(a.magFilter != b.magFilter) return 0;
    //if(a.anisoFilter != b.anisoFilter) return 0;
    if(a.mipmapped != b.mipmapped) return 0;
    if(a.noStretch != b.noStretch) return 0;
    if(a.gammaCorrection != b.gammaCorrection) return 0;
    if(a.toAlpha != b.toAlpha) return 0;
    if(a.border != b.border) return 0;
    if(a.flags & TSF_HAS_COLORPALETTE_XLAT)
    {
        colorpalettetranslationspecification_t const *cptA = a.translated;
        colorpalettetranslationspecification_t const *cptB = b.translated;
        DENG_ASSERT(cptA && cptB);
        if(cptA->tClass != cptB->tClass) return 0;
        if(cptA->tMap   != cptB->tMap) return 0;
    }
    return 1; // Equal.
}

static int compareDetailVariantSpecifications(detailvariantspecification_t const &a,
    detailvariantspecification_t const &b)
{
    if(a.contrast != b.contrast) return 0;
    return 1; // Equal.
}

int TextureVariantSpec_Compare(texturevariantspecification_t const *a,
    texturevariantspecification_t const *b)
{
    DENG_ASSERT(a && b);
    if(a == b) return 1;
    if(a->type != b->type) return 0;
    switch(a->type)
    {
    case TST_GENERAL: return compareVariantSpecifications(TS_GENERAL(*a), TS_GENERAL(*b));
    case TST_DETAIL:  return compareDetailVariantSpecifications(TS_DETAIL(*a), TS_DETAIL(*b));
    }
    throw de::Error("TextureVariantSpec_Compare", QString("Invalid type %1").arg(a->type));
}

namespace de {

DENG2_PIMPL(Texture::Variant)
{
    /// Superior Texture of which this is a derivative.
    Texture &texture;

    /// Specification used to derive this variant.
    texturevariantspecification_t const &spec;

    /// Variant flags.
    Texture::Variant::Flags flags;

    /// Source of this texture.
    TexSource texSource;

    /// Name of the associated GL texture object.
    uint glTexName;

    /// Prepared coordinates for the bottom right of the texture minus border.
    float s, t;

    Instance(Public &a, Texture &generalCase,
             texturevariantspecification_t const &spec) : Base(a),
      texture(generalCase),
      spec(spec),
      flags(0),
      texSource(TEXS_NONE),
      glTexName(0),
      s(0),
      t(0)
    {}
};

Texture::Variant::Variant(Texture &generalCase, texturevariantspecification_t const &spec)
    : d(new Instance(*this, generalCase, spec))
{}

Texture::Variant::~Variant()
{
    delete d;
}

Texture &Texture::Variant::generalCase() const
{
    return d->texture;
}

texturevariantspecification_t const &Texture::Variant::spec() const
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

Texture::Variant::Flags Texture::Variant::flags() const
{
    return d->flags;
}

void Texture::Variant::setFlags(Texture::Variant::Flags flagsToChange, bool set)
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

#ifdef __CLIENT__
uint Texture::Variant::glName() const
{
    return d->glTexName;
}

void Texture::Variant::setGLName(uint newGLName)
{
    d->glTexName = newGLName;
}
#endif

} // namespace de
