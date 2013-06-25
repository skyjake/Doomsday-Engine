/** @file edit_bias.cpp Shadow Bias editor UI.
 *
 * @authors Copyright © 2006-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifdef __CLIENT__

#include <de/Log>

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"
#include "de_ui.h"

#include "Hand"
#include "HueCircle"
#include "world/map.h"
#include "world/p_players.h" // viewPlayer

#include "render/r_main.h" // viewdata_t, remove me
#include "render/rend_main.h" // gameDrawHUD/vOrigin, remove me

#include "edit_bias.h"

using namespace de;

D_CMD(BLEditor);

/*
 * Editor variables:
 */
int editHidden;
int editBlink;
int editShowAll;
int editShowIndices = true;

/*
 * Editor status:
 */
static bool editActive; // Edit mode active?
static bool editHueCircle;
static HueCircle *hueCircle;

void SBE_Register()
{
    // Variables.
    C_VAR_INT("edit-bias-blink",         &editBlink,          0, 0, 1);
    C_VAR_INT("edit-bias-hide",          &editHidden,         0, 0, 1);
    C_VAR_INT("edit-bias-show-sources",  &editShowAll,        0, 0, 1);
    C_VAR_INT("edit-bias-show-indices",  &editShowIndices,    0, 0, 1);

    // Commands.
    C_CMD_FLAGS("bledit",   "",     BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blquit",   "",     BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blclear",  "",     BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blsave",   NULL,   BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blnew",    "",     BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("bldel",    "",     BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("bllock",   NULL,   BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blunlock", NULL,   BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blgrab",   NULL,   BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blungrab", NULL,   BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("bldup",    "",     BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blc",      "fff",  BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("bli",      NULL,   BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("bllevels", NULL,   BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blhue",    NULL,   BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
}

bool SBE_Active()
{
    return editActive;
}

HueCircle *SBE_HueCircle()
{
    if(!editActive || !editHueCircle) return 0;
    return hueCircle;
}

void SBE_SetHueCircle(bool activate = true)
{
    if(!editActive) return;

    // Any change in state?
    if(activate == editHueCircle) return;

    // The circle can only be activated when something is grabbed.
    if(activate && App_World().hand().isEmpty()) return;

    editHueCircle = activate;

    if(activate)
    {
        viewdata_t const &viewer = *R_ViewData(viewPlayer - ddPlayers);
        hueCircle->setOrientation(viewer.frontVec, viewer.sideVec, viewer.upVec);
    }
}

/*
 * Editor Functionality:
 */

static void SBE_Begin()
{
    if(editActive) return;

#ifdef __CLIENT__
    // Advise the game not to draw any HUD displays
    gameDrawHUD = false;
#endif

    editActive = true;
    editHueCircle = false;
    hueCircle = new HueCircle;

    LOG_AS("Bias");
    LOG_VERBOSE("Editing begins.");
}

static void SBE_End()
{
    if(!editActive) return;

    App_World().hand().ungrab();

    delete hueCircle; hueCircle = 0;
    editHueCircle = false;
    editActive = false;

#ifdef __CLIENT__
    // Advise the game it can safely draw any HUD displays again
    gameDrawHUD = true;
#endif

    LOG_AS("Bias");
    LOG_VERBOSE("Editing ends.");
}

static void SBE_Clear()
{
    DENG_ASSERT(editActive);
    App_World().map().removeAllBiasSources();
}

static void SBE_Delete(int which)
{
    DENG_ASSERT(editActive);
    App_World().map().removeBiasSource(which);
}

static BiasSource *SBE_New()
{
    DENG_ASSERT(editActive);
    try
    {
        Hand &hand = App_World().hand();
        BiasSource &source = App_World().map().addBiasSource(hand.origin());

        // Update the edit properties.
        hand.setEditIntensity(source.intensity());
        hand.setEditColor(source.color());

        hand.grab(source);

        // As this is a new source -- unlock immediately.
        source.unlock();

        return &source;
    }
    catch(Map::FullError const &)
    {} // Ignore this error.

    return 0;
}

static BiasSource *SBE_Dupe(BiasSource const &other)
{
    DENG_ASSERT(editActive);
    try
    {
        Hand &hand = App_World().hand();
        BiasSource &source = App_World().map().addBiasSource(other); // A copy is made.

        source.setOrigin(hand.origin());

        // Update the edit properties.
        hand.setEditIntensity(source.intensity());
        hand.setEditColor(source.color());

        hand.grab(source);

        // As this is a new source -- unlock immediately.
        source.unlock();

        return &source;
    }
    catch(Map::FullError const &)
    {} // Ignore this error.

    return 0;
}

static void SBE_Grab(int which)
{
    DENG_ASSERT(editActive);
    Hand &hand = App_World().hand();
    if(BiasSource *source = App_World().map().biasSource(which))
    {
        if(hand.isEmpty())
        {
            // Update the edit properties.
            hand.setEditIntensity(source->intensity());
            hand.setEditColor(source->color());
        }

        hand.grabMulti(*source);
    }
}

static void SBE_Ungrab(int which)
{
    DENG_ASSERT(editActive);
    Hand &hand = App_World().hand();
    if(BiasSource *source = App_World().map().biasSource(which))
    {
        hand.ungrab(*source);
    }
    else
    {
        hand.ungrab();
    }
}

static void SBE_SetLock(int which, bool enable = true)
{
    DENG_ASSERT(editActive);
    Hand &hand = App_World().hand();
    if(BiasSource *source = App_World().map().biasSource(which))
    {
        if(enable) source->lock();
        else       source->unlock();
        return;
    }

    foreach(Grabbable *grabbable, hand.grabbed())
    {
        if(enable) grabbable->lock();
        else       grabbable->unlock();
    }
}

static bool SBE_Save(char const *name = 0)
{
    DENG_ASSERT(editActive);

    LOG_AS("Bias");

    ddstring_t fileName; Str_Init(&fileName);
    if(!name || !name[0])
    {
        String mapPath = App_World().map().uri().resolvedRef() + ".ded";
        Str_Set(&fileName, mapPath.toUtf8().constData());
    }
    else
    {
        Str_Set(&fileName, name);
        F_ExpandBasePath(&fileName, &fileName);
        // Do we need to append an extension?
        if(!F_FindFileExtension(Str_Text(&fileName)))
        {
            Str_Append(&fileName, ".ded");
        }
    }

    F_ToNativeSlashes(&fileName, &fileName);
    FILE *file = fopen(Str_Text(&fileName), "wt");
    if(!file)
    {
        LOG_WARNING("Failed opening \"%s\". Sources were not saved.")
                << F_PrettyPath(Str_Text(&fileName));
        Str_Free(&fileName);
        return false;
    }

    LOG_VERBOSE("Saving to \"%s\"...") << F_PrettyPath(Str_Text(&fileName));

    Map &map = App_World().map();

    char const *uid = map.oldUniqueId();
    fprintf(file, "# %i Bias Lights for %s\n\n", map.biasSourceCount(), uid);

    // Since there can be quite a lot of these, make sure we'll skip
    // the ones that are definitely not suitable.
    fprintf(file, "SkipIf Not %s\n", Str_Text(App_CurrentGame().identityKey()));

    foreach(BiasSource *src, map.biasSources())
    {
        fprintf(file, "\nLight {\n");
        fprintf(file, "  Map = \"%s\"\n", uid);
        fprintf(file, "  Origin { %g %g %g }\n",
                      src->origin().x, src->origin().y, src->origin().z);
        fprintf(file, "  Color { %g %g %g }\n",
                      src->color().x, src->color().y, src->color().z);
        fprintf(file, "  Intensity = %g\n", src->intensity());

        float minLight, maxLight;
        src->lightLevels(minLight, maxLight);
        fprintf(file, "  Sector levels { %g %g }\n", minLight, maxLight);
        fprintf(file, "}\n");
    }

    fclose(file);
    Str_Free(&fileName);
    return true;
}

/*
 * Editor commands.
 */
D_CMD(BLEditor)
{
    DENG_UNUSED(src);

    char *cmd = argv[0] + 2;

    if(!qstricmp(cmd, "edit"))
    {
        SBE_Begin();
        return true;
    }

    if(!editActive)
    {
        LOG_MSG("The bias lighting editor is not active.");
        return false;
    }

    if(!qstricmp(cmd, "quit"))
    {
        SBE_End();
        return true;
    }

    if(!qstricmp(cmd, "save"))
    {
        return SBE_Save(argc >= 2 ? argv[1] : 0);
    }

    if(!qstricmp(cmd, "clear"))
    {
        SBE_Clear();
        return true;
    }

    if(!qstricmp(cmd, "hue"))
    {
        int activate = (argc >= 2 ? stricmp(argv[1], "off") : !editHueCircle);
        SBE_SetHueCircle(activate);
        return true;
    }

    Map &map = App_World().map();
    coord_t handDistance;
    Hand &hand = App_World().hand(&handDistance);

    if(!qstricmp(cmd, "new"))
    {
        return SBE_New() != 0;
    }

    if(!qstricmp(cmd, "c"))
    {
        // Update the edit properties.
        hand.setEditColor(Vector3f(argc > 1? strtod(argv[1], 0) : 1,
                                   argc > 2? strtod(argv[2], 0) : 1,
                                   argc > 3? strtod(argv[3], 0) : 1));
        return true;
    }

    if(!qstricmp(cmd, "i"))
    {
        hand.setEditIntensity(argc > 1? strtod(argv[1], 0) : 200);
        return true;
    }

    if(!qstricmp(cmd, "grab"))
    {
        SBE_Grab(map.toIndex(*map.biasSourceNear(hand.origin())));
        return true;
    }

    if(!qstricmp(cmd, "ungrab"))
    {
        SBE_Ungrab(argc > 1? atoi(argv[1]) : -1);
        return true;
    }

    if(!qstricmp(cmd, "lock"))
    {
        SBE_SetLock(argc > 1? atoi(argv[1]) : -1);
        return true;
    }

    if(!qstricmp(cmd, "unlock"))
    {
        SBE_SetLock(argc > 1? atoi(argv[1]) : -1, false);
        return true;
    }

    // Has a source index been given as an argument?
    int which = -1;
    if(!hand.isEmpty())
    {
        which = map.toIndex(hand.grabbed().first()->as<BiasSource>());
    }
    else
    {
        which = map.toIndex(*map.biasSourceNear(hand.origin()));
    }

    if(argc > 1)
    {
        which = atoi(argv[1]);
    }

    if(which < 0 || which >= map.biasSourceCount())
    {
        LOG_MSG("Invalid source index #%i") << which;
        return false;
    }

    if(!qstricmp(cmd, "del"))
    {
        SBE_Delete(which);
        return true;
    }

    if(!qstricmp(cmd, "dup"))
    {
        return SBE_Dupe(*map.biasSource(which)) != 0;
    }

    if(!qstricmp(cmd, "levels"))
    {
        float minLight = 0, maxLight = 0;
        if(argc >= 2)
        {
            minLight = strtod(argv[1], 0) / 255.0f;
            maxLight = argc >= 3? strtod(argv[2], 0) / 255.0f : minLight;
        }
        map.biasSource(which)->setLightLevels(minLight, maxLight);
        return true;
    }

    return false;
}

/*
 * Editor visuals (would-be widgets):
 */

#include "world/map.h"
#include "world/p_players.h"
#include "BspLeaf"

#include "render/rend_font.h"

static void drawBoxBackground(Vector2i const &origin_, Vector2i const &size_, ui_color_t *color)
{
    Point2Raw origin(origin_.x, origin_.y);
    Size2Raw size(size_.x, size_.y);
    UI_GradientEx(&origin, &size, 6,
                  color ? color : UI_Color(UIC_BG_MEDIUM),
                  color ? color : UI_Color(UIC_BG_LIGHT),
                  .2f, .4f);
    UI_DrawRectEx(&origin, &size, 6, false, color ? color : UI_Color(UIC_BRD_HI),
                  NULL, .4f, -1);
}

static void drawText(String const &text, Vector2i const &origin_, ui_color_t *color, float alpha,
                     int align = ALIGN_LEFT, short flags = DTF_ONLY_SHADOW)
{
    Point2Raw origin(origin_.x, origin_.y);
    UI_TextOutEx2(text.toUtf8().constData(), &origin, color, alpha, align, flags);
}

/**
 * - index #, lock status
 * - origin
 * - distance from eye
 * - intensity, light level threshold
 * - color
 */
static void drawInfoBox(BiasSource *s, int rightX, String const title, float alpha)
{
    int const precision = 3;

    if(!s) return;

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    int th = FR_SingleLineHeight("Info");
    Vector2i size(16 + FR_TextWidth("Color:(0.000, 0.000, 0.000)"), 16 + th * 6);

    Vector2i origin(DENG_WINDOW->width()  - 10 - size.x - rightX,
                    DENG_WINDOW->height() - 10 - size.y);

    ui_color_t color;
    color.red   = s->color().x;
    color.green = s->color().y;
    color.blue  = s->color().z;

    DENG_ASSERT_IN_MAIN_THREAD();

    glEnable(GL_TEXTURE_2D);

    drawBoxBackground(origin, size, &color);
    origin.x += 8;
    origin.y += 8 + th/2;

    drawText(title, origin, UI_Color(UIC_TITLE), alpha);
    origin.y += th;

    int sourceIndex = App_World().map().toIndex(*s);
    coord_t distance = (s->origin() - Vector3d(vOrigin[VX], vOrigin[VZ], vOrigin[VY])).length();
    float minLight, maxLight;
    s->lightLevels(minLight, maxLight);

    String text1 = String("#%1").arg(sourceIndex, 3, 10, QLatin1Char('0')) + (s->isLocked()? " (locked)" : "");
    drawText(text1, origin, UI_Color(UIC_TEXT), alpha);
    origin.y += th;

    String text2 = String("Origin:") + s->origin().asText();
    drawText(text2, origin, UI_Color(UIC_TEXT), alpha);
    origin.y += th;

    String text3 = String("Distance:%1").arg(distance, 5, 'f', precision, QLatin1Char('0'));
    drawText(text3, origin, UI_Color(UIC_TEXT), alpha);
    origin.y += th;

    String text4 = String("Intens:%1").arg(s->intensity(), 5, 'f', precision, QLatin1Char('0'));
    if(!de::fequal(minLight, 0) || !de::fequal(maxLight, 0))
        text4 += String(" L:%2/%3").arg(int(255.0f * minLight), 3).arg(int(255.0f * maxLight), 3);
    drawText(text4, origin, UI_Color(UIC_TEXT), alpha);
    origin.y += th;

    String text5 = String("Color:(%1, %2, %3)").arg(s->color().x, 0, 'f', precision).arg(s->color().y, 0, 'f', precision).arg(s->color().z, 0, 'f', precision);
    drawText(text5, origin, UI_Color(UIC_TEXT), alpha);
    origin.y += th;

    glDisable(GL_TEXTURE_2D);
}

static void drawLightGauge(Vector2i const &origin, int height = 255)
{
    static float minLevel = 0, maxLevel = 0;
    static Sector *lastSector = 0;

    Hand &hand = App_World().hand();
    Map &map = App_World().map();

    BiasSource *src;
    if(!hand.isEmpty())
        src = &hand.grabbed().first()->as<BiasSource>();
    else
        src = map.biasSourceNear(hand.origin());

    Sector &sector = src->bspLeafAtOrigin().sector();
    if(lastSector != &sector)
    {
        minLevel = maxLevel = sector.lightLevel();
        lastSector = &sector;
    }

    if(sector.lightLevel() < minLevel)
        minLevel = sector.lightLevel();
    if(sector.lightLevel() > maxLevel)
        maxLevel = sector.lightLevel();

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);
    int off = FR_TextWidth("000");

    int minY = 0, maxY = 0;

    glBegin(GL_LINES);
    glColor4f(1, 1, 1, .5f);
    glVertex2f(origin.x + off, origin.y);
    glVertex2f(origin.x + off, origin.y + height);
    // Normal light level.
    int secY = origin.y + height * (1.0f - sector.lightLevel());
    glVertex2f(origin.x + off - 4, secY);
    glVertex2f(origin.x + off, secY);
    if(maxLevel != minLevel)
    {
        // Max light level.
        maxY = origin.y + height * (1.0f - maxLevel);
        glVertex2f(origin.x + off + 4, maxY);
        glVertex2f(origin.x + off, maxY);

        // Min light level.
        minY = origin.y + height * (1.0f - minLevel);
        glVertex2f(origin.x + off + 4, minY);
        glVertex2f(origin.x + off, minY);
    }

    // Current min/max bias sector level.
    float minLight, maxLight;
    src->lightLevels(minLight, maxLight);
    if(minLight > 0 || maxLight > 0)
    {
        glColor3f(1, 0, 0);
        int p = origin.y + height * (1.0f - minLight);
        glVertex2f(origin.x + off + 2, p);
        glVertex2f(origin.x + off - 2, p);

        glColor3f(0, 1, 0);
        p = origin.y + height * (1.0f - maxLight);
        glVertex2f(origin.x + off + 2, p);
        glVertex2f(origin.x + off - 2, p);
    }
    glEnd();

    glEnable(GL_TEXTURE_2D);

    // The number values.
    drawText(String::number(int(255.0f * sector.lightLevel())),
             Vector2i(origin.x, secY), UI_Color(UIC_TITLE), .7f, 0, DTF_ONLY_SHADOW);

    if(maxLevel != minLevel)
    {
        drawText(String::number(int(255.0f * maxLevel)),
                 Vector2i(origin.x + 2*off, maxY), UI_Color(UIC_TEXT), .7f, 0, DTF_ONLY_SHADOW);

        drawText(String::number(int(255.0f * minLevel)),
                 Vector2i(origin.x + 2*off, minY), UI_Color(UIC_TEXT), .7f, 0, DTF_ONLY_SHADOW);
    }

    glDisable(GL_TEXTURE_2D);
}

void SBE_DrawGui()
{
    float const opacity = .8f;

    if(!editActive || editHidden) return;

    if(!App_World().hasMap()) return;

    Map &map = App_World().map();
    Hand &hand = App_World().hand();

    DENG_ASSERT_IN_MAIN_THREAD();

    // Go into screen projection mode.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, DENG_WINDOW->width(), DENG_WINDOW->height(), 0, -1, 1);

    glEnable(GL_TEXTURE_2D);

    // Overall stats: numSources / MAX (left)
    String text = String("%1 / %2 (%3 free)")
                    .arg(map.biasSourceCount())
                    .arg(Map::MAX_BIAS_SOURCES)
                    .arg(Map::MAX_BIAS_SOURCES - map.biasSourceCount());

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    Vector2i size(FR_TextWidth(text.toUtf8().constData()) + 16,
                  FR_SingleLineHeight(text.toUtf8().constData()) + 16);
    int top = DENG_WINDOW->height() - 10 - size.y;

    Vector2i origin(10, top);
    drawBoxBackground(origin, size, 0);
    origin.x += 8;
    origin.y += size.y / 2;

    drawText(text, origin, UI_Color(UIC_TITLE), opacity);
    origin.y = top - size.y / 2;

    // The map ID.
    drawText(String(map.oldUniqueId()), origin, UI_Color(UIC_TITLE), opacity);

    glDisable(GL_TEXTURE_2D);

    if(map.biasSourceCount())
    {
        // Stats for nearest & grabbed:
        drawInfoBox(map.biasSourceNear(hand.origin()), 0, "Nearest", opacity);

        if(!hand.isEmpty())
        {
            FR_SetFont(fontFixed);
            int x = FR_TextWidth("0") * 30;
            drawInfoBox(&hand.grabbed().first()->as<BiasSource>(), x, "Grabbed", opacity);
        }

        drawLightGauge(Vector2i(20, DENG_WINDOW->height()/2 - 255/2));
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

#endif // __CLIENT__
