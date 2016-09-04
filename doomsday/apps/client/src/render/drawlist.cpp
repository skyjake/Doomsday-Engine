/** @file drawlist.cpp  Drawable primitive list.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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
#include "render/drawlist.h"

#include <de/concurrency.h>
#include <de/memoryzone.h>
#include <de/GLInfo>
#include "clientapp.h"
#include "gl/gl_main.h"
#include "render/rend_main.h"
#include "render/store.h"

using namespace de;

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

DENG2_PIMPL(DrawList)
{
    /**
     * Each Element begins a block of GL commands/geometry to apply/transfer.
     */
    struct Element {
        // Must be an offset since the list is sometimes reallocated.
        duint size;  ///< Size of this element (zero = n/a).

        Element *next() {
            if(!size) return nullptr;
            auto *elem = (Element *) ((duint8 *) (this) + size);
            if(!elem->size) return nullptr;
            return elem;
        }

        struct Data {
            Store const *buffer;
            gl::Primitive type;

            // Element indices into the global backing store for the geometry.
            // These are always contiguous and all are used (some are shared):
            //   indices[0] is the base, and indices[1...n] > indices[0].
            duint numIndices;
            duint *indices;

            bool oneLight;
            bool manyLights;
            blendmode_t blendMode;
            DGLuint  modTexture;
            Vector3f modColor;

            Vector2f texOffset;
            Vector2f texScale;

            Vector2f dtexOffset;
            Vector2f dtexScale;

            /**
             * Draw the geometry for this element.
             */
            void draw(DrawConditions const &conditions, TexUnitMap const &texUnitMap)
            {
                DENG2_ASSERT(buffer);

                if(conditions & SetLightEnv)
                {
                    // Use the correct texture and color for the light.
                    LIBGUI_GL.glActiveTexture((conditions & SetLightEnv0)? GL_TEXTURE0 : GL_TEXTURE1);
                    GL_BindTextureUnmanaged(!renderTextures? 0 : modTexture,
                                            gl::ClampToEdge, gl::ClampToEdge);

                    dfloat modColorV[4] = { modColor.x, modColor.y, modColor.z, 0 };
                    LIBGUI_GL.glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, modColorV);
                }

                if(conditions & SetMatrixTexture)
                {
                    // Primitive-specific texture translation & scale.
                    if(conditions & SetMatrixTexture0)
                    {
                        LIBGUI_GL.glActiveTexture(GL_TEXTURE0);
                        LIBGUI_GL.glMatrixMode(GL_TEXTURE);
                        LIBGUI_GL.glPushMatrix();
                        LIBGUI_GL.glLoadIdentity();
                        LIBGUI_GL.glTranslatef(texOffset.x * texScale.x, texOffset.y * texScale.y, 1);
                        LIBGUI_GL.glScalef(texScale.x, texScale.y, 1);
                    }

                    if(conditions & SetMatrixTexture1)
                    {
                        LIBGUI_GL.glActiveTexture(GL_TEXTURE1);
                        LIBGUI_GL.glMatrixMode(GL_TEXTURE);
                        LIBGUI_GL.glPushMatrix();
                        LIBGUI_GL.glLoadIdentity();
                        LIBGUI_GL.glTranslatef(texOffset.x * texScale.x, texOffset.y * texScale.y, 1);
                        LIBGUI_GL.glScalef(texScale.x, texScale.y, 1);
                    }
                }

                if(conditions & SetMatrixDTexture)
                {
                    // Primitive-specific texture translation & scale.
                    if(conditions & SetMatrixDTexture0)
                    {
                        LIBGUI_GL.glActiveTexture(GL_TEXTURE0);
                        LIBGUI_GL.glMatrixMode(GL_TEXTURE);
                        LIBGUI_GL.glPushMatrix();
                        LIBGUI_GL.glLoadIdentity();
                        LIBGUI_GL.glTranslatef(dtexOffset.x * dtexScale.x, dtexOffset.y * dtexScale.y, 1);
                        LIBGUI_GL.glScalef(dtexScale.x, dtexScale.y, 1);
                    }

                    if(conditions & SetMatrixDTexture1)
                    {
                        LIBGUI_GL.glActiveTexture(GL_TEXTURE1);
                        LIBGUI_GL.glMatrixMode(GL_TEXTURE);
                        LIBGUI_GL.glPushMatrix();
                        LIBGUI_GL.glLoadIdentity();
                        LIBGUI_GL.glTranslatef(dtexOffset.x * dtexScale.x, dtexOffset.y * dtexScale.y, 1);
                        LIBGUI_GL.glScalef(dtexScale.x, dtexScale.y, 1);
                    }
                }

                if(conditions & SetBlendMode)
                {
                    // Primitive-specific blending. Not used in all lists.
                    GL_BlendMode(blendMode);
                }

                LIBGUI_GL.glBegin(type == gl::TriangleStrip? GL_TRIANGLE_STRIP : GL_TRIANGLE_FAN);
                for(duint i = 0; i < numIndices; ++i)
                {
                    duint const index = indices[i];

                    for(dint k = 0; k < numTexUnits; ++k)
                    {
                        Vector2f const *tc = nullptr;  // No mapping.
                        switch(texUnitMap[k])
                        {
                        case AttributeSpec::TexCoord0:   tc = buffer->texCoords[0]; break;
                        case AttributeSpec::TexCoord1:   tc = buffer->texCoords[1]; break;
                        case AttributeSpec::ModTexCoord: tc = buffer->modCoords;    break;

                        default: break;
                        }

                        if(tc)
                        {
                            LIBGUI_GL.glMultiTexCoord2f(GL_TEXTURE0 + k, tc[index].x, tc[index].y);
                        }
                    }

                    if(!(conditions & NoColor))
                    {
                        Vector4ub const &color = buffer->colorCoords[index];
                        LIBGUI_GL.glColor4ub(color.x, color.y, color.z, color.w);
                    }

                    Vector3f const &pos = buffer->posCoords[index];
                    LIBGUI_GL.glVertex3f(pos.x, pos.z, pos.y);
                }
                LIBGUI_GL.glEnd();

                // Restore the texture matrix if changed.
                if(conditions & SetMatrixDTexture)
                {
                    if(conditions & SetMatrixDTexture0)
                    {
                        LIBGUI_GL.glActiveTexture(GL_TEXTURE0);
                        LIBGUI_GL.glMatrixMode(GL_TEXTURE);
                        LIBGUI_GL.glPopMatrix();
                    }
                    if(conditions & SetMatrixDTexture1)
                    {
                        LIBGUI_GL.glActiveTexture(GL_TEXTURE1);
                        LIBGUI_GL.glMatrixMode(GL_TEXTURE);
                        LIBGUI_GL.glPopMatrix();
                    }
                }

                if(conditions & SetMatrixTexture)
                {
                    if(conditions & SetMatrixTexture0)
                    {
                        LIBGUI_GL.glActiveTexture(GL_TEXTURE0);
                        LIBGUI_GL.glMatrixMode(GL_TEXTURE);
                        LIBGUI_GL.glPopMatrix();
                    }
                    if(conditions & SetMatrixTexture1)
                    {
                        LIBGUI_GL.glActiveTexture(GL_TEXTURE1);
                        LIBGUI_GL.glMatrixMode(GL_TEXTURE);
                        LIBGUI_GL.glPopMatrix();
                    }
                }
            }
        } data;
    };

    Spec spec;                 ///< List specification.
    dsize dataSize = 0;        ///< Number of bytes allocated for the data.
    duint8 *data   = nullptr;  ///< Data for a number of polygons (The List).
    duint8 *cursor = nullptr;  ///< Data pointer for reading/writing.
    Element *last  = nullptr;  ///< Last element (if any).

    Impl(Public *i, Spec const &spec) : Base(i), spec(spec) {}
    ~Impl() { clearAllData(); }

    void clearAllData()
    {
        if(data)
        {
            // All the list data will be destroyed.
            Z_Free(data); data = nullptr;
#ifdef DENG_DEBUG
            Z_CheckHeap();
#endif
        }

        cursor   = nullptr;
        last     = nullptr;
        dataSize = 0;
    }

    /**
     * @return  Start of the allocated data.
     */
    void *allocateData(dsize bytes)
    {
        // Number of extra bytes to keep allocated in the end of each list.
        dint const PADDING = 16;

        if(!bytes) return nullptr;

        // We require the extra bytes because we want that the end of the list data is
        // always safe for writing-in-advance. This is needed when the 'end of data'
        // marker is written.
        dint const startOffset = cursor - data;
        dsize const required   = startOffset + bytes + PADDING;

        // First check that the data buffer of the list is large enough.
        if(required > dataSize)
        {
            // Offsets must be preserved.
            duint8 const *oldData   = data;
            dint const cursorOffset = (cursor? cursor - oldData : -1);
            dint const lastOffset   = (last? (duint8 *) last - oldData : -1);

            // Allocate more memory for the data buffer.
            if(dataSize == 0)
            {
                dataSize = 1024;
            }
            while(dataSize < required)
            {
                dataSize *= 2;
            }
            data = (duint8 *) Z_Realloc(data, dataSize, PU_APPSTATIC);

            // Restore main pointers.
            cursor = (cursorOffset >= 0? data + cursorOffset : data);
            last   = (lastOffset >= 0? (Element *) (data + lastOffset) : 0);

            // Restore in-list pointers.
            // When the list is resized, pointers in the primitives need to be restored
            // so that they point to the new list data.
            if(oldData)
            {
                for(Element *elem = first(); elem && elem <= last; elem = elem->next())
                {
                    if(elem->data.indices)
                    {
                        elem->data.indices = (duint *) (data + ((duint8 *) elem->data.indices - oldData));
                    }
                }
            }
        }

        // Advance the cursor.
        cursor += bytes;

        return data + startOffset;
    }

    /// Returns a pointer to the first element in the list; otherwise @c nullptr.
    Element *first() const
    {
        auto *elem = (Element *)data;
        if(!elem->size) return nullptr;
        return elem;
    }

    /**
     * Configure GL state for drawing in this @a mode.
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

        case DM_ALL:  // All surfaces.
            DENG2_ASSERT(spec.group == UnlitGeom || spec.group == LitGeom);

            // Should we do blending?
            if(spec.unit(TU_INTER).hasTexture())
            {
                // Blend between two textures, modulate with primary color.
                DENG2_ASSERT(numTexUnits >= 2);
                GL_SelectTexUnits(2);

                GL_BindTo(spec.unit(TU_PRIMARY), 0);
                GL_BindTo(spec.unit(TU_INTER  ), 1);
                GL_ModulateTexture(2);

                dfloat color[4] = { 0, 0, 0, spec.unit(TU_INTER).opacity };
                LIBGUI_GL.glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
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
            GL_BindTo(spec.unit(TU_INTER  ), 1);
            GL_ModulateTexture(2);

            dfloat color[4] = { 0, 0, 0, spec.unit(TU_INTER).opacity };
            LIBGUI_GL.glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
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
                GL_BindTo(spec.unit(TU_INTER  ), 1);
                GL_ModulateTexture(3);

                dfloat color[4] = { 0, 0, 0, spec.unit(TU_INTER).opacity };
                LIBGUI_GL.glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
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
                GL_BindTo(spec.unit(TU_PRIMARY       ), 0);
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
                GL_BindTo(spec.unit(TU_PRIMARY       ), 0);
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
               !spec.unit(TU_INTER_DETAIL  ).hasTexture())
            {
                break;
            }

            GL_BindTo(spec.unit(TU_PRIMARY_DETAIL), 0);
            GL_BindTo(spec.unit(TU_INTER_DETAIL  ), 1);

            dfloat color[4] = { 0, 0, 0, spec.unit(TU_INTER_DETAIL).opacity };
            LIBGUI_GL.glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
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
                LIBGUI_GL.glMatrixMode(GL_MODELVIEW);
                LIBGUI_GL.glPushMatrix();

                // Scale towards the viewpoint to avoid Z-fighting.
                LIBGUI_GL.glTranslatef(vOrigin.x, vOrigin.y, vOrigin.z);
                LIBGUI_GL.glScalef(.99f, .99f, .99f);
                LIBGUI_GL.glTranslatef(-vOrigin.x, -vOrigin.y, -vOrigin.z);
            }
            return 0;

        case DM_MASKED_SHINY:
            DENG2_ASSERT(spec.group == ShineGeom);

            if(spec.unit(TU_INTER).hasTexture())
            {
                GL_SelectTexUnits(2);
                // The intertex holds the info for the mask texture.
                GL_BindTo(spec.unit(TU_INTER), 1);
                dfloat color[4] = { 0, 0, 0, 1 };
                LIBGUI_GL.glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
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
                LIBGUI_GL.glMatrixMode(GL_MODELVIEW);
                LIBGUI_GL.glPopMatrix();
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

DrawList::DrawList(Spec const &spec) : d(new Impl(this, spec))
{}

bool DrawList::isEmpty() const
{
    return d->last == nullptr;
}

DrawList &DrawList::write(Store const &buffer, gl::Primitive primitive, DrawList::Indices const &indices,
    blendmode_t blendMode, bool oneLight, bool manyLights,
    Vector2f const &texScale, Vector2f const &texOffset,
    Vector2f const &detailTexScale, Vector2f const &detailTexOffset,
    GLuint modTexture, Vector3f const &modColor)
{
    // Sanity check usage.
    DENG2_ASSERT(!(spec().group == LightGeom  && (oneLight || manyLights || modTexture)));
    DENG2_ASSERT(!(spec().group == LitGeom    && !Rend_IsMTexLights() && (oneLight || modTexture)));
    DENG2_ASSERT(!(spec().group == ShadowGeom && (oneLight || manyLights || modTexture)));

    if(indices.isEmpty()) return *this;  // Huh?

    // This becomes the new last element.
    d->last = (Impl::Element *) d->allocateData(sizeof(Impl::Element));
    d->last->size = 0;

    // Vertex buffer element indices for the primitive are stored in the list.
    /// @note That 'last' may be reallocated during allocateData() - use a temporary variable.
    d->last->data.buffer     = &buffer;
    d->last->data.type       = primitive;
    d->last->data.numIndices = indices.count();
    auto *lti = (duint *) d->allocateData(sizeof(duint) * d->last->data.numIndices);
    d->last->data.indices = lti;
    for(duint i = 0; i < d->last->data.numIndices; ++i)
    {
        d->last->data.indices[i] = indices[i];
    }
    // The element has been written, update the size in the header.
    d->last->size = d->cursor - (duint8 *) d->last;

    // Write the end marker (will be overwritten by the next write). The idea is that this
    // zero is interpreted as the size of the following element.
    *(duint *) d->cursor = 0;

    // Configure the GL state to be applied when this primitive is drawn later.
    Impl::Element &elem = *d->last;
    elem.data.blendMode  = blendMode;
    elem.data.modTexture = modTexture;
    elem.data.modColor   = modColor;
    elem.data.texScale   = texScale;
    elem.data.texOffset  = texOffset;
    elem.data.dtexScale  = detailTexScale;
    elem.data.dtexOffset = detailTexOffset;
    elem.data.oneLight   = oneLight;
    elem.data.manyLights = manyLights;

    return *this;
}

void DrawList::draw(DrawMode mode, TexUnitMap const &texUnitMap) const
{
    DENG_ASSERT_IN_MAIN_THREAD();
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
    for(Impl::Element *elem = d->first(); elem; elem = elem->next())
    {
        // Check for skip conditions.
        if(!bypass)
        {
            skip = false;
            if(conditions.testFlag(JustOneLight) && elem->data.manyLights)
            {
                skip = true;
            }
            else if(conditions.testFlag(ManyLights) && elem->data.oneLight)
            {
                skip = true;
            }
        }

        if(!skip)
        {
            elem->data.draw(conditions, texUnitMap);

            DENG2_ASSERT(!Sys_GLCheckError());
        }
    }

    // Some modes require cleanup.
    d->popGLState(mode);
}

DrawList::Spec &DrawList::spec()
{
    return d->spec;
}

DrawList::Spec const &DrawList::spec() const
{
    return d->spec;
}

void DrawList::clear()
{
    d->clearAllData();
}

void DrawList::rewind()
{
    d->cursor = d->data;
    d->last   = nullptr;
}
