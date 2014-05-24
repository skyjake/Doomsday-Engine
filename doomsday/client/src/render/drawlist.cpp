/** @file drawlist.cpp  Drawable primitive list.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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
#include "render/drawlist.h"
#include "clientapp.h"
#include "gl/gl_main.h"
#include "render/rend_main.h"
#include "Shard"
#include <QFlags>
#include <QList>

namespace de {
namespace internal {

    /**
     * Drawing condition flags.
     *
     * @todo Most of these are actually list specification parameters. Rather than
     * set them each time an identified list is drawn it would be better to record
     * in the list itself. -ds
     */
    enum DrawCondition
    {
        NoBlend            = 0x00000001,
        Blend              = 0x00000002,
        SetLightEnv0       = 0x00000004,
        SetLightEnv1       = 0x00000008,
        JustOneLight       = 0x00000010,
        ManyLights         = 0x00000020,
        SetBlendMode       = 0x00000040, // Primitive-specific blending.
        SetMatrixDTexture0 = 0x00000080,
        SetMatrixDTexture1 = 0x00000100,
        SetMatrixTexture0  = 0x00000200,
        SetMatrixTexture1  = 0x00000400,
        NoColor            = 0x00000800,

        Skip               = 0x80000000,

        SetLightEnv        = SetLightEnv0 | SetLightEnv1,
        SetMatrixDTexture  = SetMatrixDTexture0 | SetMatrixDTexture1,
        SetMatrixTexture   = SetMatrixTexture0 | SetMatrixTexture1
    };

    Q_DECLARE_FLAGS(DrawConditions, DrawCondition)
    Q_DECLARE_OPERATORS_FOR_FLAGS(DrawConditions)

} // namespace internal
} // namespace de

using namespace de;
using namespace internal;

