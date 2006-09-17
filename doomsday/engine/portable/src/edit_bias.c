/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

/*
 * edit_bias.c: Bias light source editor.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_edit.h"
#include "de_system.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_ui.h"

#include <math.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void Con_ClearActions(void);
extern boolean B_SetBindClass(int classID, int type);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(BLEditor);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void SBE_MenuSave(ui_object_t *ob);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int gamedrawhud;
extern int numSources;
extern int freezeRLs;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

const char *saveFile = NULL;

static ui_page_t page_bias;

static ui_object_t ob_bias[] = {    // bias editor page
    {UI_BUTTON, 0, UIF_DEFAULT, 400, 450, 180, 70, "Save", UIButton_Drawer,
     UIButton_Responder, 0, SBE_MenuSave},

    {UI_NONE}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

/*
 * Editing variables:
 *
 * edit-bias-blink: keep blinking the nearest light (unless a light is grabbed)
 * edit-bias-grab-distance: how far the light is when grabbed
 * edit-bias-{red,green,blue,intensity}: RGBI of the light
 */
static int editBlink = false;
static float editDistance = 300;
static float editColor[3];
static float editIntensity;

/*
 * Editor status.
 */
static int editActive = false; // Edit mode active?
static int editGrabbed = -1;
static int editHidden = false;
static int editShowAll = false;
static int editShowIndices = true;
static int editHueCircle = false;
static float hueDistance = 100;
static vec3_t hueOrigin, hueSide, hueUp;


// CODE --------------------------------------------------------------------

/*
 * Register console variables for Shadow Bias.
 */
void SBE_Register(void)
{
    // Editing variables.
    C_VAR_INT("edit-bias-blink", &editBlink, 0, 0, 1);

    C_VAR_FLOAT("edit-bias-grab-distance", &editDistance, 0, 10, 1000);

    C_VAR_FLOAT("edit-bias-red", &editColor[0], 0, 0, 1);

    C_VAR_FLOAT("edit-bias-green", &editColor[1], 0, 0, 1);

    C_VAR_FLOAT("edit-bias-blue", &editColor[2], 0, 0, 1);

    C_VAR_FLOAT("edit-bias-intensity", &editIntensity, 0, 1, 50000);

    C_VAR_INT("edit-bias-hide", &editHidden, 0, 0, 1);

    C_VAR_INT("edit-bias-show-sources", &editShowAll, 0, 0, 1);

    C_VAR_INT("edit-bias-show-indices", &editShowIndices, 0, 0, 1);

    // Commands for light editing.
    C_CMD("bledit", BLEditor);
    C_CMD("blquit", BLEditor);
    C_CMD("blclear", BLEditor);
    C_CMD("blsave", BLEditor);
    C_CMD("blnew", BLEditor);
    C_CMD("bldel", BLEditor);
    C_CMD("bllock", BLEditor);
    C_CMD("blunlock", BLEditor);
    C_CMD("blgrab", BLEditor);
    C_CMD("bldup", BLEditor);
    C_CMD("blc", BLEditor);
    C_CMD("bli", BLEditor);
    C_CMD("blhue", BLEditor);
    C_CMD("blmenu", BLEditor);
}

/*
 * Editor Functionality:
 */

static void SBE_GetHand(float pos[3])
{
    pos[0] = vx + viewfrontvec[VX] * editDistance;
    pos[1] = vz + viewfrontvec[VZ] * editDistance;
    pos[2] = vy + viewfrontvec[VY] * editDistance;
}

static source_t *SBE_GrabSource(int index)
{
    source_t *s;
    int i;

    editGrabbed = index;
    s = SB_GetSource(index);

    // Update the property cvars.
    editIntensity = s->primaryIntensity;
    for(i = 0; i < 3; ++i)
        editColor[i] = s->color[i];

    return s;
}

static source_t *SBE_GetGrabbed(void)
{
    if(editGrabbed >= 0 && editGrabbed < numSources)
    {
        return SB_GetSource(editGrabbed);
    }
    return NULL;
}

