/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * m_profile.c: Common player profile menu.
 *
 * Contains an extension for edit fields.
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>
#include <ctype.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "hu_menu.h"
#include "hu_stuff.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void            M_EnterEditProfileMenu(int option, void* data);

void            M_DrawProfilesMenu(void);
void            M_DrawEditProfileMenu(void);

void            SCPlayerColor(int option, void* data);
#if __JHEXEN__
void            SCPlayerClass(int option, void* data);
#endif
void            M_SaveProfile(int option, void* data);
void            M_WeaponOrder(int option, void* context);
void            M_WeaponAutoSwitch(int option, void* context);
void            M_AmmoAutoSwitch(int option, void* context);

// Edit fields.
void            DrawEditField(menu_t* menu, int index, editfield_t* ef);
void            SCEditField(int efptr, void* data);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int menusnds[];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

editfield_t* ActiveEdit = NULL; // No active edit field by default.

editfield_t plrNameEd;
int CurrentPlrFrame = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int plrColor;

#if __JHEXEN__
static int plrClass;
#endif

static menuitem_t ProfilesItems[] = {
    {ITT_EFUNC, 0, "Edit Profile", M_EnterEditProfileMenu, 0 },
};

menu_t ProfilesDef = {
    0,
    32, 40,
    M_DrawProfilesMenu,
    1, ProfilesItems,
    0, MENU_NEWGAME,
    huFontB,                    //1, 0, 0,
    gs.cfg.menuColor,
    NULL, false,
    LINEHEIGHT_B,
    0, 1
};

menuitem_t EditProfileItems[] = {
    {ITT_EFUNC, 0, "", SCEditField, 0, NULL, &plrNameEd },
    {ITT_EMPTY, 0, "Gameplay", NULL, 0},
    {ITT_EFUNC, 0, "Always run :", M_ToggleVar, 0, NULL, "ctl-run"},
    {ITT_EFUNC, 0, "Use lookspring :", M_ToggleVar, 0, NULL, "ctl-look-spring"},
    {ITT_EFUNC, 0, "Use autoaim :", M_ToggleVar, 0, NULL, "ctl-aim-auto"},
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, "Weapons", NULL, 0},
    {ITT_EMPTY,  0, "Priority order", NULL, 0},
    {ITT_LRFUNC, 0, "1 :", M_WeaponOrder, 0 << NUM_WEAPON_TYPES },
    {ITT_LRFUNC, 0, "2 :", M_WeaponOrder, 1 << NUM_WEAPON_TYPES },
    {ITT_LRFUNC, 0, "3 :", M_WeaponOrder, 2 << NUM_WEAPON_TYPES },
    {ITT_LRFUNC, 0, "4 :", M_WeaponOrder, 3 << NUM_WEAPON_TYPES },
#if !__JHEXEN__
    {ITT_LRFUNC, 0, "5 :", M_WeaponOrder, 4 << NUM_WEAPON_TYPES },
    {ITT_LRFUNC, 0, "6 :", M_WeaponOrder, 5 << NUM_WEAPON_TYPES },
    {ITT_LRFUNC, 0, "7 :", M_WeaponOrder, 6 << NUM_WEAPON_TYPES },
    {ITT_LRFUNC, 0, "8 :", M_WeaponOrder, 7 << NUM_WEAPON_TYPES },
#endif
#if __JDOOM__ || __JDOOM64__
    {ITT_LRFUNC, 0, "9 :", M_WeaponOrder, 8 << NUM_WEAPON_TYPES },
#endif
#if __JDOOM64__
    {ITT_LRFUNC, 0, "10 :", M_WeaponOrder, 9 << NUM_WEAPON_TYPES },
#endif
    {ITT_EFUNC,  0, "Use with Next/Previous :", M_ToggleVar, 0, NULL, "player-weapon-nextmode"},
    {ITT_EMPTY,  0, NULL, NULL, 0},
    {ITT_EMPTY,  0, "AUTOSWITCH", NULL, 0},
    {ITT_LRFUNC, 0, "PICKUP WEAPON :", M_WeaponAutoSwitch, 0},
    {ITT_EFUNC,  0, "   IF NOT FIRING :", M_ToggleVar, 0, NULL, "player-autoswitch-notfiring"},
    {ITT_LRFUNC, 0, "PICKUP AMMO :", M_AmmoAutoSwitch, 0},
#if __JDOOM__ || __JDOOM64__
    {ITT_EFUNC,  0, "PICKUP BERSERK :", M_ToggleVar, 0, NULL, "player-autoswitch-berserk"},
#endif
#if __JDOOM__
    {ITT_EFUNC, 0, "Fix ouch face :", M_ToggleVar, 0, NULL, "hud-face-ouchfix"},
#endif
    {ITT_EMPTY, 0, NULL, NULL, 0},