DENG2_PIMPL_NOREF(DrawList)
{
    Spec spec; ///< Draw state specification.

    // Rewindable list buffer of geometries to be drawn (not owned).
    struct Shards : private QList<Shard const *>
    {
        Shards() : _cursor(0) {
            QList<Shard const *>::reserve(64);
        }

        int size() const { return _cursor; }
        inline bool isEmpty() const { return size() == 0; }

        void clear() {
            QList<Shard const *>::clear();
            rewind();
        }

        void rewind() {
            _cursor = 0;
        }

        void operator << (Shard const &shard) {
            if(_cursor < QList<Shard const *>::size())
                replace(_cursor, &shard);
            else
                append(&shard);
            _cursor += 1;
        }

        Shard const &operator [] (int idx) const {
            DENG2_ASSERT(idx < size());
            return *at(idx);
        }

    private:
        int _cursor;
    } shards;

    Instance(Spec const &spec) : spec(spec) {}

    /**
     * Configure GL state for drawing in this @a mode.
     *
     * @return  The conditions to select primitives.
     */
    DrawConditions pushGLState(DrawMode mode)
    {
        switch(mode)
        {
        case DM_SKYMASK:
            DENG2_ASSERT(spec.group == SkyMaskGeom);

            // Render all primitives on the list without discrimination.
            return NoColor;

        case DM_ALL: // All surfaces.
            DENG2_ASSERT(spec.group == UnlitGeom || spec.group == LitGeom);

            // Should we do blending?
            if(spec.unit(TU_INTER).hasTexture())
            {
                // Blend between two textures, modulate with primary color.
                DENG2_ASSERT(numTexUnits >= 2);
                GL_SelectTexUnits(2);

                GL_BindTo(spec.unit(TU_PRIMARY), 0);
                GL_BindTo(spec.unit(TU_INTER), 1);
                GL_ModulateTexture(2);

                float color[4] = { 0, 0, 0, spec.unit(TU_INTER).opacity };
                glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
            }
            else if(!spec.unit(TU_PRIMARY).hasTexture())
            {
                // Opaque texture-less surface.
                return 0;
            }
            else
            {
                // Normal modulation.
                GL_SelectTexUnits(1);
                GL_Bind(spec.unit(TU_PRIMARY));
                GL_ModulateTexture(1);
            }

            if(spec.unit(TU_INTER).hasTexture())
            {
                return SetMatrixTexture0 | SetMatrixTexture1;
            }
            return SetMatrixTexture0;

        case DM_LIGHT_MOD_TEXTURE:
            DENG2_ASSERT(spec.group == LitGeom);

            // Modulate sector light, dynamic light and regular texture.
            GL_BindTo(spec.unit(TU_PRIMARY), 1);
            return SetMatrixTexture1 | SetLightEnv0 | JustOneLight | NoBlend;

        case DM_TEXTURE_PLUS_LIGHT:
            DENG2_ASSERT(spec.group == LitGeom);

            GL_BindTo(spec.unit(TU_PRIMARY), 0);
            return SetMatrixTexture0 | SetLightEnv1 | NoBlend;

        case DM_FIRST_LIGHT:
            DENG2_ASSERT(spec.group == LitGeom);

            // Draw all primitives with more than one light
            // and all primitives which will have a blended texture.
            return SetLightEnv0 | ManyLights | Blend;

        case DM_BLENDED: {
            DENG2_ASSERT(spec.group == UnlitGeom || spec.group == LitGeom);

            // Only render the blended surfaces.
            if(!spec.unit(TU_INTER).hasTexture())
            {
                return Skip;
            }

            DENG2_ASSERT(numTexUnits >= 2);
            GL_SelectTexUnits(2);
            GL_BindTo(spec.unit(TU_PRIMARY), 0);
            GL_BindTo(spec.unit(TU_INTER), 1);
            GL_ModulateTexture(2);

            float color[4] = { 0, 0, 0, spec.unit(TU_INTER).opacity };
            glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
            return SetMatrixTexture0 | SetMatrixTexture1; }

        case DM_BLENDED_FIRST_LIGHT:
            DENG2_ASSERT(spec.group == LitGeom);

            // Only blended surfaces.
            if(!spec.unit(TU_INTER).hasTexture())
            {
                return Skip;
            }
            return SetMatrixTexture1 | SetLightEnv0;

        case DM_WITHOUT_TEXTURE:
            DENG2_ASSERT(spec.group == LitGeom);

            // Only render geometries affected by dynlights.
            return 0;

        case DM_LIGHTS:
            DENG2_ASSERT(spec.group == LightGeom);

            // These lists only contain light geometries.
            GL_Bind(spec.unit(TU_PRIMARY));
            return 0;

        case DM_BLENDED_MOD_TEXTURE:
            DENG2_ASSERT(spec.group == LitGeom);

            // Blending required.
            if(!spec.unit(TU_INTER).hasTexture())
            {
                break;
            }

            // Intentional fall-through.

        case DM_MOD_TEXTURE:
        case DM_MOD_TEXTURE_MANY_LIGHTS:
            DENG2_ASSERT(spec.group == LitGeom);

            // Texture for surfaces with (many) dynamic lights.
            // Should we do blending?
            if(spec.unit(TU_INTER).hasTexture())
            {
                // Mode 3 actually just disables the second texture stage,
                // which would modulate with primary color.
                DENG2_ASSERT(numTexUnits >= 2);
                GL_SelectTexUnits(2);
                GL_BindTo(spec.unit(TU_PRIMARY), 0);
                GL_BindTo(spec.unit(TU_INTER), 1);
                GL_ModulateTexture(3);

                float color[4] = { 0, 0, 0, spec.unit(TU_INTER).opacity };
                glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
                // Render all geometry.
                return SetMatrixTexture0 | SetMatrixTexture1;
            }
            // No modulation at all.
            GL_SelectTexUnits(1);
            GL_Bind(spec.unit(TU_PRIMARY));
            GL_ModulateTexture(0);
            if(mode == DM_MOD_TEXTURE_MANY_LIGHTS)
            {
                return SetMatrixTexture0 | ManyLights;
            }
            return SetMatrixTexture0;

        case DM_UNBLENDED_MOD_TEXTURE_AND_DETAIL:
            DENG2_ASSERT(spec.group == LitGeom);

            // Blending is not done now.
            if(spec.unit(TU_INTER).hasTexture())
            {
                break;
            }

            if(spec.unit(TU_PRIMARY_DETAIL).hasTexture())
            {
                GL_SelectTexUnits(2);
                GL_ModulateTexture(9); // Tex+Detail, no color.
                GL_BindTo(spec.unit(TU_PRIMARY), 0);
                GL_BindTo(spec.unit(TU_PRIMARY_DETAIL), 1);
                return SetMatrixTexture0 | SetMatrixDTexture1;
            }
            else
            {
                GL_SelectTexUnits(1);
                GL_ModulateTexture(0);
                GL_Bind(spec.unit(TU_PRIMARY));
                return SetMatrixTexture0;
            }
            break;

        case DM_ALL_DETAILS:
            DENG2_ASSERT(spec.group == UnlitGeom || spec.group == LitGeom);

            if(spec.unit(TU_PRIMARY_DETAIL).hasTexture())
            {
                GL_Bind(spec.unit(TU_PRIMARY_DETAIL));
                return SetMatrixDTexture0;
            }
            break;

        case DM_UNBLENDED_TEXTURE_AND_DETAIL:
            DENG2_ASSERT(spec.group == UnlitGeom || spec.group == LitGeom);

            // Only unblended. Details are optional.
            if(spec.unit(TU_INTER).hasTexture())
            {
                break;
            }

            if(spec.unit(TU_PRIMARY_DETAIL).hasTexture())
            {
                GL_SelectTexUnits(2);
                GL_ModulateTexture(8);
                GL_BindTo(spec.unit(TU_PRIMARY), 0);
                GL_BindTo(spec.unit(TU_PRIMARY_DETAIL), 1);
                return SetMatrixTexture0 | SetMatrixDTexture1;
            }
            else
            {
                // Normal modulation.
                GL_SelectTexUnits(1);
                GL_ModulateTexture(1);
                GL_Bind(spec.unit(TU_PRIMARY));
                return SetMatrixTexture0;
            }
            break;

        case DM_BLENDED_DETAILS: {
            DENG2_ASSERT(spec.group == UnlitGeom || spec.group == LitGeom);

            // We'll only render blended primitives.
            if(!spec.unit(TU_INTER).hasTexture())
            {
                break;
            }

            if(!spec.unit(TU_PRIMARY_DETAIL).hasTexture() ||
               !spec.unit(TU_INTER_DETAIL).hasTexture())
            {
                break;
            }

            GL_BindTo(spec.unit(TU_PRIMARY_DETAIL), 0);
            GL_BindTo(spec.unit(TU_INTER_DETAIL), 1);

            float color[4] = { 0, 0, 0, spec.unit(TU_INTER_DETAIL).opacity };
            glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
            return SetMatrixDTexture0 | SetMatrixDTexture1; }

        case DM_SHADOW:
            DENG2_ASSERT(spec.group == ShadowGeom);

            if(spec.unit(TU_PRIMARY).hasTexture())
            {
                GL_Bind(spec.unit(TU_PRIMARY));
            }
            else
            {
                GL_BindTextureUnmanaged(0);
            }

            if(!spec.unit(TU_PRIMARY).hasTexture())
            {
                // Apply a modelview shift.
                glMatrixMode(GL_MODELVIEW);
                glPushMatrix();

                // Scale towards the viewpoint to avoid Z-fighting.
                glTranslatef(vOrigin.x, vOrigin.y, vOrigin.z);
                glScalef(.99f, .99f, .99f);
                glTranslatef(-vOrigin.x, -vOrigin.y, -vOrigin.z);
            }
            return 0;

        case DM_MASKED_SHINY:
            DENG2_ASSERT(spec.group == ShineGeom);

            if(spec.unit(TU_INTER).hasTexture())
            {
                GL_SelectTexUnits(2);
                // The intertex holds the info for the mask texture.
                GL_BindTo(spec.unit(TU_INTER), 1);
                float color[4] = { 0, 0, 0, 1 };
                glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
            }

            // Intentional fall-through.

        case DM_ALL_SHINY:
        case DM_SHINY:
            DENG2_ASSERT(spec.group == ShineGeom);

            GL_BindTo(spec.unit(TU_PRIMARY), 0);
            if(!spec.unit(TU_INTER).hasTexture())
            {
                GL_SelectTexUnits(1);
            }

            // Render all primitives.
            if(mode == DM_ALL_SHINY)
            {
                return SetBlendMode;
            }
            if(mode == DM_MASKED_SHINY)
            {
                return SetBlendMode | SetMatrixTexture1;
            }
            return SetBlendMode | NoBlend;

        default: break;
        }

        // Draw nothing for the specified mode.
        return Skip;
    }

    /**
     * Restore GL state after drawing in the specified @a mode.
     */
    void popGLState(DrawMode mode)
    {
        switch(mode)
        {
        default: break;

        case DM_ALL:
            if(spec.unit(TU_INTER).hasTexture())
            {
                GL_SelectTexUnits(1);
                GL_ModulateTexture(1);
            }
            break;

        case DM_BLENDED:
            if(spec.unit(TU_INTER).hasTexture())
            {
                GL_SelectTexUnits(1);
                GL_ModulateTexture(1);
            }
            break;

        case DM_BLENDED_MOD_TEXTURE:
        case DM_MOD_TEXTURE:
        case DM_MOD_TEXTURE_MANY_LIGHTS:
            if(spec.unit(TU_INTER).hasTexture())
            {
                GL_SelectTexUnits(1);
                GL_ModulateTexture(1);
            }
            else if(mode != DM_BLENDED_MOD_TEXTURE)
            {
                GL_ModulateTexture(1);
            }
            break;

        case DM_UNBLENDED_MOD_TEXTURE_AND_DETAIL:
            if(!spec.unit(TU_INTER).hasTexture())
            {
                if(spec.unit(TU_PRIMARY_DETAIL).hasTexture())
                {
                    GL_SelectTexUnits(1);
                    GL_ModulateTexture(1);
                }
                else
                {
                    GL_ModulateTexture(1);
                }
            }
            break;

        case DM_UNBLENDED_TEXTURE_AND_DETAIL:
            if(!spec.unit(TU_INTER).hasTexture() &&
               spec.unit(TU_PRIMARY_DETAIL).hasTexture())
            {
                GL_SelectTexUnits(1);
                GL_ModulateTexture(1);
            }
            break;

        case DM_SHADOW:
            if(!spec.unit(TU_PRIMARY).hasTexture())
            {
                // Restore original modelview matrix.
                glMatrixMode(GL_MODELVIEW);
                glPopMatrix();
            }
            break;

        case DM_SHINY:
        case DM_ALL_SHINY:
        case DM_MASKED_SHINY:
            GL_BlendMode(BM_NORMAL);
            if(mode == DM_MASKED_SHINY && spec.unit(TU_INTER).hasTexture())
            {
                GL_SelectTexUnits(1);
            }
            break;
        }
    }
};