static source_t *SBE_GetNearest(void)
{
    float hand[3];
    source_t *nearest = NULL, *s;
    float minDist = 0, len;
    int i;

    SBE_GetHand(hand);

    s = SB_GetSource(0);
    for(i = 0; i < numSources; ++i, s++)
    {
        len = M_Distance(hand, s->pos);
        if(i == 0 || len < minDist)
        {
            minDist = len;
            nearest = s;
        }
    }
    return nearest;
}

static void SBE_GetHueColor(float *color, float *angle, float *sat)
{
    int i;
    float dot;
    float saturation, hue, scale;
    float minAngle = 0.1f, range = 0.19f;
    vec3_t h, proj;

    dot = M_DotProduct(viewfrontvec, hueOrigin);
    saturation = (acos(dot) - minAngle) / range;

    if(saturation < 0)
        saturation = 0;
    if(saturation > 1)
        saturation = 1;
    if(sat)
        *sat = saturation;

    if(saturation == 0 || dot > .999f)
    {
        if(angle)
            *angle = 0;
        if(sat)
            *sat = 0;

        HSVtoRGB(color, 0, 0, 1);
        return;
    }

    // Calculate hue angle by projecting the current viewfront to the
    // hue circle plane.  Project onto the normal and subtract.
    scale = M_DotProduct(viewfrontvec, hueOrigin) /
        M_DotProduct(hueOrigin, hueOrigin);
    M_Scale(h, hueOrigin, scale);

    for(i = 0; i < 3; ++i)
        proj[i] = viewfrontvec[i] - h[i];

    // Now we have the projected view vector on the circle's plane.
    // Normalize the projected vector.
    M_Normalize(proj);

    hue = acos(M_DotProduct(proj, hueUp));

    if(M_DotProduct(proj, hueSide) > 0)
        hue = 2*PI - hue;

    hue /= (float) (2*PI);
    hue += 0.25;

    if(angle)
        *angle = hue;

    //Con_Printf("sat=%f, hue=%f\n", saturation, hue);

    HSVtoRGB(color, hue, saturation, 1);
}

void SBE_EndFrame(void)
{
    source_t *src;

    // Update the grabbed light.
    if(editActive && (src = SBE_GetGrabbed()) != NULL)
    {
        source_t old;

        memcpy(&old, src, sizeof(old));

        if(editHueCircle)
        {
            // Get the new color from the circle.
            SBE_GetHueColor(editColor, NULL, NULL);
        }

        SB_SetColor(src->color, editColor);
        src->primaryIntensity = src->intensity = editIntensity;
        if(!(src->flags & BLF_LOCKED))
        {
            // Update source properties.
            SBE_GetHand(src->pos);
        }

        if(memcmp(&old, src, sizeof(old)))
        {
            // The light must be re-evaluated.
            src->flags |= BLF_CHANGED;
        }
    }
}

static void SBE_Begin(void)
{
    // Advise the game not to draw any HUD displays
    gamedrawhud = false;
    editActive = true;
    editGrabbed = -1;
    // Enable the biaseditor binding class
    B_SetBindClass(DDBC_BIASEDITOR, true);
    Con_Printf("Bias light editor: ON\n");
}

static void SBE_End(void)
{
    // Advise the game it can safely draw any HUD displays again
    gamedrawhud = true;
    editActive = false;
    // Disable the biaseditor binding class
    B_SetBindClass(DDBC_BIASEDITOR, false);
    Con_Printf("Bias light editor: OFF\n");
}

static boolean SBE_New(void)
{
    source_t *s;

    if(numSources == MAX_BIAS_LIGHTS)
        return false;

    s = SBE_GrabSource(numSources);
    s->flags &= ~BLF_LOCKED;
    s->flags |= BLF_COLOR_OVERRIDE;
    editIntensity = 200;
    editColor[0] = editColor[1] = editColor[2] = 1;

    numSources++;

    return true;
}

static void SBE_Clear(void)
{
    SB_Clear();
    editGrabbed = -1;
    SBE_New();
}

static void SBE_Delete(int which)
{
    if(editGrabbed == which)
        editGrabbed = -1;
    else if(editGrabbed > which)
        editGrabbed--;

    SB_Delete(which);
}