#if __JHEXEN__
    {ITT_LRFUNC, 0, "Class :", SCPlayerClass, 0},
#else
    {ITT_EMPTY, 0, NULL, NULL, 0},
#endif
    {ITT_LRFUNC, 0, "Color :", SCPlayerColor, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EFUNC, 0, "Accept Changes", M_SaveProfile, 0 }
};

menu_t EditProfileDef = {
    MNF_NOHOTKEYS,
    60, 52,
    M_DrawEditProfileMenu,
#if __JDOOM64__
    30,
#elif __JDOOM__
    30,
#elif __JHERETIC__
    27,
#elif __JHEXEN__
    23,
#endif
    EditProfileItems,
    0, MENU_PROFILES,
    huFontA, gs.cfg.menuColor2, NULL, false, LINEHEIGHT_A,
#if __JDOOM__
    0, 13
#else
    0, 12
#endif
};

// CODE --------------------------------------------------------------------

void M_InitProfilesMenu(void)
{
    VERBOSE( Con_Message("M_InitProfilesMenu: Creating controls items.\n") );

    /// \todo enumerate profiles and construct selection boxes etc.
}

/**
 * M_DrawProfilesMenu
 */
void M_DrawProfilesMenu(void)
{
    const menu_t*       menu = &ProfilesDef;
    const menuitem_t*   item = menu->items + menu->firstItem;

    M_DrawTitle("Player Profiles", menu->y - 28);
}

static void drawSpritePreview(float x, float y, float availWidth,
                              float availHeight, int sprite, int frame,
                              int tclass, int tmap, float alpha)
{
    float               w, h, w2, h2, s, t, scale;
    spriteinfo_t        sprInfo;

    // Draw the color selection as a random player frame.
    R_GetSpriteInfo(sprite, frame, &sprInfo);

    w = sprInfo.width;
    h = sprInfo.height;
    w2 = M_CeilPow2(w);
    h2 = M_CeilPow2(h);
    // Let's calculate texture coordinates.
    // To remove a possible edge artifact, move the corner a bit up/left.
    s = (w - 0.4f) / w2 + 1.f / sprInfo.offset;
    t = (h - 0.4f) / h2 + 1.f / sprInfo.topOffset;

    if(h > w)
        scale = availHeight / h;
    else
        scale = availWidth / w;

    w *= scale;
    h *= scale;

    x -= sprInfo.width / 2 * scale;
    y -= sprInfo.height * scale;

    DGL_SetTranslatedSprite(sprInfo.material, tclass, tmap);

    DGL_Color4f(1, 1, 1, alpha);
    DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, 0 * s, 0);
        DGL_Vertex2f(x, y);

        DGL_TexCoord2f(0, 1 * s, 0);
        DGL_Vertex2f(x + w, y);

        DGL_TexCoord2f(0, 1 * s, t);
        DGL_Vertex2f(x + w, y + h);

        DGL_TexCoord2f(0, 0 * s, t);
        DGL_Vertex2f(x, y + h);
    DGL_End();
}

