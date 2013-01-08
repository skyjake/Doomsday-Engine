/** @file material.cpp Logical Material.
 *
 * @author Copyright &copy; 2009-2013 Daniel Swanson <danij@dengine.net>
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

#include <cstring>

#include "de_base.h"
#include "de_resource.h"
#include "de_render.h"

#include "audio/s_environ.h"
#include "gl/sys_opengl.h" // TODO: get rid of this
#include "map/r_world.h"
#include "sys_system.h" // var: novideo

#include "resource/material.h"

struct material_s
{
    /// DMU object header.
    runtime_mapdata_header_t header;

    /// Definition from which this material was derived (if any).
    ded_material_t *def;

    /// Set of use-case/context variant instances.
    de::Material::Variants variants;

    /// Environmental sound class.
    material_env_class_t envClass;

    /// Unique identifier of the MaterialBind associated with this Material or @c NULL if not bound.
    materialid_t primaryBind;

    /// World dimensions in map coordinate space units.
    Size2 *dimensions;

    /// @see materialFlags
    short flags;

    /// @c true if belongs to some animgroup.
    bool inAnimGroup;
    bool isCustom;

    /// Detail texture layer & properties.
    de::Texture *detailTex;
    float detailScale;
    float detailStrength;

    /// Shiny texture layer & properties.
    de::Texture *shinyTex;
    blendmode_t shinyBlendmode;
    float shinyMinColor[3];
    float shinyStrength;
    de::Texture *shinyMaskTex;

    /// Current prepared state.
    byte prepared;

    material_s(short _flags, bool _isCustom, ded_material_t *_def,
               Size2Raw &_dimensions, material_env_class_t _envClass)
        : def(_def), envClass(_envClass), primaryBind(0),
          dimensions(Size2_NewFromRaw(&_dimensions)), flags(_flags),
          inAnimGroup(false), isCustom(_isCustom),
          detailTex(0), detailScale(0), detailStrength(0),
          shinyTex(0), shinyBlendmode(BM_ADD), shinyStrength(0), shinyMaskTex(0)
    {
        header.type = DMU_MATERIAL;
        std::memset(shinyMinColor, 0, sizeof(shinyMinColor));
    }

    ~material_s()
    {
        clearVariants();
        Size2_Delete(dimensions);
    }

    void clearVariants()
    {
        while(!variants.isEmpty())
        {
             delete variants.takeFirst();
        }
        prepared = 0;
    }

    /**
     * Add a new variant to the list of resources for this Material.
     * Material takes ownership of the variant.
     *
     * @param variant  Variant instance to add to the list.
     */
    void addVariant(de::MaterialVariant &variant)
    {
        variants.push_back(&variant);
    }
};