static void SBE_Lock(int which)
{
    SB_GetSource(which)->flags |= BLF_LOCKED;
}

static void SBE_Unlock(int which)
{
    SB_GetSource(which)->flags &= ~BLF_LOCKED;
}

static void SBE_Grab(int which)
{
    if(editGrabbed != which)
        SBE_GrabSource(which);
    else
        editGrabbed = -1;
}

static void SBE_Dupe(int which)
{
    source_t *orig = SB_GetSource(which);
    source_t *s;
    int i;

    if(SBE_New())
    {
        s = SBE_GetGrabbed();
        s->flags &= ~BLF_LOCKED;
        s->sectorLevel[0] = orig->sectorLevel[0];
        s->sectorLevel[1] = orig->sectorLevel[1];
        editIntensity = orig->primaryIntensity;
        for(i = 0; i < 3; ++i)
            editColor[i] = orig->color[i];
    }
}

static boolean SBE_Save(const char *name)
{
    filename_t fileName;
    FILE *file;
    int i;
    source_t *s;
    const char *uid = R_GetUniqueLevelID();

    if(!name)
    {
        sprintf(fileName, "%s.ded", R_GetCurrentLevelID());
    }
    else
    {
        strcpy(fileName, name);
        if(!strchr(fileName, '.'))
        {
            // Append the file name extension.
            strcat(fileName, ".ded");
        }
    }

    Con_Printf("Saving to %s...\n", fileName);

    if((file = fopen(fileName, "wt")) == NULL)
        return false;

    fprintf(file, "# %i Bias Lights for %s\n\n", numSources, uid);

    // Since there can be quite a lot of these, make sure we'll skip
    // the ones that are definitely not suitable.
    fprintf(file, "SkipIf Not %s\n", (char *) gx.GetVariable(DD_GAME_MODE));

    s = SB_GetSource(0);
    for(i = 0; i < numSources; ++i, ++s)
    {
        fprintf(file, "\nLight {\n");
        fprintf(file, "  Map = \"%s\"\n", uid);
        fprintf(file, "  Origin { %g %g %g }\n",
                s->pos[0], s->pos[1], s->pos[2]);
        fprintf(file, "  Color { %g %g %g }\n",
                s->color[0], s->color[1], s->color[2]);
        fprintf(file, "  Intensity = %g\n", s->primaryIntensity);
        fprintf(file, "  Sector levels { %i %i }\n", s->sectorLevel[0],
                s->sectorLevel[1]);
        fprintf(file, "}\n");
    }

    fclose(file);
    return true;
}

void SBE_MenuSave(ui_object_t *ob)
{
    SBE_Save(saveFile);
}

void SBE_SetHueCircle(boolean activate)
{
    int i;

    if(activate == editHueCircle)
        return; // No change in state.

    if(activate && SBE_GetGrabbed() == NULL)
        return;

    editHueCircle = activate;

    if(activate)
    {
        // Determine the orientation of the hue circle.
        for(i = 0; i < 3; ++i)
        {
            hueOrigin[i] = viewfrontvec[i];
            hueSide[i] = viewsidevec[i];
            hueUp[i] = viewupvec[i];
        }
    }
}

/**
 * Returns true if the console player is currently using the HueCircle.
 */
boolean SBE_UsingHueCircle(void)
{
    return (editActive && editHueCircle);
}

/*
 * Editor commands.
 */