void M_DrawEditProfileMenu(void)
{
    menu_t*             menu = &EditProfileDef;
    int                 tmap = plrColor;
    float               menuAlpha = Hu_MenuAlpha();
    int                 i, idx = 0, tclass = 0;
#if __JHEXEN__
    int                 numColors = 8;
    static const int sprites[3] = {SPR_PLAY, SPR_CLER, SPR_MAGE};
#else
    int                 plrClass = 0;
    int                 numColors = 4;
    static const int    sprites[3] = {SPR_PLAY, SPR_PLAY, SPR_PLAY};
#endif
    float               x, y;
    char*               autoswitch[] = { "NEVER", "IF BETTER", "ALWAYS" };
#if __JHEXEN__
    char*               weaponids[] = { "First", "Second", "Third", "Fourth"};
#endif
#if __JDOOM__ || __JDOOM64__
    byte                berserkAutoSwitch = PLRPROFILE.inventory.berserkAutoSwitch;
#endif
#if __JHERETIC__ || __JHEXEN__
    char*               token;
#else
    char                buf[80];
#endif

#if __JDOOM__ || __JDOOM64__
    M_DrawTitle(GET_TXT(TXT_PLAYERPROFILE), menu->y - 28);
    Hu_MenuPageString(buf, menu);
    M_WriteText2(160 - M_StringWidth(buf, huFontA) / 2, menu->y - 12, buf,
                 huFontA, 1, .7f, .3f, Hu_MenuAlpha());
#else
    M_WriteText2(120, 100 - 98/gs.cfg.menuScale, GET_TXT(TXT_PLAYERPROFILE), huFontB, gs.cfg.menuColor[0],
                 gs.cfg.menuColor[1], gs.cfg.menuColor[2], Hu_MenuAlpha());

    DGL_Color4f(1, 1, 1, Hu_MenuAlpha());

    // Draw the page arrows.
    token = (!menu->firstItem || menuTime & 8) ? "invgeml2" : "invgeml1";
    GL_DrawPatch_CS(menu->x, menu->y - 12, W_GetNumForName(token));
    token = (menu->firstItem + menu->numVisItems >= menu->itemCount ||
             menuTime & 8) ? "invgemr2" : "invgemr1";
    GL_DrawPatch_CS(312 - menu->x, menu->y - 12, W_GetNumForName(token));
#endif

    DrawEditField(menu, idx++, &plrNameEd);
    idx++;
    M_WriteMenuText(menu, idx++, yesno[PLRPROFILE.ctrl.alwaysRun != 0]);
    M_WriteMenuText(menu, idx++, yesno[PLRPROFILE.camera.lookSpring != 0]);
    M_WriteMenuText(menu, idx++, yesno[PLRPROFILE.ctrl.useAutoAim != 0]);

    idx += 3;

    /**
     * \kludge Inform the user how to change the order.
     */
    if(itemOn >= 8 && itemOn < 8 + NUM_WEAPON_TYPES)
    {
        const char* str = "Use left/right to move weapon up/down";

        M_WriteText3(160 - M_StringWidth(str, huFontA) / 2,
                     200 - M_StringHeight(str, huFontA) - 2, str, huFontA,
                     gs.cfg.menuColor2[0], gs.cfg.menuColor2[1], gs.cfg.menuColor2[3],
                     menuAlpha, true, 0);
    }

    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
#if __JDOOM__ || __JDOOM64__
        M_WriteMenuText(menu, idx++, GET_TXT(TXT_WEAPON1 + PLRPROFILE.inventory.weaponOrder[i]));
#elif __JHERETIC__
        /**
         * \fixme We should allow different weapon preferences per player
         * class. However, since the only other class in jHeretic is the
         * chicken which has only 1 weapon anyway -we'll just show the
         * names of the player's weapons for now.
         */
        M_WriteMenuText(menu, idx++, GET_TXT(TXT_TXT_WPNSTAFF + PLRPROFILE.inventory.weaponOrder[i]));
#elif __JHEXEN__
        /**
         * \fixme We should allow different weapon preferences per player
         * class. Then we can show the real names here.
         */
        M_WriteMenuText(menu, idx++, weaponids[PLRPROFILE.inventory.weaponOrder[i]]);
#endif
    }

    M_WriteMenuText(menu, idx++, yesno[PLRPROFILE.inventory.weaponNextMode]);
    idx += 2;
    M_WriteMenuText(menu, idx++, autoswitch[PLRPROFILE.inventory.weaponAutoSwitch]);
    M_WriteMenuText(menu, idx++, yesno[PLRPROFILE.inventory.noWeaponAutoSwitchIfFiring]);
    M_WriteMenuText(menu, idx++, autoswitch[PLRPROFILE.inventory.ammoAutoSwitch]);
#if __JDOOM__ || __JDOOM64__
    M_WriteMenuText(menu, idx++, yesno[berserkAutoSwitch != 0]);
#endif
#if __JDOOM__
    M_WriteMenuText(menu, idx++, yesno[PLRPROFILE.statusbar.fixOuchFace != 0]);
#endif

    if(tmap == numColors)
        tmap = menuTime / 5 % numColors;

#if __JHEXEN__
    tclass = 1;
    if(plrClass == PCLASS_FIGHTER)
    {
        // Fighter's colors are a bit different.
        if(tmap == 0)
            tmap = 2;
        else if(tmap == 2)
            tmap = 0;
    }
#endif

    x = 162;
#if __JDOOM__ || __JDOOM64__
    y = menu->y + 70;
#elif __JHERETIC__
    y = menu->y + 80;
#else
    y = menu->y + 90;
#endif

    drawSpritePreview(x, y, 38, 52, sprites[plrClass], CurrentPlrFrame,
                      tclass, tmap, menuAlpha);
    if(plrColor == numColors)
        M_WriteText2(184, y - 52/2, "RANDOM", huFontA, 1, 1, 1, menuAlpha);
}

void M_EnterEditProfileMenu(int option, void* data)
{
    strncpy(plrNameEd.text, *(char **) Con_GetVariable("net-name")->ptr, 255);
    plrColor = PLRPROFILE.color;
#if __JHEXEN__
    plrClass = PLRPROFILE.pClass;
#endif
    M_SetupNextMenu(&EditProfileDef);
}

#if __JHEXEN__
void SCPlayerClass(int option, void* data)
{
    if(option == RIGHT_DIR)
    {
        if(plrClass < 2)
            plrClass++;
    }
    else if(plrClass > 0)
        plrClass--;
}
#endif