namespace de {

MaterialAnim::MaterialAnim(int _id, int _flags)
    : id_(_id), flags_(_flags),
      index(0), maxTimer(0), timer(0)
{}

int MaterialAnim::id() const
{
    return id_;
}

int MaterialAnim::flags() const
{
    return flags_;
}

int MaterialAnim::frameCount() const
{
    return frames.count();
}

MaterialAnim::Frame &MaterialAnim::frame(int number)
{
    if(number < 0 || number >= frameCount())
    {
        /// @throw InvalidFrameError Attempt to access an invalid frame.
        throw InvalidFrameError("MaterialAnim::frame", QString("Invalid frame #%1, valid range [0..%2)").arg(number).arg(frameCount()));
    }
    return frames[number];
}

void MaterialAnim::addFrame(material_t &mat, int tics, int randomTics)
{
    LOG_AS("MaterialAnim::addFrame");

    // Mark the material as being part of an animation group.
    Material_SetGroupAnimated(&mat, true);

    // Allocate a new frame.
    frames.push_back(Frame(mat, tics, randomTics));
}

bool MaterialAnim::hasFrameForMaterial(material_t const &mat) const
{
    DENG2_FOR_EACH_CONST(Frames, i, frames)
    {
        if(&i->material() == &mat)
            return true;
    }
    return false;
}

MaterialAnim::Frames const &MaterialAnim::allFrames() const
{
    return frames;
}

static void setVariantTranslation(MaterialVariant &variant, material_t *current, material_t *next)
{
    MaterialVariantSpec const &spec = variant.spec();
    MaterialVariant *currentV, *nextV;

    currentV = Material_ChooseVariant(current, spec, false, true/*create if necessary*/);
    nextV    = Material_ChooseVariant(next,    spec, false, true/*create if necessary*/);
    variant.setTranslation(currentV, nextV);
}

void MaterialAnim::animate()
{
    if(frames.isEmpty()) return;

    if(--timer <= 0)
    {
        // Advance to next frame.
        index = (index + 1) % frames.count();
        Frame const &nextFrame = frames[index];
        int newTimer = nextFrame.tics();

        if(nextFrame.randomTics())
        {
            newTimer += int(RNG_RandByte()) % (nextFrame.randomTics() + 1);
        }
        timer = maxTimer = newTimer;

        // Update translations.
        for(int i = 0; i < frames.count(); ++i)
        {
            material_t *current = &frames[(index + i    ) % frames.count()].material();
            material_t *next    = &frames[(index + i + 1) % frames.count()].material();

            Material::Variants const &variants = Material_Variants(&frames[i].material());
            DENG2_FOR_EACH_CONST(Material::Variants, k, variants)
            {
                setVariantTranslation(**k, current, next);
            }

            // Surfaces using this material may need to be updated.
            R_UpdateMapSurfacesOnMaterialChange(&frames[i].material());

            // Just animate the first in the sequence?
            if(flags_ & AGF_FIRST_ONLY) break;
        }
        return;
    }

    // Update the interpolation point of animated group members.
    DENG2_FOR_EACH_CONST(Frames, i, frames)
    {
        /*ded_material_t *def = Material_Definition(mat);
        if(def && def->layers[0].stageCount.num > 1)
        {
            de::Uri *texUri = reinterpret_cast<de::Uri *>(def->layers[0].stages[0].texture)
            if(texUri && Textures::resolveUri(*texUri))
                continue; // Animated elsewhere.
        }*/

        float interp;
        if(flags_ & AGF_SMOOTH)
        {
            interp = 1 - timer / float( maxTimer );
        }
        else
        {
            interp = 0;
        }

        Material::Variants const &variants = Material_Variants(&i->material());
        DENG2_FOR_EACH_CONST(Material::Variants, k, variants)
        {
            (*k)->setTranslationPoint(interp);
        }

        // Just animate the first in the sequence?
        if(flags_ & AGF_FIRST_ONLY) break;
    }
}

void MaterialAnim::reset()
{
    if(frames.isEmpty()) return;

    timer = 0;
    maxTimer = 1;

    // The animation should restart from the first step using the correct timings.
    index = frames.count() - 1;
}

} // namespace de

using namespace de;

material_t *Material_New(short flags, boolean isCustom, ded_material_t *def,
    Size2Raw *dimensions, material_env_class_t envClass)
{
    return new material_s(flags, CPP_BOOL(isCustom), def, *dimensions, envClass);
}

void Material_Delete(material_t *mat)
{
    if(mat)
    {
        delete (material_s *) mat;
    }
}

void Material_Ticker(material_t *mat, timespan_t time)
{
    DENG2_ASSERT(mat);
    DENG2_FOR_EACH(Material::Variants, i, mat->variants)
    {
        (*i)->ticker(time);
    }
}

ded_material_t *Material_Definition(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return mat->def;
}