DrawList::DrawList(Spec const &spec) : d(new Instance(spec))
{}

DrawList::Spec &DrawList::spec()
{
    return d->spec;
}

DrawList::Spec const &DrawList::spec() const
{
    return d->spec;
}

void DrawList::draw(DrawMode mode, TexUnitMap const &texUnitMap) const
{
    DENG2_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Setup GL state for this list.
    DrawConditions conditions = d->pushGLState(mode);

    // Should we just skip all this?
    if(conditions & Skip) return; // Assume no state changes were made.

    bool bypass = false;
    if(d->spec.unit(TU_INTER).hasTexture())
    {
        // Is blending allowed?
        if(conditions.testFlag(NoBlend))
        {
            return;
        }

        // Should all blended primitives be included?
        if(conditions.testFlag(Blend))
        {
            // The other conditions will be bypassed.
            bypass = true;
        }
    }

    // Check conditions dependant on primitive-specific values once before
    // entering the loop. If none of the conditions are true for this list
    // then we can bypass the skip tests completely during iteration.
    if(!bypass)
    {
        if(!conditions.testFlag(JustOneLight) &&
           !conditions.testFlag(ManyLights))
        {
            bypass = true;
        }
    }

    bool skip = false;
    for(int i = 0; i < d->shards.size(); ++i)
    {
        Shard const &shard = d->shards[i];

        // Check for skip conditions.
        if(!bypass)
        {
            skip = false;
            if(shard.modTex && !shard.hasDynlights)
            {
                if(conditions.testFlag(ManyLights))
                    skip = true;
            }
            else if(shard.modTex || shard.hasDynlights)
            {
                if(conditions.testFlag(JustOneLight))
                    skip = true;
            }
        }

        if(!skip)
        {
            if(conditions & SetLightEnv)
            {
                // Use the correct texture and color for the light.
                glActiveTexture((conditions & SetLightEnv0)? GL_TEXTURE0 : GL_TEXTURE1);
                GL_BindTextureUnmanaged(!renderTextures? 0 : shard.modTex,
                                        gl::ClampToEdge, gl::ClampToEdge);

                float modColorV[4] = { shard.modColor.x, shard.modColor.y, shard.modColor.z, 0 };
                glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, modColorV);
            }

            if(conditions & SetBlendMode)
            {
                // Primitive-specific blending. Not used in all lists.
                GL_BlendMode(shard.blendmode);
            }

            foreach(Shard::Primitive const &prim, shard.primitives)
            {
                if(conditions & SetMatrixTexture)
                {
                    // Primitive-specific texture translation & scale.
                    if(conditions & SetMatrixTexture0)
                    {
                        glActiveTexture(GL_TEXTURE0);
                        glMatrixMode(GL_TEXTURE);
                        glPushMatrix();
                        glLoadIdentity();
                        glTranslatef(prim.texOffset.x * prim.texScale.x, prim.texOffset.y * prim.texScale.y, 1);
                        glScalef(prim.texScale.x, prim.texScale.y, 1);
                    }

                    if(conditions & SetMatrixTexture1)
                    {
                        glActiveTexture(GL_TEXTURE1);
                        glMatrixMode(GL_TEXTURE);
                        glPushMatrix();
                        glLoadIdentity();
                        glTranslatef(prim.texOffset.x * prim.texScale.x, prim.texOffset.y * prim.texScale.y, 1);
                        glScalef(prim.texScale.x, prim.texScale.y, 1);
                    }
                }

                if(conditions & SetMatrixDTexture)
                {
                    // Primitive-specific texture translation & scale.
                    if(conditions & SetMatrixDTexture0)
                    {
                        glActiveTexture(GL_TEXTURE0);
                        glMatrixMode(GL_TEXTURE);
                        glPushMatrix();
                        glLoadIdentity();
                        glTranslatef(prim.detailTexOffset.x * prim.detailTexScale.x, prim.detailTexOffset.y * prim.detailTexScale.y, 1);
                        glScalef(prim.detailTexScale.x, prim.detailTexScale.y, 1);
                    }

                    if(conditions & SetMatrixDTexture1)
                    {
                        glActiveTexture(GL_TEXTURE1);
                        glMatrixMode(GL_TEXTURE);
                        glPushMatrix();
                        glLoadIdentity();
                        glTranslatef(prim.detailTexOffset.x * prim.detailTexScale.x, prim.detailTexOffset.y * prim.detailTexScale.y, 1);
                        glScalef(prim.detailTexScale.x, prim.detailTexScale.y, 1);
                    }
                }

                glBegin(prim.type == gl::TriangleStrip? GL_TRIANGLE_STRIP : GL_TRIANGLE_FAN);
                for(WorldVBuf::Index k = 0; k < prim.vertCount; ++k)
                {
                    WorldVBuf::Type const &vertex = (*prim.vbuffer)[prim.indices[k]];

                    for(int m = 0; m < numTexUnits; ++m)
                    {
                        if(texUnitMap[m])
                        {
                            Vector2f const &tc = vertex.texCoord[texUnitMap[m] - 1];
                            glMultiTexCoord2f(GL_TEXTURE0 + m, tc.x, tc.y);
                        }
                    }

                    if(!(conditions & NoColor))
                    {
                        Vector4f color = vertex.rgba;

                        // We should not be relying on clamping at this late stage...
                        DENG2_ASSERT(INRANGE_OF(color.x, 0.f, 1.f));
                        DENG2_ASSERT(INRANGE_OF(color.y, 0.f, 1.f));
                        DENG2_ASSERT(INRANGE_OF(color.z, 0.f, 1.f));
                        DENG2_ASSERT(INRANGE_OF(color.w, 0.f, 1.f));

                        color = color.max(Vector4f(0, 0, 0, 0)).min(Vector4f(1, 1, 1, 1));

                        glColor4f(color.x, color.y, color.z, color.w);
                    }

                    glVertex3f(vertex.pos.x, vertex.pos.z, vertex.pos.y);
                }
                glEnd();

                // Restore the texture matrix if changed.
                if(conditions & SetMatrixDTexture)
                {
                    if(conditions & SetMatrixDTexture0)
                    {
                        glActiveTexture(GL_TEXTURE0);
                        glMatrixMode(GL_TEXTURE);
                        glPopMatrix();
                    }
                    if(conditions & SetMatrixDTexture1)
                    {
                        glActiveTexture(GL_TEXTURE1);
                        glMatrixMode(GL_TEXTURE);
                        glPopMatrix();
                    }
                }

                if(conditions & SetMatrixTexture)
                {
                    if(conditions & SetMatrixTexture0)
                    {
                        glActiveTexture(GL_TEXTURE0);
                        glMatrixMode(GL_TEXTURE);
                        glPopMatrix();
                    }
                    if(conditions & SetMatrixTexture1)
                    {
                        glActiveTexture(GL_TEXTURE1);
                        glMatrixMode(GL_TEXTURE);
                        glPopMatrix();
                    }
                }
            }

            DENG2_ASSERT(!Sys_GLCheckError());
        }
    }

    // Some modes require cleanup.
    d->popGLState(mode);
}

bool DrawList::isEmpty() const
{
    return d->shards.isEmpty();
}

void DrawList::append(Shard const &shard)
{
    if(!shard.primitives.isEmpty())
    {
        d->shards << shard; // Becomes the new last element.
    }
}

void DrawList::append(QList<Shard *> const &shards)
{
    foreach(Shard *shard, shards) append(*shard);
}

void DrawList::clear()
{
    d->shards.clear();
}

void DrawList::rewind()
{
    d->shards.rewind();
}