D_CMD(BLEditor)
{
    char *cmd = argv[0] + 2;
    int which;

    if(!stricmp(cmd, "edit"))
    {
        if(!editActive)
        {
            SBE_Begin();
            return true;
        }
        return false;
    }

    if(!editActive)
    {
        Con_Printf("The bias light editor is not active.\n");
        return false;
    }

    if(!stricmp(cmd, "quit"))
    {
        SBE_End();
        return true;
    }

    if(!stricmp(cmd, "save"))
    {
        return SBE_Save(argc >= 2 ? argv[1] : NULL);
    }

    if(!stricmp(cmd, "clear"))
    {
        SBE_Clear();
        return true;
    }

    if(!stricmp(cmd, "hue"))
    {
        int activate = (argc >= 2 ? stricmp(argv[1], "off") : !editHueCircle);
        SBE_SetHueCircle(activate);
        return true;
    }

    if(!stricmp(cmd, "new"))
    {
        return SBE_New();
    }

    if(!stricmp(cmd, "menu"))
    {
        Con_ClearActions(); // clear the actions array
        Con_Open(false); // close the console if open

        // show the bias menu interface
        UI_InitPage(&page_bias, ob_bias);
        sprintf(page_bias.title, "Doomsday BIAS Light Editor");
        page_bias.background = false;  // we don't want a background
        UI_Init(false, true, true, true, false, false);
        UI_SetPage(&page_bias);
        return true;
    }

    // Has the light index been given as an argument?
    if(editGrabbed >= 0)
        which = editGrabbed;
    else
        which = SB_ToIndex(SBE_GetNearest());

    if(!stricmp(cmd, "c") && numSources > 0)
    {
        source_t *src = SB_GetSource(which);
        float r = 1, g = 1, b = 1;

        if(argc >= 4)
        {
            r = strtod(argv[1], NULL);
            g = strtod(argv[2], NULL);
            b = strtod(argv[3], NULL);
        }

        editColor[0] = r;
        editColor[1] = g;
        editColor[2] = b;
        SB_SetColor(src->color, editColor);
        src->flags |= BLF_CHANGED;
        return true;
    }

    if(!stricmp(cmd, "i") && numSources > 0)
    {
        source_t *src = SB_GetSource(which);

        if(argc >= 3)
        {
            src->sectorLevel[0] = atoi(argv[1]);
            src->sectorLevel[1] = atoi(argv[2]);
        }
        else if(argc >= 2)
        {
            editIntensity = strtod(argv[1], NULL);
        }

        src->primaryIntensity = src->intensity = editIntensity;
        src->flags |= BLF_CHANGED;
        return true;
    }

    if(argc > 1)
    {
        which = atoi(argv[1]);
    }

    if(which < 0 || which >= numSources)
    {
        Con_Printf("Invalid light index %i.\n", which);
        return false;
    }

    if(!stricmp(cmd, "del"))
    {
        SBE_Delete(which);
        return true;
    }

    if(!stricmp(cmd, "dup"))
    {
        SBE_Dupe(which);
        return true;
    }

    if(!stricmp(cmd, "lock"))
    {
        SBE_Lock(which);
        return true;
    }

    if(!stricmp(cmd, "unlock"))
    {
        SBE_Unlock(which);
        return true;
    }

    if(!stricmp(cmd, "grab"))
    {
        SBE_Grab(which);
        return true;
    }

    return false;
}

static void SBE_DrawBox(int x, int y, int w, int h, ui_color_t *c)
{
    UI_GradientEx(x, y, w, h, 6,
                  c ? c : UI_COL(UIC_BG_MEDIUM),
                  c ? c : UI_COL(UIC_BG_LIGHT),
                  .2f, .4f);
    UI_DrawRectEx(x, y, w, h, 6, false, c ? c : UI_COL(UIC_BRD_HI),
                  NULL, .4f, -1);
}