void Material_SetDefinition(material_t *mat, struct ded_material_s *def)
{
    DENG2_ASSERT(mat);
    if(mat->def != def)
    {
        mat->def = def;

        // Textures are updated automatically at prepare-time, so just clear them.
        Material_SetDetailTexture(mat, NULL);
        Material_SetShinyTexture(mat, NULL);
        Material_SetShinyMaskTexture(mat, NULL);
    }

    if(!mat->def) return;

    mat->flags = mat->def->flags;

    Size2Raw size(def->width, def->height);
    Material_SetDimensions(mat, &size);

    Material_SetEnvironmentClass(mat, S_MaterialEnvClassForUri(def->uri));

    // Update custom status.
    /// @todo This should take into account the whole definition, not just whether
    ///       the primary layer's first texture is custom or not.
    mat->isCustom = false;
    if(def->layers[0].stageCount.num > 0 && def->layers[0].stages[0].texture)
    {
        de::Uri *texUri = reinterpret_cast<de::Uri *>(def->layers[0].stages[0].texture);
        try
        {
            TextureManifest &manifest = App_Textures()->find(*texUri);
            if(Texture *tex = manifest.texture())
            {
                mat->isCustom = tex->flags().testFlag(Texture::Custom);
            }
        }
        catch(Textures::NotFoundError const &)
        {} // Ignore this error.
    }
}

Size2 const *Material_Dimensions(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return mat->dimensions;
}

void Material_SetDimensions(material_t* mat, const Size2Raw* newSize)
{
    DENG2_ASSERT(mat);
    if(!newSize) return;

    Size2 *size = Size2_NewFromRaw(newSize);
    if(!Size2_Equality(mat->dimensions, size))
    {
        Size2_SetWidthHeight(mat->dimensions, newSize->width, newSize->height);
        R_UpdateMapSurfacesOnMaterialChange(mat);
    }
    Size2_Delete(size);
}

int Material_Width(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return Size2_Width(mat->dimensions);
}

void Material_SetWidth(material_t *mat, int width)
{
    DENG2_ASSERT(mat);
    if(Size2_Width(mat->dimensions) == width) return;
    Size2_SetWidth(mat->dimensions, width);
    R_UpdateMapSurfacesOnMaterialChange(mat);
}

int Material_Height(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return Size2_Height(mat->dimensions);
}

void Material_SetHeight(material_t *mat, int height)
{
    DENG2_ASSERT(mat);
    if(Size2_Height(mat->dimensions) == height) return;
    Size2_SetHeight(mat->dimensions, height);
    R_UpdateMapSurfacesOnMaterialChange(mat);
}

short Material_Flags(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return mat->flags;
}

void Material_SetFlags(material_t *mat, short flags)
{
    DENG2_ASSERT(mat);
    mat->flags = flags;
}

boolean Material_IsCustom(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return boolean( mat->isCustom );
}

boolean Material_IsAnimated(material_t const *mat)
{
    if(Material_IsGroupAnimated(mat)) return true;

    // Perhaps stage animated?
    if(mat->def)
    {
        int const layerCount = Material_LayerCount(mat);
        for(int i = 0; i < layerCount; ++i)
        {
            ded_material_layer_t const &layer = mat->def->layers[i];
            if(layer.stageCount.num > 1) return true;
        }
    }
    return false; // Not at all.
}

boolean Material_IsGroupAnimated(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return boolean( mat->inAnimGroup );
}

boolean Material_IsSkyMasked(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return 0 != (mat->flags & MATF_SKYMASK);
}

boolean Material_IsDrawable(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return 0 == (mat->flags & MATF_NO_DRAW);
}

boolean Material_HasGlow(material_t *mat)
{
    if(novideo) return false;

    /// @todo We should not need to prepare to determine this.
    MaterialSnapshot const &ms =
        App_Materials()->prepare(*mat, Rend_MapSurfaceMaterialSpec(), true);

    return (ms.glowStrength() > .0001f);
}

boolean Material_HasTranslation(material_t const *mat)
{
    DENG2_ASSERT(mat);
    /// @todo Separate meanings.
    return Material_IsGroupAnimated(mat);
}

int Material_LayerCount(material_t const *mat)
{
    DENG2_ASSERT(mat);
    DENG2_UNUSED(mat);
    return 1;
}