void SCPlayerColor(int option, void* data)
{
    if(option == RIGHT_DIR)
    {
#if __JHEXEN__
        if(plrColor < 8)
            plrColor++;
#else
        if(plrColor < 4)
            plrColor++;
#endif
    }
    else if(plrColor > 0)
        plrColor--;
}

void M_SaveProfile(int option, void* data)
{
    /// \todo save changes.
    M_SetupNextMenu(&ProfilesDef);
}

/**
 * The extended ticker.
 */
void MN_TickerEx(void)
{
    static int          frameTimer = 0;

    if(currentMenu == &EditProfileDef)
    {
        if(frameTimer++ >= 14)
        {
            frameTimer = 0;
            CurrentPlrFrame = M_Random() % 8;
        }
    }
}

int Ed_VisibleSlotChars(char* text, int (*widthFunc) (const char* text, dpatch_t* font))
{
#define SLOT_WIDTH          (200)

    char                cbuf[2] = { 0, 0 };
    int                 i, w;

    for(i = 0, w = 0; text[i]; ++i)
    {
        cbuf[0] = text[i];
        w += widthFunc(cbuf, huFontA);
        if(w > SLOT_WIDTH)
            break;
    }
    return i;

#undef SLOT_WIDTH
}

void Ed_MakeCursorVisible(void)
{
    char                buf[MAX_EDIT_LEN + 1];
    int                 i, len, vis;

    strcpy(buf, ActiveEdit->text);
    strupr(buf);
    strcat(buf, "_");           // The cursor.
    len = strlen(buf);
    for(i = 0; i < len; i++)
    {
        vis = Ed_VisibleSlotChars(buf + i, M_StringWidth);

        if(i + vis >= len)
        {
            ActiveEdit->firstVisible = i;
            break;
        }
    }
}

#if __JDOOM__ || __JDOOM64__
#define EDITFIELD_BOX_YOFFSET -1
#else
#define EDITFIELD_BOX_YOFFSET -1
#endif

void DrawEditField(menu_t* menu, int index, editfield_t* ef)
{
    int                 vis;
    char                buf[MAX_EDIT_LEN + 1], *text;
    int                 width = M_StringWidth("a", huFontA) * 27;

    strcpy(buf, ef->text);
    strupr(buf);
    if(ActiveEdit == ef && menuTime & 0x8)
        strcat(buf, "_");
    text = buf + ef->firstVisible;
    vis = Ed_VisibleSlotChars(text, M_StringWidth);
    text[vis] = 0;

    M_DrawSaveLoadBorder(menu->x - 8, menu->y + EDITFIELD_BOX_YOFFSET + (menu->itemHeight * index), width + 16);
    M_WriteText2(menu->x, menu->y + EDITFIELD_BOX_YOFFSET + (menu->itemHeight * index),
                 text, huFontA, 1, 1, 1, Hu_MenuAlpha());
}

void SCEditField(int efptr, void* data)
{
    editfield_t*        ef = data;

    // Activate this edit field.
    ActiveEdit = ef;
    strcpy(ef->oldtext, ef->text);
    Ed_MakeCursorVisible();
}

void M_WeaponOrder(int option, void* context)
{
    int         choice = option >> NUM_WEAPON_TYPES;
    int         temp;

    if(option & RIGHT_DIR)
    {
        if(choice < NUM_WEAPON_TYPES-1)
        {
            temp = PLRPROFILE.inventory.weaponOrder[choice+1];
            PLRPROFILE.inventory.weaponOrder[choice+1] = PLRPROFILE.inventory.weaponOrder[choice];
            PLRPROFILE.inventory.weaponOrder[choice] = temp;

            itemOn++;
        }
    }
    else
    {
        if(choice > 0)
        {
            temp = PLRPROFILE.inventory.weaponOrder[choice];
            PLRPROFILE.inventory.weaponOrder[choice] = PLRPROFILE.inventory.weaponOrder[choice-1];
            PLRPROFILE.inventory.weaponOrder[choice-1] = temp;

            itemOn--;
        }
    }
}

void M_WeaponAutoSwitch(int option, void* context)
{
    if(option == RIGHT_DIR)
    {
        if(PLRPROFILE.inventory.weaponAutoSwitch < 2)
            PLRPROFILE.inventory.weaponAutoSwitch++;
    }
    else if(PLRPROFILE.inventory.weaponAutoSwitch > 0)
        PLRPROFILE.inventory.weaponAutoSwitch--;
}

void M_AmmoAutoSwitch(int option, void* context)
{
    if(option == RIGHT_DIR)
    {
        if(PLRPROFILE.inventory.ammoAutoSwitch < 2)
            PLRPROFILE.inventory.ammoAutoSwitch++;
    }
    else if(PLRPROFILE.inventory.ammoAutoSwitch > 0)
        PLRPROFILE.inventory.ammoAutoSwitch--;
}