static void SBE_InfoBox(source_t *s, int rightX, char *title, float alpha)
{
    float eye[3] = { vx, vz, vy };
    int w = 16 + FR_TextWidth("R:0.000 G:0.000 B:0.000");
    int th = FR_TextHeight("a"), h = th * 6 + 16;
    int x = glScreenWidth - 10 - w - rightX, y = glScreenHeight - 10 - h;
    char buf[80];
    ui_color_t color;

    color.red = s->color[0];
    color.green = s->color[1];
    color.blue = s->color[2];

    SBE_DrawBox(x, y, w, h, &color);
    x += 8;
    y += 8 + th/2;

    // - index #
    // - locked status
    // - coordinates
    // - distance from eye
    // - intensity
    // - color

    UI_TextOutEx(title, x, y, false, true, UI_COL(UIC_TITLE), alpha);
    y += th;

    sprintf(buf, "# %03i %s", SB_ToIndex(s),
            s->flags & BLF_LOCKED ? "(lock)" : "");
    UI_TextOutEx(buf, x, y, false, true, UI_COL(UIC_TEXT), alpha);
    y += th;

    sprintf(buf, "(%+06.0f,%+06.0f,%+06.0f)", s->pos[0], s->pos[1], s->pos[2]);
    UI_TextOutEx(buf, x, y, false, true, UI_COL(UIC_TEXT), alpha);
    y += th;

    sprintf(buf, "Distance:%-.0f", M_Distance(eye, s->pos));
    UI_TextOutEx(buf, x, y, false, true, UI_COL(UIC_TEXT), alpha);
    y += th;

    sprintf(buf, "Intens:%-5.0f L:%3i/%3i", s->primaryIntensity,
            s->sectorLevel[0], s->sectorLevel[1]);

    UI_TextOutEx(buf, x, y, false, true, UI_COL(UIC_TEXT), alpha);
    y += th;

    sprintf(buf, "R:%.3f G:%.3f B:%.3f",
            s->color[0], s->color[1], s->color[2]);
    UI_TextOutEx(buf, x, y, false, true, UI_COL(UIC_TEXT), alpha);
    y += th;

}


/*
 * Editor HUD
 */

static void SBE_DrawLevelGauge(void)
{
    static sector_t *lastSector = NULL;
    static int minLevel = 0, maxLevel = 0;
    sector_t *sector;
    int height = 255;
    int x = 20, y = glScreenHeight/2 - height/2;
    int off = FR_TextWidth("000");
    int secY, maxY = 0, minY = 0, p;
    char buf[80];
    source_t *src;

    if(SBE_GetGrabbed())
        src = SBE_GetGrabbed();
    else
        src = SBE_GetNearest();

    sector = R_PointInSubsector(FRACUNIT * src->pos[VX],
                                FRACUNIT * src->pos[VY])->sector;

    if(lastSector != sector)
    {
        minLevel = maxLevel = sector->lightlevel;
        lastSector = sector;
    }

    if(sector->lightlevel < minLevel)
        minLevel = sector->lightlevel;
    if(sector->lightlevel > maxLevel)
        maxLevel = sector->lightlevel;

    gl.Disable(DGL_TEXTURING);

    gl.Begin(DGL_LINES);
    gl.Color4f(1, 1, 1, .5f);
    gl.Vertex2f(x + off, y);
    gl.Vertex2f(x + off, y + height);
    // Normal lightlevel.
    secY = y + height * (1 - sector->lightlevel / 255.0f);
    gl.Vertex2f(x + off - 4, secY);
    gl.Vertex2f(x + off, secY);
    if(maxLevel != minLevel)
    {
        // Max lightlevel.
        maxY = y + height * (1 - maxLevel / 255.0f);
        gl.Vertex2f(x + off + 4, maxY);
        gl.Vertex2f(x + off, maxY);
        // Min lightlevel.
        minY = y + height * (1 - minLevel / 255.0f);
        gl.Vertex2f(x + off + 4, minY);
        gl.Vertex2f(x + off, minY);
    }
    // Current min/max bias sector level.
    if(src->sectorLevel[0] > 0 || src->sectorLevel[1] > 0)
    {
        gl.Color3f(1, 0, 0);
        p = y + height * (1 - src->sectorLevel[0] / 255.0f);
        gl.Vertex2f(x + off + 2, p);
        gl.Vertex2f(x + off - 2, p);

        gl.Color3f(0, 1, 0);
        p = y + height * (1 - src->sectorLevel[1] / 255.0f);
        gl.Vertex2f(x + off + 2, p);
        gl.Vertex2f(x + off - 2, p);
    }
    gl.End();

    gl.Enable(DGL_TEXTURING);

    // The number values.
    sprintf(buf, "%03i", sector->lightlevel);
    UI_TextOutEx(buf, x, secY, true, true, UI_COL(UIC_TITLE), .7f);
    if(maxLevel != minLevel)
    {
        sprintf(buf, "%03i", maxLevel);
        UI_TextOutEx(buf, x + 2*off, maxY, true, true, UI_COL(UIC_TEXT), .7f);
        sprintf(buf, "%03i", minLevel);
        UI_TextOutEx(buf, x + 2*off, minY, true, true, UI_COL(UIC_TEXT), .7f);
    }
}

