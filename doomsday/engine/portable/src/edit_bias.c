/**\file edit_bias.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

/**
 * Bias light source editor.
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
#include "de_console.h"
#include "de_play.h"

#include <math.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(BLEditor);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void SBE_MenuSave(ui_object_t *ob);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int gameDrawHUD;
extern int numSources;
extern byte freezeRLs;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

const char *saveFile = NULL;

static ui_page_t page_bias;

static ui_object_t ob_bias[] = {    // bias editor page
    {UI_BUTTON, 0, UIF_DEFAULT, 400, 450, 180, 70, "Save", UIButton_Drawer,
     UIButton_Responder, 0, SBE_MenuSave},

    {UI_NONE}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

/**
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

/**
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

/**
 * Register console variables for Shadow Bias.
 */
void SBE_Register(void)
{
    // Editing variables.
    C_VAR_FLOAT("edit-bias-grab-distance", &editDistance, 0, 10, 1000);

    C_VAR_FLOAT("edit-bias-red", &editColor[0], CVF_NO_ARCHIVE, 0, 1);
    C_VAR_FLOAT("edit-bias-green", &editColor[1], CVF_NO_ARCHIVE, 0, 1);
    C_VAR_FLOAT("edit-bias-blue", &editColor[2], CVF_NO_ARCHIVE, 0, 1);
    C_VAR_FLOAT("edit-bias-intensity", &editIntensity, CVF_NO_ARCHIVE, 1, 50000);

    C_VAR_INT("edit-bias-blink", &editBlink, 0, 0, 1);
    C_VAR_INT("edit-bias-hide", &editHidden, 0, 0, 1);
    C_VAR_INT("edit-bias-show-sources", &editShowAll, 0, 0, 1);
    C_VAR_INT("edit-bias-show-indices", &editShowIndices, 0, 0, 1);

    // Commands for light editing.
    C_CMD_FLAGS("bledit", "", BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blquit", "", BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blclear", "", BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blsave", NULL, BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blnew", "", BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("bldel", "", BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("bllock", "", BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blunlock", "", BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blgrab", "", BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("bldup", "", BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blc", "fff", BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("bli", NULL, BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blhue", NULL, BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blmenu", "", BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
}

/**
 * Editor Functionality:
 */

static void SBE_GetHand(float pos[3])
{
    const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);
    pos[0] = vx + viewData->frontVec[VX] * editDistance;
    pos[1] = vz + viewData->frontVec[VZ] * editDistance;
    pos[2] = vy + viewData->frontVec[VY] * editDistance;
}

static source_t *SBE_GrabSource(int index)
{
    source_t           *s;
    int                 i;

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
    float               hand[3];
    source_t           *nearest = NULL, *s;
    float               minDist = 0, len;
    int                 i;

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
    const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);

    dot = M_DotProduct(viewData->frontVec, hueOrigin);
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

        R_HSVToRGB(color, 0, 0, 1);
        return;
    }

    // Calculate hue angle by projecting the current viewfront to the
    // hue circle plane.  Project onto the normal and subtract.
    scale = M_DotProduct(viewData->frontVec, hueOrigin) /
        M_DotProduct(hueOrigin, hueOrigin);
    M_Scale(h, hueOrigin, scale);

    for(i = 0; i < 3; ++i)
        proj[i] = viewData->frontVec[i] - h[i];

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

    R_HSVToRGB(color, hue, saturation, 1);
}

void SBE_EndFrame(void)
{
    source_t           *src;

    // Update the grabbed light.
    if(editActive && (src = SBE_GetGrabbed()) != NULL)
    {
        source_t            old;

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
    gameDrawHUD = false;
    editActive = true;
    editGrabbed = -1;

    Con_Printf("Bias light editor: ON\n");
}

static void SBE_End(void)
{
    // Advise the game it can safely draw any HUD displays again
    gameDrawHUD = true;
    editActive = false;

    Con_Printf("Bias light editor: OFF\n");
}

static boolean SBE_New(void)
{
    source_t           *s;

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
    source_t           *orig = SB_GetSource(which);
    source_t           *s;
    int                 i;

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

static boolean SBE_Save(const char* name)
{
    gamemap_t* map = P_GetCurrentMap();
    const char* uid = P_GetUniqueMapID(map);
    ddstring_t fileName;
    source_t* s;
    FILE* file;

    Str_Init(&fileName);
    if(NULL == name || !name[0])
    {
        Str_Appendf(&fileName, "%s.ded", P_GetMapID(map));
    }
    else
    {
        Str_Set(&fileName, name);
        F_FixSlashes(&fileName, &fileName);
        F_ExpandBasePath(&fileName, &fileName);
        // Do we need to append an extension?
        if(NULL == F_FindFileExtension(Str_Text(&fileName)))
        {
            Str_Append(&fileName, ".ded");
        }
    }

    file = fopen(Str_Text(&fileName), "wt");
    if(NULL == file)
    {
        Con_Message("Warning failed to open \"%s\" for writing. Bias Lights not saved.\n", Str_Text(&fileName));
        Str_Free(&fileName);
        return false;
    }

    Con_Printf("Saving to \"%s\"...\n", F_PrettyPath(Str_Text(&fileName)));

    fprintf(file, "# %i Bias Lights for %s\n\n", numSources, uid);

    // Since there can be quite a lot of these, make sure we'll skip
    // the ones that are definitely not suitable.
    fprintf(file, "SkipIf Not %s\n", Str_Text(GameInfo_IdentityKey(DD_GameInfo())));

    s = SB_GetSource(0);
    { int i;
    for(i = 0; i < numSources; ++i, ++s)
    {
        fprintf(file, "\nLight {\n");
        fprintf(file, "  Map = \"%s\"\n", uid);
        fprintf(file, "  Origin { %g %g %g }\n",
                s->pos[0], s->pos[1], s->pos[2]);
        fprintf(file, "  Color { %g %g %g }\n",
                s->color[0], s->color[1], s->color[2]);
        fprintf(file, "  Intensity = %g\n", s->primaryIntensity);
        fprintf(file, "  Sector levels { %g %g }\n", s->sectorLevel[0],
                s->sectorLevel[1]);
        fprintf(file, "}\n");
    }}

    fclose(file);
    Str_Free(&fileName);
    return true;
}

void SBE_MenuSave(ui_object_t *ob)
{
    SBE_Save(saveFile);
}

void SBE_SetHueCircle(boolean activate)
{
    int i;

    if((signed) activate == editHueCircle)
        return; // No change in state.

    if(activate && SBE_GetGrabbed() == NULL)
        return;

    editHueCircle = activate;

    if(activate)
    {
        const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);

        // Determine the orientation of the hue circle.
        for(i = 0; i < 3; ++i)
        {
            hueOrigin[i] = viewData->frontVec[i];
            hueSide[i] = viewData->sideVec[i];
            hueUp[i] = viewData->upVec[i];
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
    char               *cmd = argv[0] + 2;
    int                 which;

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
        Con_Open(false); // Close the console if open.

        // Show the bias menu interface.
        UI_InitPage(&page_bias, ob_bias);
        page_bias.flags.showBackground = false; // We don't want a background.
        UI_PageInit(false, true, true, true, false);
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
        source_t           *src = SB_GetSource(which);
        float               r = 1, g = 1, b = 1;

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
        source_t           *src = SB_GetSource(which);

        if(argc >= 3)
        {
            src->sectorLevel[0] = strtod(argv[1], NULL) / 255.0f;
            if(src->sectorLevel[0] < 0)
                src->sectorLevel[0] = 0;
            else if(src->sectorLevel[0] > 1)
                src->sectorLevel[0] = 1;

            src->sectorLevel[1] = strtod(argv[2], NULL) / 255.0f;
            if(src->sectorLevel[1] < 0)
                src->sectorLevel[1] = 0;
            else if(src->sectorLevel[1] > 1)
                src->sectorLevel[1] = 1;
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

static void SBE_DrawBox(int x, int y, int w, int h, ui_color_t* c)
{
    UI_GradientEx(x, y, w, h, 6,
                  c ? c : UI_Color(UIC_BG_MEDIUM),
                  c ? c : UI_Color(UIC_BG_LIGHT),
                  .2f, .4f);
    UI_DrawRectEx(x, y, w, h, 6, false, c ? c : UI_Color(UIC_BRD_HI),
                  NULL, .4f, -1);
}

static void SBE_InfoBox(source_t* s, int rightX, char* title, float alpha)
{
    int w, h, th, x, y;
    ui_color_t color;
    char buf[80];
    float eye[3];

    FR_SetFont(glFontFixed);
    FR_LoadDefaultAttrib();
    w = 16 + FR_TextFragmentWidth("R:0.000 G:0.000 B:0.000");
    th = FR_TextFragmentHeight("Info");
    h = 16 + th * 6;

    x = theWindow->width  - 10 - w - rightX;
    y = theWindow->height - 10 - h;

    eye[0] = vx;
    eye[1] = vz;
    eye[2] = vy;

    color.red   = s->color[CR];
    color.green = s->color[CG];
    color.blue  = s->color[CB];

    SBE_DrawBox(x, y, w, h, &color);
    x += 8;
    y += 8 + th/2;

    // - index #
    // - locked status
    // - coordinates
    // - distance from eye
    // - intensity
    // - color

    FR_SetFont(glFontFixed);
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);
    UI_TextOutEx2(title, x, y, UI_Color(UIC_TITLE), alpha, ALIGN_LEFT, DTF_ONLY_SHADOW);
    y += th;

    sprintf(buf, "# %03i %s", SB_ToIndex(s), (s->flags & BLF_LOCKED ? "(lock)" : ""));
    UI_TextOutEx2(buf, x, y, UI_Color(UIC_TEXT), alpha, ALIGN_LEFT, DTF_ONLY_SHADOW);
    y += th;

    sprintf(buf, "(%+06.0f,%+06.0f,%+06.0f)", s->pos[0], s->pos[1], s->pos[2]);
    UI_TextOutEx2(buf, x, y, UI_Color(UIC_TEXT), alpha, ALIGN_LEFT, DTF_ONLY_SHADOW);
    y += th;

    sprintf(buf, "Distance:%-.0f", M_Distance(eye, s->pos));
    UI_TextOutEx2(buf, x, y, UI_Color(UIC_TEXT), alpha, ALIGN_LEFT, DTF_ONLY_SHADOW);
    y += th;

    sprintf(buf, "Intens:%-5.0f L:%3i/%3i", s->primaryIntensity,
            (int) (255.0f * s->sectorLevel[0]),
            (int) (255.0f * s->sectorLevel[1]));

    UI_TextOutEx2(buf, x, y, UI_Color(UIC_TEXT), alpha, ALIGN_LEFT, DTF_ONLY_SHADOW);
    y += th;

    sprintf(buf, "R:%.3f G:%.3f B:%.3f", s->color[0], s->color[1], s->color[2]);
    UI_TextOutEx2(buf, x, y, UI_Color(UIC_TEXT), alpha, ALIGN_LEFT, DTF_ONLY_SHADOW);
    y += th;
}


/*
 * Editor HUD
 */

static void SBE_DrawLevelGauge(int x, int y, int height)
{
    static sector_t* lastSector = NULL;
    static float minLevel = 0, maxLevel = 0;

    int off, secY, maxY = 0, minY = 0, p;
    sector_t* sector;
    source_t* src;
    char buf[80];

    if(SBE_GetGrabbed())
        src = SBE_GetGrabbed();
    else
        src = SBE_GetNearest();

    sector = R_PointInSubsector(src->pos[VX], src->pos[VY])->sector;

    if(lastSector != sector)
    {
        minLevel = maxLevel = sector->lightLevel;
        lastSector = sector;
    }

    if(sector->lightLevel < minLevel)
        minLevel = sector->lightLevel;
    if(sector->lightLevel > maxLevel)
        maxLevel = sector->lightLevel;

    FR_SetFont(glFontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);
    off = FR_TextFragmentWidth("000");

    glBegin(GL_LINES);
    glColor4f(1, 1, 1, .5f);
    glVertex2f(x + off, y);
    glVertex2f(x + off, y + height);
    // Normal lightlevel.
    secY = y + height * (1.0f - sector->lightLevel);
    glVertex2f(x + off - 4, secY);
    glVertex2f(x + off, secY);
    if(maxLevel != minLevel)
    {
        // Max lightlevel.
        maxY = y + height * (1.0f - maxLevel);
        glVertex2f(x + off + 4, maxY);
        glVertex2f(x + off, maxY);
        // Min lightlevel.
        minY = y + height * (1.0f - minLevel);
        glVertex2f(x + off + 4, minY);
        glVertex2f(x + off, minY);
    }
    // Current min/max bias sector level.
    if(src->sectorLevel[0] > 0 || src->sectorLevel[1] > 0)
    {
        glColor3f(1, 0, 0);
        p = y + height * (1.0f - src->sectorLevel[0]);
        glVertex2f(x + off + 2, p);
        glVertex2f(x + off - 2, p);

        glColor3f(0, 1, 0);
        p = y + height * (1.0f - src->sectorLevel[1]);
        glVertex2f(x + off + 2, p);
        glVertex2f(x + off - 2, p);
    }
    glEnd();

    glEnable(GL_TEXTURE_2D);

    // The number values.
    sprintf(buf, "%03i", (short) (255.0f * sector->lightLevel));
    UI_TextOutEx2(buf, x, secY, UI_Color(UIC_TITLE), .7f, 0, DTF_ONLY_SHADOW);
    if(maxLevel != minLevel)
    {
        sprintf(buf, "%03i", (short) (255.0f * maxLevel));
        UI_TextOutEx2(buf, x + 2*off, maxY, UI_Color(UIC_TEXT), .7f, 0, DTF_ONLY_SHADOW);
        sprintf(buf, "%03i", (short) (255.0f * minLevel));
        UI_TextOutEx2(buf, x + 2*off, minY, UI_Color(UIC_TEXT), .7f, 0, DTF_ONLY_SHADOW);
    }

    glDisable(GL_TEXTURE_2D);
}

void SBE_DrawHUD(void)
{
    int                 w, h, y;
    char                buf[80];
    float               alpha = .8f;
    gamemap_t          *map = P_GetCurrentMap();
    source_t           *s;

    if(!editActive || editHidden)
        return;

    // Go into screen projection mode.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, theWindow->width, theWindow->height, 0, -1, 1);

    glEnable(GL_TEXTURE_2D);

    // Overall stats: numSources / MAX (left)
    sprintf(buf, "%i / %i (%i free)", numSources, MAX_BIAS_LIGHTS, MAX_BIAS_LIGHTS - numSources);
    FR_SetFont(glFontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);
    w = FR_TextFragmentWidth(buf) + 16;
    h = FR_TextFragmentHeight(buf) + 16;
    y = theWindow->height - 10 - h;
    SBE_DrawBox(10, y, w, h, 0);
    UI_TextOutEx2(buf, 18, y + h / 2, UI_Color(UIC_TITLE), alpha, ALIGN_LEFT, DTF_ONLY_SHADOW);

    // The map ID.
    UI_TextOutEx2(P_GetUniqueMapID(map), 18, y - h/2, UI_Color(UIC_TITLE), alpha, ALIGN_LEFT, DTF_ONLY_SHADOW);

    // Stats for nearest & grabbed:
    if(numSources)
    {
        s = SBE_GetNearest();
        SBE_InfoBox(s, 0, SBE_GetGrabbed() ? "Nearest" : "Highlighted", alpha);
    }

    if((s = SBE_GetGrabbed()) != NULL)
    {
        int x;
        FR_SetFont(glFontFixed);
        x = FR_TextFragmentWidth("0") * 26;
        SBE_InfoBox(s, x, "Grabbed", alpha);
    }

    if(SBE_GetGrabbed() || SBE_GetNearest())
    {
        SBE_DrawLevelGauge(20, theWindow->height/2 - 255/2, 255);
    }

    glDisable(GL_TEXTURE_2D);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

void SBE_DrawStar(float pos[3], float size, float color[4])
{
    float               black[4] = { 0, 0, 0, 0 };

    glBegin(GL_LINES);
        glColor4fv(black);
        glVertex3f(pos[VX] - size, pos[VZ], pos[VY]);
        glColor4fv(color);
        glVertex3f(pos[VX], pos[VZ], pos[VY]);
        glVertex3f(pos[VX], pos[VZ], pos[VY]);
        glColor4fv(black);
        glVertex3f(pos[VX] + size, pos[VZ], pos[VY]);

        glVertex3f(pos[VX], pos[VZ] - size, pos[VY]);
        glColor4fv(color);
        glVertex3f(pos[VX], pos[VZ], pos[VY]);
        glVertex3f(pos[VX], pos[VZ], pos[VY]);
        glColor4fv(black);
        glVertex3f(pos[VX], pos[VZ] + size, pos[VY]);

        glVertex3f(pos[VX], pos[VZ], pos[VY] - size);
        glColor4fv(color);
        glVertex3f(pos[VX], pos[VZ], pos[VY]);
        glVertex3f(pos[VX], pos[VZ], pos[VY]);
        glColor4fv(black);
        glVertex3f(pos[VX], pos[VZ], pos[VY] + size);
    glEnd();
}

static void SBE_DrawIndex(source_t *src)
{
    char                buf[80];
    float               eye[3], scale;

    if(!editShowIndices)
        return;

    eye[0] = vx;
    eye[1] = vz;
    eye[2] = vy;
    scale = M_Distance(src->pos, eye) / (theWindow->width / 2);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(src->pos[VX], src->pos[VZ], src->pos[VY]);
    glRotatef(-vang + 180, 0, 1, 0);
    glRotatef(vpitch, 1, 0, 0);
    glScalef(-scale, -scale, 1);

    // Show the index number of the source.
    sprintf(buf, "%i", SB_ToIndex(src));
    FR_SetFont(glFontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);
    UI_TextOutEx(buf, 2, 2, UI_Color(UIC_TITLE), 1 - M_Distance(src->pos, eye)/2000);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
}

static void SBE_DrawSource(source_t *src)
{
    float               col[4], d;
    float               eye[3];

    eye[0] = vx;
    eye[1] = vz;
    eye[2] = vy;

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
    vec3_t              eye;
    vec3_t              center, off, off2;
    float               steps = 32, inner = 10, outer = 30, s;
    double              angle;
    float               color[4], sel[4], hue, saturation;
    int                 i;

    eye[0] = vx;
    eye[1] = vy;
    eye[2] = vz;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    glTranslatef(vx, vy, vz);
    glScalef(1, 1.0f/1.2f, 1);
    glTranslatef(-vx, -vy, -vz);

    // The origin of the circle.
    for(i = 0; i < 3; ++i)
        center[i] = eye[i] + hueOrigin[i] * hueDistance;

    // Draw the circle.
    glBegin(GL_QUAD_STRIP);
    for(i = 0; i <= steps; ++i)
    {
        angle = 2*PI * i/steps;

        // Calculate the hue color for this angle.
        R_HSVToRGB(color, i/steps, 1, 1);
        color[3] = .5f;

        SBE_HueOffset(angle, off);

        glColor4fv(color);
        glVertex3f(center[0] + outer * off[0], center[1] + outer * off[1],
                   center[2] + outer * off[2]);

        // Saturation decreases in the center.
        color[0] = 1;
        color[1] = 1;
        color[2] = 1;
        color[3] = .15f;
        glColor4fv(color);
        glVertex3f(center[0] + inner * off[0], center[1] + inner * off[1],
                   center[2] + inner * off[2]);
    }
    glEnd();

    glBegin(GL_LINES);

    // Draw the current hue.
    SBE_GetHueColor(sel, &hue, &saturation);
    SBE_HueOffset(2*PI * hue, off);
    sel[3] = 1;
    if(saturation > 0)
    {
        glColor4fv(sel);
        glVertex3f(center[0] + outer * off[0], center[1] + outer * off[1],
                   center[2] + outer * off[2]);
        glVertex3f(center[0] + inner * off[0], center[1] + inner * off[1],
                   center[2] + inner * off[2]);
    }

    // Draw the edges.
    for(i = 0; i < steps; ++i)
    {
        SBE_HueOffset(2*PI * i/steps, off);
        SBE_HueOffset(2*PI * (i + 1)/steps, off2);

        // Calculate the hue color for this angle.
        R_HSVToRGB(color, i/steps, 1, 1);
        color[3] = 1;

        glColor4fv(color);
        glVertex3f(center[0] + outer * off[0], center[1] + outer * off[1],
                   center[2] + outer * off[2]);
        glVertex3f(center[0] + outer * off2[0], center[1] + outer * off2[1],
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
        glColor4fv(sel);
        s = inner + (outer - inner) * saturation;
        glVertex3f(center[0] + s * off[0], center[1] + s * off[1],
                   center[2] + s * off[2]);
        glVertex3f(center[0] + s * off2[0], center[1] + s * off2[1],
                   center[2] + s * off2[2]);
    }
    glEnd();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void SBE_DrawCursor(void)
{
#define SET_COL(x, r, g, b, a) {x[0]=(r); x[1]=(g); x[2]=(b); x[3]=(a);}

    double              t = Sys_GetRealTime()/100.0f;
    source_t           *s;
    float               hand[3];
    float               size = 10000, distance;
    float               col[4], eye[3];

    if(!editActive || !numSources || editHidden || freezeRLs)
        return;

    eye[0] = vx;
    eye[1] = vz;
    eye[2] = vy;

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

    SBE_GetHand(hand);
    if((distance = M_Distance(s->pos, hand)) > 2 * editDistance)
    {
        // Show where it is.
        glDisable(GL_DEPTH_TEST);
    }

    SBE_DrawStar(s->pos, size, col);
    SBE_DrawIndex(s);

    // Show if the source is locked.
    if(s->flags & BLF_LOCKED)
    {
        float lock = 2 + M_Distance(eye, s->pos)/100;

        glColor4f(1, 1, 1, 1);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glTranslatef(s->pos[VX], s->pos[VZ], s->pos[VY]);

        glRotatef(t/2, 0, 0, 1);
        glRotatef(t, 1, 0, 0);
        glRotatef(t * 15, 0, 1, 0);

        glBegin(GL_LINES);
            glVertex3f(-lock, 0, -lock);
            glVertex3f(+lock, 0, -lock);

            glVertex3f(+lock, 0, -lock);
            glVertex3f(+lock, 0, +lock);

            glVertex3f(+lock, 0, +lock);
            glVertex3f(-lock, 0, +lock);

            glVertex3f(-lock, 0, +lock);
            glVertex3f(-lock, 0, -lock);
        glEnd();

        glPopMatrix();
    }

    if(SBE_GetNearest() != SBE_GetGrabbed() && SBE_GetGrabbed())
    {
        glDisable(GL_DEPTH_TEST);
        SBE_DrawSource(SBE_GetNearest());
    }

    // Show all sources?
    if(editShowAll)
    {
        int i;
        source_t *src;

        glDisable(GL_DEPTH_TEST);
        src = SB_GetSource(0);
        for(i = 0; i < numSources; ++i, src++)
        {
            if(s == src)
                continue;

            SBE_DrawSource(src);
        }
    }

    glEnable(GL_DEPTH_TEST);

#undef SET_COL
}
