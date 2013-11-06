/** @file rend_list.cpp Rendering Lists v3.3
 *
 * 3.3 -- Texture unit write state and revised primitive write interface.
 * 3.2 -- Shiny walls and floors
 * 3.1 -- Support for multiple shadow textures
 * 3.0 -- Multitexturing
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "render/rend_list.h"

#include "clientapp.h"
#include "de_graphics.h"
#include "de_render.h"

#include <de/concurrency.h>
#include <de/memory.h>

using namespace de;

/// Logical drawing modes.
enum DrawMode
{
    DM_SKYMASK,
    DM_ALL,
    DM_LIGHT_MOD_TEXTURE,
    DM_FIRST_LIGHT,
    DM_TEXTURE_PLUS_LIGHT,
    DM_UNBLENDED_TEXTURE_AND_DETAIL,
    DM_BLENDED,
    DM_BLENDED_FIRST_LIGHT,
    DM_NO_LIGHTS,
    DM_WITHOUT_TEXTURE,
    DM_LIGHTS,
    DM_MOD_TEXTURE,
    DM_MOD_TEXTURE_MANY_LIGHTS,
    DM_UNBLENDED_MOD_TEXTURE_AND_DETAIL,
    DM_BLENDED_MOD_TEXTURE,
    DM_ALL_DETAILS,
    DM_BLENDED_DETAILS,
    DM_SHADOW,
    DM_SHINY,
    DM_MASKED_SHINY,
    DM_ALL_SHINY
};

/**
 * Set per-list GL state.
 *
 * @return  The conditions to select primitives.
 */