void SBE_DrawHUD(void)
{
    int w, h, y;
    char buf[80];
    float alpha = .8f;
    source_t *s;

    if(!editActive || editHidden)
        return;

    // Go into screen projection mode.
    gl.MatrixMode(DGL_PROJECTION);
    gl.PushMatrix();
    gl.LoadIdentity();
    gl.Ortho(0, 0, glScreenWidth, glScreenHeight, -1, 1);

    // Overall stats: numSources / MAX (left)
    sprintf(buf, "%i / %i (%i free)", numSources, MAX_BIAS_LIGHTS,
            MAX_BIAS_LIGHTS - numSources);
    w = FR_TextWidth(buf) + 16;
    h = FR_TextHeight(buf) + 16;
    y = glScreenHeight - 10 - h;
    SBE_DrawBox(10, y, w, h, 0);
    UI_TextOutEx(buf, 18, y + h / 2, false, true,
                 UI_COL(UIC_TITLE), alpha);

    // The map ID.
    UI_TextOutEx((char*)R_GetUniqueLevelID(), 18, y - h/2, false, true,
                 UI_COL(UIC_TITLE), alpha);

    // Stats for nearest & grabbed:
    if(numSources)
    {
        s = SBE_GetNearest();
        SBE_InfoBox(s, 0, SBE_GetGrabbed() ? "Nearest" : "Highlighted", alpha);
    }

    if((s = SBE_GetGrabbed()) != NULL)
    {
        SBE_InfoBox(s, FR_TextWidth("0") * 26, "Grabbed", alpha);
    }

    if(SBE_GetGrabbed() || SBE_GetNearest())
    {
        SBE_DrawLevelGauge();
    }

    gl.MatrixMode(DGL_PROJECTION);
    gl.PopMatrix();
}

void SBE_DrawStar(float pos[3], float size, float color[4])
{
    float black[4] = { 0, 0, 0, 0 };

    gl.Begin(DGL_LINES);
    {
        gl.Color4fv(black);
        gl.Vertex3f(pos[VX] - size, pos[VZ], pos[VY]);
        gl.Color4fv(color);
        gl.Vertex3f(pos[VX], pos[VZ], pos[VY]);
        gl.Vertex3f(pos[VX], pos[VZ], pos[VY]);
        gl.Color4fv(black);
        gl.Vertex3f(pos[VX] + size, pos[VZ], pos[VY]);

        gl.Vertex3f(pos[VX], pos[VZ] - size, pos[VY]);
        gl.Color4fv(color);
        gl.Vertex3f(pos[VX], pos[VZ], pos[VY]);
        gl.Vertex3f(pos[VX], pos[VZ], pos[VY]);
        gl.Color4fv(black);
        gl.Vertex3f(pos[VX], pos[VZ] + size, pos[VY]);

        gl.Vertex3f(pos[VX], pos[VZ], pos[VY] - size);
        gl.Color4fv(color);
        gl.Vertex3f(pos[VX], pos[VZ], pos[VY]);
        gl.Vertex3f(pos[VX], pos[VZ], pos[VY]);
        gl.Color4fv(black);
        gl.Vertex3f(pos[VX], pos[VZ], pos[VY] + size);
    }
    gl.End();
}

static void SBE_DrawIndex(source_t *src)
{
    char buf[80];
    float eye[3] = { vx, vz, vy };
    float scale = M_Distance(src->pos, eye)/(glScreenWidth/2);

    if(!editShowIndices)
        return;

    gl.Disable(DGL_DEPTH_TEST);
    gl.Enable(DGL_TEXTURING);

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PushMatrix();
    gl.Translatef(src->pos[VX], src->pos[VZ], src->pos[VY]);
    gl.Rotatef(-vang + 180, 0, 1, 0);
    gl.Rotatef(vpitch, 1, 0, 0);
    gl.Scalef(-scale, -scale, 1);

    // Show the index number of the source.
    sprintf(buf, "%i", SB_ToIndex(src));
    UI_TextOutEx(buf, 2, 2, false, false, UI_COL(UIC_TITLE),
                 1 - M_Distance(src->pos, eye)/2000);

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PopMatrix();

    gl.Enable(DGL_DEPTH_TEST);
    gl.Disable(DGL_TEXTURING);
}