de::MaterialAnim &Material_AnimGroup(material_t *mat)
{
    if(Material_IsGroupAnimated(mat))
    {
        Materials::AnimGroups const &allAnims = App_Materials()->allAnimGroups();
        DENG2_FOR_EACH_CONST(Materials::AnimGroups, i, allAnims)
        {
            MaterialAnim &anim = const_cast<MaterialAnim &>(*i);
            // Is this material in this animation?
            if(!anim.hasFrameForMaterial(*mat)) continue;

            return anim;
        }
    }

    /// @throw Material::NoAnimGroupError The material is not group-animated.
    throw Material::NoAnimGroupError("Material_AnimGroup", QString("Material [%1] is not group-animated")
                                                               .arg(de::dintptr(mat)));
}

void Material_SetGroupAnimated(material_t *mat, boolean yes)
{
    DENG2_ASSERT(mat);
    mat->inAnimGroup = CPP_BOOL(yes);
}

byte Material_Prepared(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return mat->prepared;
}

void Material_SetPrepared(material_t *mat, byte state)
{
    DENG2_ASSERT(mat && state <= 2);
    mat->prepared = state;
}

materialid_t Material_PrimaryBind(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return mat->primaryBind;
}

void Material_SetPrimaryBind(material_t *mat, materialid_t bindId)
{
    DENG2_ASSERT(mat);
    mat->primaryBind = bindId;
}

material_env_class_t Material_EnvironmentClass(material_t const *mat)
{
    DENG2_ASSERT(mat);
    if(!Material_IsDrawable(mat))
        return MEC_UNKNOWN;
    return mat->envClass;
}

void Material_SetEnvironmentClass(material_t *mat, material_env_class_t envClass)
{
    DENG2_ASSERT(mat);
    mat->envClass = envClass;
}

struct texture_s *Material_DetailTexture(material_t *mat)
{
    DENG2_ASSERT(mat);
    return reinterpret_cast<struct texture_s *>(mat->detailTex);
}

void Material_SetDetailTexture(material_t *mat, struct texture_s *tex)
{
    DENG2_ASSERT(mat);
    mat->detailTex = reinterpret_cast<de::Texture *>(tex);
}

float Material_DetailStrength(material_t *mat)
{
    DENG2_ASSERT(mat);
    return mat->detailStrength;
}

void Material_SetDetailStrength(material_t *mat, float strength)
{
    DENG2_ASSERT(mat);
    mat->detailStrength = MINMAX_OF(0, strength, 1);
}

float Material_DetailScale(material_t *mat)
{
    DENG2_ASSERT(mat);
    return mat->detailScale;
}

void Material_SetDetailScale(material_t *mat, float scale)
{
    DENG2_ASSERT(mat);
    mat->detailScale = MINMAX_OF(0, scale, 1);
}

struct texture_s *Material_ShinyTexture(material_t *mat)
{
    DENG2_ASSERT(mat);
    return reinterpret_cast<struct texture_s *>(mat->shinyTex);
}

void Material_SetShinyTexture(material_t *mat, struct texture_s *tex)
{
    DENG2_ASSERT(mat);
    mat->shinyTex = reinterpret_cast<de::Texture *>(tex);
}

blendmode_t Material_ShinyBlendmode(material_t *mat)
{
    DENG2_ASSERT(mat);
    return mat->shinyBlendmode;
}

void Material_SetShinyBlendmode(material_t *mat, blendmode_t blendmode)
{
    DENG2_ASSERT(mat && VALID_BLENDMODE(blendmode));
    mat->shinyBlendmode = blendmode;
}

float const *Material_ShinyMinColor(material_t *mat)
{
    DENG2_ASSERT(mat);
    return mat->shinyMinColor;
}

void Material_SetShinyMinColor(material_t *mat, float const colorRGB[3])
{
    DENG2_ASSERT(mat && colorRGB);
    mat->shinyMinColor[CR] = MINMAX_OF(0, colorRGB[CR], 1);
    mat->shinyMinColor[CG] = MINMAX_OF(0, colorRGB[CG], 1);
    mat->shinyMinColor[CB] = MINMAX_OF(0, colorRGB[CB], 1);
}

float Material_ShinyStrength(material_t *mat)
{
    DENG2_ASSERT(mat);
    return mat->shinyStrength;
}