static DrawConditions pushGLStateForList(DrawList const &list, DrawMode mode)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    switch(mode)
    {
    case DM_SKYMASK:
        // Render all primitives on the list without discrimination.
        return NoColor;

    case DM_ALL: // All surfaces.
        // Should we do blending?
        if(list.unit(TU_INTER).hasTexture())
        {
            // Blend between two textures, modulate with primary color.
            DENG2_ASSERT(numTexUnits >= 2);
            GL_SelectTexUnits(2);

            list.unit(TU_PRIMARY).bindTo(0);
            list.unit(TU_INTER).bindTo(1);
            GL_ModulateTexture(2);

            float color[4] = { 0, 0, 0, list.unit(TU_INTER).opacity };
            glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
        }
        else if(!list.unit(TU_PRIMARY).hasTexture())
        {
            // Opaque texture-less surface.
            return 0;
        }
        else
        {
            // Normal modulation.
            GL_SelectTexUnits(1);
            list.unit(TU_PRIMARY).bind();
            GL_ModulateTexture(1);
        }

        if(list.unit(TU_INTER).hasTexture())
        {
            return SetMatrixTexture0 | SetMatrixTexture1;
        }
        return SetMatrixTexture0;

    case DM_LIGHT_MOD_TEXTURE:
        // Modulate sector light, dynamic light and regular texture.
        list.unit(TU_PRIMARY).bindTo(1);
        return SetMatrixTexture1 | SetLightEnv0 | JustOneLight | NoBlend;

    case DM_TEXTURE_PLUS_LIGHT:
        list.unit(TU_PRIMARY).bindTo(0);
        return SetMatrixTexture0 | SetLightEnv1 | NoBlend;

    case DM_FIRST_LIGHT:
        // Draw all primitives with more than one light
        // and all primitives which will have a blended texture.
        return SetLightEnv0 | ManyLights | Blend;

    case DM_BLENDED: {
        // Only render the blended surfaces.
        if(!list.unit(TU_INTER).hasTexture())
        {
            return Skip;
        }

        DENG2_ASSERT(numTexUnits >= 2);
        GL_SelectTexUnits(2);

        list.unit(TU_PRIMARY).bindTo(0);
        list.unit(TU_INTER).bindTo(1);

        GL_ModulateTexture(2);

        float color[4] = { 0, 0, 0, list.unit(TU_INTER).opacity };
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
        return SetMatrixTexture0 | SetMatrixTexture1; }

    case DM_BLENDED_FIRST_LIGHT:
        // Only blended surfaces.
        if(!list.unit(TU_INTER).hasTexture())
        {
            return Skip;
        }
        return SetMatrixTexture1 | SetLightEnv0;

    case DM_WITHOUT_TEXTURE:
        // Only render geometries affected by dynlights.
        return 0;

    case DM_LIGHTS:
        // These lists only contain light geometries.
        list.unit(TU_PRIMARY).bind();
        return 0;

    case DM_BLENDED_MOD_TEXTURE:
        // Blending required.
        if(!list.unit(TU_INTER).hasTexture())
        {
            break;
        }

        // Intentional fall-through.

    case DM_MOD_TEXTURE:
    case DM_MOD_TEXTURE_MANY_LIGHTS:
        // Texture for surfaces with (many) dynamic lights.
        // Should we do blending?
        if(list.unit(TU_INTER).hasTexture())
        {
            // Mode 3 actually just disables the second texture stage,
            // which would modulate with primary color.
            DENG2_ASSERT(numTexUnits >= 2);
            GL_SelectTexUnits(2);

            list.unit(TU_PRIMARY).bindTo(0);
            list.unit(TU_INTER).bindTo(1);

            GL_ModulateTexture(3);

            float color[4] = { 0, 0, 0, list.unit(TU_INTER).opacity };
            glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
            // Render all geometry.
            return SetMatrixTexture0 | SetMatrixTexture1;
        }
        // No modulation at all.
        GL_SelectTexUnits(1);
        list.unit(TU_PRIMARY).bind();
        GL_ModulateTexture(0);
        if(mode == DM_MOD_TEXTURE_MANY_LIGHTS)
        {
            return SetMatrixTexture0 | ManyLights;
        }
        return SetMatrixTexture0;

    case DM_UNBLENDED_MOD_TEXTURE_AND_DETAIL:
        // Blending is not done now.
        if(list.unit(TU_INTER).hasTexture())
        {
            break;
        }

        if(list.unit(TU_PRIMARY_DETAIL).hasTexture())
        {
            GL_SelectTexUnits(2);
            GL_ModulateTexture(9); // Tex+Detail, no color.
            list.unit(TU_PRIMARY).bindTo(0);
            list.unit(TU_PRIMARY_DETAIL).bindTo(1);
            return SetMatrixTexture0 | SetMatrixDTexture1;
        }
        else
        {
            GL_SelectTexUnits(1);
            GL_ModulateTexture(0);
            list.unit(TU_PRIMARY).bind();
            return SetMatrixTexture0;
        }
        break;

    case DM_ALL_DETAILS:
        if(list.unit(TU_PRIMARY_DETAIL).hasTexture())
        {
            list.unit(TU_PRIMARY_DETAIL).bind();
            return SetMatrixDTexture0;
        }
        break;

    case DM_UNBLENDED_TEXTURE_AND_DETAIL:
        // Only unblended. Details are optional.
        if(list.unit(TU_INTER).hasTexture())
        {
            break;
        }

        if(list.unit(TU_PRIMARY_DETAIL).hasTexture())
        {
            GL_SelectTexUnits(2);
            GL_ModulateTexture(8);
            list.unit(TU_PRIMARY).bindTo(0);
            list.unit(TU_PRIMARY_DETAIL).bindTo(1);
            return SetMatrixTexture0 | SetMatrixDTexture1;
        }
        else
        {
            // Normal modulation.
            GL_SelectTexUnits(1);
            GL_ModulateTexture(1);
            list.unit(TU_PRIMARY).bind();
            return SetMatrixTexture0;
        }
        break;

    case DM_BLENDED_DETAILS: {
        // We'll only render blended primitives.
        if(!list.unit(TU_INTER).hasTexture())
        {
            break;
        }

        if(!list.unit(TU_PRIMARY_DETAIL).hasTexture() ||
           !list.unit(TU_INTER_DETAIL).hasTexture())
        {
            break;
        }

        list.unit(TU_PRIMARY_DETAIL).bindTo(0);
        list.unit(TU_INTER_DETAIL).bindTo(1);

        float color[4] = { 0, 0, 0, list.unit(TU_INTER_DETAIL).opacity };
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
        return SetMatrixDTexture0 | SetMatrixDTexture1; }

    case DM_SHADOW:
        if(list.unit(TU_PRIMARY).hasTexture())
        {
            list.unit(TU_PRIMARY).bind();
        }
        else
        {
            GL_BindTextureUnmanaged(0);
        }

        if(!list.unit(TU_PRIMARY).hasTexture())
        {
            // Apply a modelview shift.
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();

            // Scale towards the viewpoint to avoid Z-fighting.
            glTranslatef(vOrigin[VX], vOrigin[VY], vOrigin[VZ]);
            glScalef(.99f, .99f, .99f);
            glTranslatef(-vOrigin[VX], -vOrigin[VY], -vOrigin[VZ]);
        }
        return 0;

    case DM_MASKED_SHINY:
        if(list.unit(TU_INTER).hasTexture())
        {
            GL_SelectTexUnits(2);
            // The intertex holds the info for the mask texture.
            list.unit(TU_INTER).bindTo(1);
            float color[4] = { 0, 0, 0, 1 };
            glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
        }

        // Intentional fall-through.

    case DM_ALL_SHINY:
    case DM_SHINY:
        list.unit(TU_PRIMARY).bindTo(0);
        if(!list.unit(TU_INTER).hasTexture())
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

static void popGLStateForList(DrawMode mode, DrawList const &list)
{
    switch(mode)
    {
    default: break;

    case DM_SHADOW:
        if(!list.unit(TU_PRIMARY).hasTexture())
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
        break;
    }
}

/**
 * Setup GL state for an entire rendering pass (compassing multiple lists).
 */
static void pushGLStateForPass(DrawMode mode, TexUnitMap &texUnitMap)
{
    static float const black[4] = { 0, 0, 0, 0 };

    zap(texUnitMap);

    switch(mode)
    {
    case DM_SKYMASK:
        GL_SelectTexUnits(0);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        break;

    case DM_BLENDED:
        GL_SelectTexUnits(2);

        // Intentional fall-through.

    case DM_ALL:
        // The first texture unit is used for the main texture.
        texUnitMap[0] = Store::TCA_MAIN + 1;
        texUnitMap[1] = Store::TCA_BLEND + 1;
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Fog is allowed during this pass.
        if(usingFog)
        {
            glEnable(GL_FOG);
        }
        // All of the surfaces are opaque.
        glDisable(GL_BLEND);
        break;

    case DM_LIGHT_MOD_TEXTURE:
    case DM_TEXTURE_PLUS_LIGHT:
        // Modulate sector light, dynamic light and regular texture.
        GL_SelectTexUnits(2);
        if(mode == DM_LIGHT_MOD_TEXTURE)
        {
            texUnitMap[0] = Store::TCA_LIGHT + 1;
            texUnitMap[1] = Store::TCA_MAIN + 1;
            GL_ModulateTexture(4); // Light * texture.
        }
        else
        {
            texUnitMap[0] = Store::TCA_MAIN + 1;
            texUnitMap[1] = Store::TCA_LIGHT + 1;
            GL_ModulateTexture(5); // Texture + light.
        }
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Fog is allowed during this pass.
        if(usingFog)
        {
            glEnable(GL_FOG);
        }
        // All of the surfaces are opaque.
        glDisable(GL_BLEND);
        break;

    case DM_FIRST_LIGHT:
        // One light, no texture.
        GL_SelectTexUnits(1);
        texUnitMap[0] = Store::TCA_LIGHT + 1;
        GL_ModulateTexture(6);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        // All of the surfaces are opaque.
        glDisable(GL_BLEND);
        break;

    case DM_BLENDED_FIRST_LIGHT:
        // One additive light, no texture.
        GL_SelectTexUnits(1);
        texUnitMap[0] = Store::TCA_LIGHT + 1;
        GL_ModulateTexture(7); // Add light, no color.
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 1 / 255.0f);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        // All of the surfaces are opaque.
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        break;

    case DM_WITHOUT_TEXTURE:
        GL_SelectTexUnits(0);
        GL_ModulateTexture(1);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        // All of the surfaces are opaque.
        glDisable(GL_BLEND);
        break;

    case DM_LIGHTS:
        GL_SelectTexUnits(1);
        texUnitMap[0] = Store::TCA_MAIN + 1;
        GL_ModulateTexture(1);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 1 / 255.0f);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        if(usingFog)
        {
            glEnable(GL_FOG);
            glFogfv(GL_FOG_COLOR, black);
        }

        glEnable(GL_BLEND);
        GL_BlendMode(BM_ADD);
        break;

    case DM_MOD_TEXTURE:
    case DM_MOD_TEXTURE_MANY_LIGHTS:
    case DM_BLENDED_MOD_TEXTURE:
        // The first texture unit is used for the main texture.
        texUnitMap[0] = Store::TCA_MAIN + 1;
        texUnitMap[1] = Store::TCA_BLEND + 1;
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        // All of the surfaces are opaque.
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_ZERO);
        break;

    case DM_UNBLENDED_TEXTURE_AND_DETAIL:
        texUnitMap[0] = Store::TCA_MAIN + 1;
        texUnitMap[1] = Store::TCA_MAIN + 1;
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // All of the surfaces are opaque.
        glDisable(GL_BLEND);
        // Fog is allowed.
        if(usingFog)
        {
            glEnable(GL_FOG);
        }
        break;

    case DM_UNBLENDED_MOD_TEXTURE_AND_DETAIL:
        texUnitMap[0] = Store::TCA_MAIN + 1;
        texUnitMap[1] = Store::TCA_MAIN + 1;
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        // All of the surfaces are opaque.
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_ZERO);
        break;

    case DM_ALL_DETAILS:
        GL_SelectTexUnits(1);
        texUnitMap[0] = Store::TCA_MAIN + 1;
        GL_ModulateTexture(0);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        // All of the surfaces are opaque.
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
        // Use fog to fade the details, if fog is enabled.
        if(usingFog)
        {
            glEnable(GL_FOG);
            float const midGray[4] = { .5f, .5f, .5f, fogColor[3] }; // The alpha is probably meaningless?
            glFogfv(GL_FOG_COLOR, midGray);
        }
        break;

    case DM_BLENDED_DETAILS:
        GL_SelectTexUnits(2);
        texUnitMap[0] = Store::TCA_MAIN + 1;
        texUnitMap[1] = Store::TCA_BLEND + 1;
        GL_ModulateTexture(3);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        // All of the surfaces are opaque.
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
        // Use fog to fade the details, if fog is enabled.
        if(usingFog)
        {
            glEnable(GL_FOG);
            float const midGray[4] = { .5f, .5f, .5f, fogColor[3] }; // The alpha is probably meaningless?
            glFogfv(GL_FOG_COLOR, midGray);
        }
        break;

    case DM_SHADOW:
        // A bit like 'negative lights'.
        GL_SelectTexUnits(1);
        texUnitMap[0] = Store::TCA_MAIN + 1;
        GL_ModulateTexture(1);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 1 / 255.0f);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        // Set normal fog, if it's enabled.
        if(usingFog)
        {
            glEnable(GL_FOG);
            glFogfv(GL_FOG_COLOR, fogColor);
        }
        glEnable(GL_BLEND);
        GL_BlendMode(BM_NORMAL);
        break;

    case DM_SHINY:
        GL_SelectTexUnits(1);
        texUnitMap[0] = Store::TCA_MAIN + 1;
        GL_ModulateTexture(1); // 8 for multitexture
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        if(usingFog)
        {   // Fog makes the shininess diminish in the distance.
            glEnable(GL_FOG);
            glFogfv(GL_FOG_COLOR, black);
        }
        glEnable(GL_BLEND);
        GL_BlendMode(BM_ADD); // Purely additive.
        break;

    case DM_MASKED_SHINY:
        GL_SelectTexUnits(2);
        texUnitMap[0] = Store::TCA_MAIN + 1;
        texUnitMap[1] = Store::TCA_BLEND + 1; // the mask
        GL_ModulateTexture(8); // same as with details
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        if(usingFog)
        {
            // Fog makes the shininess diminish in the distance.
            glEnable(GL_FOG);
            glFogfv(GL_FOG_COLOR, black);
        }
        glEnable(GL_BLEND);
        GL_BlendMode(BM_ADD); // Purely additive.
        break;

    default: break;
    }
}