static void SBE_DrawSource(source_t *src)
{
    float col[4], d;
    float eye[3] = { vx, vz, vy };

    col[0] = src->color[0];
    col[1] = src->color[1];
    col[2] = src->color[2];
    d = (M_Distance(eye, src->pos) - 100) / 1000;
    if(d < 1) d = 1;
    col[3] = 1.0f / d;

    SBE_DrawStar(src->pos, 25 + src->intensity/20, col);
    SBE_DrawIndex(src);
}

static void SBE_HueOffset(double angle, float *offset)
{
    offset[0] = cos(angle) * hueSide[VX] + sin(angle) * hueUp[VX];
    offset[1] = sin(angle) * hueUp[VY];
    offset[2] = cos(angle) * hueSide[VZ] + sin(angle) * hueUp[VZ];
}

static void SBE_DrawHue(void)
{
    vec3_t eye = { vx, vy, vz };
    vec3_t center, off, off2;
    float steps = 32, inner = 10, outer = 30, s;
    double angle;
    float color[4], sel[4], hue, saturation;
    int i;

    gl.Disable(DGL_DEPTH_TEST);
    gl.Disable(DGL_TEXTURING);
    gl.Disable(DGL_CULL_FACE);

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PushMatrix();

    gl.Translatef(vx, vy, vz);
    gl.Scalef(1, 1.0f/1.2f, 1);
    gl.Translatef(-vx, -vy, -vz);

    // The origin of the circle.
    for(i = 0; i < 3; ++i)
        center[i] = eye[i] + hueOrigin[i] * hueDistance;

    // Draw the circle.
    gl.Begin(DGL_QUAD_STRIP);
    for(i = 0; i <= steps; ++i)
    {
        angle = 2*PI * i/steps;

        // Calculate the hue color for this angle.
        HSVtoRGB(color, i/steps, 1, 1);
        color[3] = .5f;

        SBE_HueOffset(angle, off);

        gl.Color4fv(color);
        gl.Vertex3f(center[0] + outer * off[0], center[1] + outer * off[1],
                    center[2] + outer * off[2]);

        // Saturation decreases in the center.
        color[0] = 1;
        color[1] = 1;
        color[2] = 1;
        color[3] = .15f;
        gl.Color4fv(color);
        gl.Vertex3f(center[0] + inner * off[0], center[1] + inner * off[1],
                    center[2] + inner * off[2]);
    }
    gl.End();

    gl.Begin(DGL_LINES);

    // Draw the current hue.
    SBE_GetHueColor(sel, &hue, &saturation);
    SBE_HueOffset(2*PI * hue, off);
    sel[3] = 1;
    if(saturation > 0)
    {
        gl.Color4fv(sel);
        gl.Vertex3f(center[0] + outer * off[0], center[1] + outer * off[1],
                    center[2] + outer * off[2]);
        gl.Vertex3f(center[0] + inner * off[0], center[1] + inner * off[1],
                    center[2] + inner * off[2]);
    }

    // Draw the edges.
    for(i = 0; i < steps; ++i)
    {
        SBE_HueOffset(2*PI * i/steps, off);
        SBE_HueOffset(2*PI * (i + 1)/steps, off2);

        // Calculate the hue color for this angle.
        HSVtoRGB(color, i/steps, 1, 1);
        color[3] = 1;

        gl.Color4fv(color);
        gl.Vertex3f(center[0] + outer * off[0], center[1] + outer * off[1],
                    center[2] + outer * off[2]);
        gl.Vertex3f(center[0] + outer * off2[0], center[1] + outer * off2[1],
                    center[2] + outer * off2[2]);

        if(saturation > 0)
        {
            // Saturation decreases in the center.
            sel[3] = 1 - fabs(M_CycleIntoRange(hue - i/steps + .5f, 1) - .5f)
                * 2.5f;
        }
        else
        {
            sel[3] = 1;
        }
        gl.Color4fv(sel);
        s = inner + (outer - inner) * saturation;
        gl.Vertex3f(center[0] + s * off[0], center[1] + s * off[1],
                    center[2] + s * off[2]);
        gl.Vertex3f(center[0] + s * off2[0], center[1] + s * off2[1],
                    center[2] + s * off2[2]);
    }
    gl.End();

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PopMatrix();

    gl.Enable(DGL_DEPTH_TEST);
    gl.Enable(DGL_TEXTURING);
    gl.Enable(DGL_CULL_FACE);
}