void Material_SetShinyStrength(material_t *mat, float strength)
{
    DENG2_ASSERT(mat);
    mat->shinyStrength = MINMAX_OF(0, strength, 1);
}

struct texture_s *Material_ShinyMaskTexture(material_t *mat)
{
    DENG2_ASSERT(mat);
    return reinterpret_cast<struct texture_s *>(mat->shinyMaskTex);
}

void Material_SetShinyMaskTexture(material_t *mat, struct texture_s *tex)
{
    DENG2_ASSERT(mat);
    mat->shinyMaskTex = reinterpret_cast<de::Texture *>(tex);
}

Material::Variants const &Material_Variants(material_t const *mat)
{
    return mat->variants;
}

MaterialVariant *Material_ChooseVariant(material_t *mat,
    MaterialVariantSpec const &spec, bool smoothed, bool canCreate)
{
    DENG_ASSERT(mat);

    MaterialVariant *variant = 0;
    DENG2_FOR_EACH_CONST(Material::Variants, i, mat->variants)
    {
        MaterialVariantSpec const &cand = (*i)->spec();
        if(cand.compare(spec))
        {
            // This will do fine.
            variant = *i;
            break;
        }
    }

    if(!variant)
    {
        if(!canCreate) return 0;

        variant = new MaterialVariant(*mat, spec, *Material_Definition(mat));
        mat->addVariant(*variant);
    }

    if(smoothed)
    {
        variant = variant->translationCurrent();
    }

    return variant;
}

int Material_VariantCount(material_t const *mat)
{
    DENG_ASSERT(mat);
    return mat->variants.count();
}

void Material_ClearVariants(material_t *mat)
{
    DENG_ASSERT(mat);
    mat->clearVariants();
}

boolean Material_HasDecorations(material_t *mat)
{
    if(novideo) return false;

    /// @todo We should not need to prepare to determine this.
    if(!mat->prepared)
    {
        App_Materials()->prepare(*mat, Rend_MapSurfaceMaterialSpec(), false);
    }
    MaterialBind &bind = *App_Materials()->toMaterialBind(mat->primaryBind);
    if(bind.decorationDef()) return true;

    if(Material_IsGroupAnimated(mat))
    {
        // If any material in this animation has decorations then this material
        // is considered to be decorated also.
        MaterialAnim &anim = Material_AnimGroup(mat);
        DENG2_FOR_EACH_CONST(MaterialAnim::Frames, i, anim.allFrames())
        {
            material_t *otherMaterial = &i->material();
            if(otherMaterial == mat) continue; // Do not test the same material again.

            if(Material_HasDecorations(otherMaterial)) return true;
        }
    }

    return false;
}

int Material_GetProperty(material_t const *mat, setargs_t *args)
{
    DENG_ASSERT(mat && args);
    switch(args->prop)
    {
    case DMU_FLAGS: {
        short flags = Material_Flags(mat);
        DMU_GetValue(DMT_MATERIAL_FLAGS, &flags, args, 0);
        break; }

    case DMU_WIDTH: {
        int width = Material_Width(mat);
        DMU_GetValue(DMT_MATERIAL_WIDTH, &width, args, 0);
        break; }

    case DMU_HEIGHT: {
        int height = Material_Height(mat);
        DMU_GetValue(DMT_MATERIAL_HEIGHT, &height, args, 0);
        break; }

    default: {
        QByteArray msg = String("Material_GetProperty: No property %1.").arg(DMU_Str(args->prop)).toUtf8();
        LegacyCore_FatalError(msg.constData());
        return 0; /* Unreachable */ }
    }
    return false; // Continue iteration.
}

int Material_SetProperty(material_t *mat, setargs_t const *args)
{
    DENG_UNUSED(mat);
    QByteArray msg = String("Material_SetProperty: Property '%1' is not writable.").arg(DMU_Str(args->prop)).toUtf8();
    LegacyCore_FatalError(msg.constData());
    return 0; // Unreachable.
}