static void popGLStateForPass(DrawMode mode)
{
    switch(mode)
    {
    default: break;

    case DM_ALL:
    case DM_SHADOW:
    case DM_BLENDED:
    case DM_LIGHT_MOD_TEXTURE:
    case DM_TEXTURE_PLUS_LIGHT:
    case DM_LIGHTS:
    case DM_UNBLENDED_TEXTURE_AND_DETAIL:
    case DM_ALL_DETAILS:
    case DM_BLENDED_DETAILS:
    case DM_SHINY:
    case DM_MASKED_SHINY:
    case DM_ALL_SHINY:
        if(usingFog)
        {
            glDisable(GL_FOG);
        }
        break;
    }
}

static void renderLists(DrawLists::FoundLists const &lists, DrawMode mode)
{
    if(lists.isEmpty()) return;
    // If the first list is empty -- do nothing.
    if(lists.at(0)->isEmpty()) return;

    // Setup GL state that's common to all the lists in this mode.
    TexUnitMap texUnitMap;
    pushGLStateForPass(mode, texUnitMap);

    // Draw each given list.
    for(int i = 0; i < lists.count(); ++i)
    {
        DrawList const &list = *lists.at(i);

        // Setup GL state for this list.
        DrawConditions conditions = pushGLStateForList(list, mode);

        // Draw all identified geometry.
        list.draw(conditions, texUnitMap);

        // Some modes require cleanup.
        popGLStateForList(mode, list);
    }

    popGLStateForPass(mode);
}