void SBE_DrawCursor(void)
{
#define SET_COL(x, r, g, b, a) {x[0]=(r); x[1]=(g); x[2]=(b); x[3]=(a);}

    double t = Sys_GetRealTime()/100.0f;
    source_t *s;
    float hand[3];
    float size = 10000, distance;
    float col[4];
    float eye[3] = { vx, vz, vy };

    if(!editActive || !numSources || editHidden || freezeRLs)
        return;

    if(editHueCircle && SBE_GetGrabbed())
        SBE_DrawHue();

    // The grabbed cursor blinks yellow.
    if(!editBlink || currentTimeSB & 0x80)
        SET_COL(col, 1.0f, 1.0f, .8f, .5f)
    else
        SET_COL(col, .7f, .7f, .5f, .4f)

    s = SBE_GetGrabbed();
    if(!s)
    {
        // The nearest cursor phases blue.
        SET_COL(col, sin(t)*.2f, .2f + sin(t)*.15f, .9f + sin(t)*.3f,
                   .8f - sin(t)*.2f)

        s = SBE_GetNearest();
    }

    gl.Disable(DGL_TEXTURING);

    SBE_GetHand(hand);
    if((distance = M_Distance(s->pos, hand)) > 2 * editDistance)
    {
        // Show where it is.
        gl.Disable(DGL_DEPTH_TEST);
    }

    SBE_DrawStar(s->pos, size, col);
    SBE_DrawIndex(s);

    // Show if the source is locked.
    if(s->flags & BLF_LOCKED)
    {
        float lock = 2 + M_Distance(eye, s->pos)/100;

        gl.Color4f(1, 1, 1, 1);

        gl.MatrixMode(DGL_MODELVIEW);
        gl.PushMatrix();

        gl.Translatef(s->pos[VX], s->pos[VZ], s->pos[VY]);

        gl.Rotatef(t/2, 0, 0, 1);
        gl.Rotatef(t, 1, 0, 0);
        gl.Rotatef(t * 15, 0, 1, 0);

        gl.Begin(DGL_LINES);
        gl.Vertex3f(-lock, 0, -lock);
        gl.Vertex3f(+lock, 0, -lock);

        gl.Vertex3f(+lock, 0, -lock);
        gl.Vertex3f(+lock, 0, +lock);

        gl.Vertex3f(+lock, 0, +lock);
        gl.Vertex3f(-lock, 0, +lock);

        gl.Vertex3f(-lock, 0, +lock);
        gl.Vertex3f(-lock, 0, -lock);
        gl.End();

        gl.PopMatrix();
    }

    if(SBE_GetNearest() != SBE_GetGrabbed() && SBE_GetGrabbed())
    {
        gl.Disable(DGL_DEPTH_TEST);
        SBE_DrawSource(SBE_GetNearest());
    }

    // Show all sources?
    if(editShowAll)
    {
        int i;
        source_t *src;

        gl.Disable(DGL_DEPTH_TEST);
        src = SB_GetSource(0);
        for(i = 0; i < numSources; ++i, src++)
        {
            if(s == src)
                continue;

            SBE_DrawSource(src);
        }
    }

    gl.Enable(DGL_TEXTURING);
    gl.Enable(DGL_DEPTH_TEST);
}