static void drawSky()
{
    DrawLists::FoundLists lists;
    ClientApp::renderSystem().drawLists().findAll(SkyMaskGeom, lists);
    if(!devRendSkyAlways && lists.isEmpty())
    {
        return;
    }

    // We do not want to update color and/or depth.
    glDisable(GL_DEPTH_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    // Mask out stencil buffer, setting the drawn areas to 1.
    glEnable(GL_STENCIL_TEST);
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
    glStencilFunc(GL_ALWAYS, 1, 0xffffffff);

    if(!devRendSkyAlways)
    {
        renderLists(lists, DM_SKYMASK);
    }
    else
    {
        glClearStencil(1);
        glClear(GL_STENCIL_BUFFER_BIT);
    }

    // Re-enable update of color and depth.
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);

    // Now, only render where the stencil is set to 1.
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, 1, 0xffffffff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    Sky_Render();

    if(!devRendSkyAlways)
    {
        glClearStencil(0);
    }

    // Return GL state to normal.
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_DEPTH_TEST);
}

/*
 * We have several different paths to accommodate both multitextured details and
 * dynamic lights. Details take precedence (they always cover entire primitives
 * and usually *all* of the surfaces in a scene).
 */
void RL_RenderAllLists()
{
    DENG_ASSERT(!Sys_GLCheckError());
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    drawSky();

    // Render the real surfaces of the visible world.

    /*
     * Pass: Unlit geometries (all normal lists).
     */

    DrawLists::FoundLists lists;
    ClientApp::renderSystem().drawLists().findAll(UnlitGeom, lists);
    if(IS_MTEX_DETAILS)
    {
        // Draw details for unblended surfaces in this pass.
        renderLists(lists, DM_UNBLENDED_TEXTURE_AND_DETAIL);

        // Blended surfaces.
        renderLists(lists, DM_BLENDED);
    }
    else
    {
        // Blending is done during this pass.
        renderLists(lists, DM_ALL);
    }

    /*
     * Pass: Lit geometries.
     */

    ClientApp::renderSystem().drawLists().findAll(LitGeom, lists);

    // If multitexturing is available, we'll use it to our advantage when
    // rendering lights.
    if(IS_MTEX_LIGHTS && dynlightBlend != 2)
    {
        if(IS_MUL)
        {
            // All (unblended) surfaces with exactly one light can be
            // rendered in a single pass.
            renderLists(lists, DM_LIGHT_MOD_TEXTURE);

            // Render surfaces with many lights without a texture, just
            // with the first light.
            renderLists(lists, DM_FIRST_LIGHT);
        }
        else // Additive ('foggy') lights.
        {
            renderLists(lists, DM_TEXTURE_PLUS_LIGHT);

            // Render surfaces with blending.
            renderLists(lists, DM_BLENDED);

            // Render the first light for surfaces with blending.
            // (Not optimal but shouldn't matter; texture is changed for
            // each primitive.)
            renderLists(lists, DM_BLENDED_FIRST_LIGHT);
        }
    }
    else // Multitexturing is not available for lights.
    {
        if(IS_MUL)
        {
            // Render all lit surfaces without a texture.
            renderLists(lists, DM_WITHOUT_TEXTURE);
        }
        else
        {
            if(IS_MTEX_DETAILS) // Draw detail textures using multitexturing.
            {
                // Unblended surfaces with a detail.
                renderLists(lists, DM_UNBLENDED_TEXTURE_AND_DETAIL);

                // Blended surfaces without details.
                renderLists(lists, DM_BLENDED);

                // Details for blended surfaces.
                renderLists(lists, DM_BLENDED_DETAILS);
            }
            else
            {
                renderLists(lists, DM_ALL);
            }
        }
    }

    /*
     * Pass: All light geometries (always additive).
     */
    if(dynlightBlend != 2)
    {
        ClientApp::renderSystem().drawLists().findAll(LightGeom, lists);
        renderLists(lists, DM_LIGHTS);
    }

    /*
     * Pass: Geometries with texture modulation.
     */
    if(IS_MUL)
    {
        // Finish the lit surfaces that didn't yet get a texture.
        ClientApp::renderSystem().drawLists().findAll(LitGeom, lists);
        if(IS_MTEX_DETAILS)
        {
            renderLists(lists, DM_UNBLENDED_MOD_TEXTURE_AND_DETAIL);
            renderLists(lists, DM_BLENDED_MOD_TEXTURE);
            renderLists(lists, DM_BLENDED_DETAILS);
        }
        else
        {
            if(IS_MTEX_LIGHTS && dynlightBlend != 2)
            {
                renderLists(lists, DM_MOD_TEXTURE_MANY_LIGHTS);
            }
            else
            {
                renderLists(lists, DM_MOD_TEXTURE);
            }
        }
    }

    /*
     * Pass: Geometries with details & modulation.
     *
     * If multitexturing is not available for details, we need to apply them as
     * an extra pass over all the detailed surfaces.
     */
    if(r_detail)
    {
        // Render detail textures for all surfaces that need them.
        ClientApp::renderSystem().drawLists().findAll(UnlitGeom, lists);
        if(IS_MTEX_DETAILS)
        {
            // Blended detail textures.
            renderLists(lists, DM_BLENDED_DETAILS);
        }
        else
        {
            renderLists(lists, DM_ALL_DETAILS);

            ClientApp::renderSystem().drawLists().findAll(LitGeom, lists);
            renderLists(lists, DM_ALL_DETAILS);
        }
    }

    /*
     * Pass: Shiny geometries.
     *
     * If we have two texture units, the shiny masks will be enabled. Otherwise
     * the masks are ignored. The shine is basically specular environmental
     * additive light, multiplied by the mask so that black texels from the mask
     * produce areas without shine.
     */

    ClientApp::renderSystem().drawLists().findAll(ShineGeom, lists);
    if(numTexUnits > 1)
    {
        // Render masked shiny surfaces in a separate pass.
        renderLists(lists, DM_SHINY);
        renderLists(lists, DM_MASKED_SHINY);
    }
    else
    {
        renderLists(lists, DM_ALL_SHINY);
    }

    /*
     * Pass: Shadow geometries (objects and Fake Radio).
     */
    int const oldRenderTextures = renderTextures;

    renderTextures = true;

    ClientApp::renderSystem().drawLists().findAll(ShadowGeom, lists);
    renderLists(lists, DM_SHADOW);

    renderTextures = oldRenderTextures;

    // Return to the normal GL state.
    GL_SelectTexUnits(1);
    GL_ModulateTexture(1);
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0);
    glEnable(GL_BLEND);
    GL_BlendMode(BM_NORMAL);
    if(usingFog)
    {
        glEnable(GL_FOG);
        glFogfv(GL_FOG_COLOR, fogColor);
    }

    // Draw masked walls, sprites and models.
    Rend_DrawMasked();

    // Draw particles.
    Rend_RenderParticles();

    if(usingFog)
    {
        glDisable(GL_FOG);
    }

    DENG2_ASSERT(!Sys_GLCheckError());
}
