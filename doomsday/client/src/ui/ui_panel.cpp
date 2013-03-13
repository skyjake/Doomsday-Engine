/** @file ui_panel.cpp
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

/**
 * Control Panel.
 *
 * Doomsday Control Panel (opened with the "panel" command).
 * During netgames the game is NOT PAUSED while the UI is active.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_ui.h"

#include "ui/displaymode.h"
#include "render/rend_font.h"

// MACROS ------------------------------------------------------------------

#define NUM_CP_BUTTONS  11
#define NUMITEMS(x)     (sizeof(x)/sizeof(uidata_listitem_t))
#define RES(x, y)       ((x) | ((y) << 16))
#define CPID_FRAME      (UIF_ID0 | UIF_ID1)
#define CPID_RES_X      UIF_ID0
#define CPID_RES_Y      UIF_ID1
#define CPID_SET_RES    UIF_ID2
#define CPID_RES_LIST   UIF_ID3
#define CPG_VIDEO       2
#define HELP_OFFSET     (UI_ScreenW(UI_WIDTH)*290/UI_WIDTH)

// TYPES -------------------------------------------------------------------

typedef struct cvarbutton_s {
    char    active;
    const char *cvarname;
    const char *yes;
    const char *no;
    int     mask;
} cvarbutton_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(OpenPanel);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void    CP_ClosePanel(ui_object_t *ob);
void    CP_ChooseGroup(ui_object_t *ob);
void    CP_DrawLogo(ui_object_t *ob);
void    CP_DrawBorder(ui_object_t *ob);
void    CP_CvarButton(ui_object_t *ob);
void    CP_CvarList(ui_object_t *ob);
void    CP_CvarEdit(ui_object_t *ob);
void    CP_CvarSlider(ui_object_t *ob);
int     CP_KeyGrabResponder(ui_object_t *ob, ddevent_t *ev);
void    CP_KeyGrabDrawer(ui_object_t *ob);
void    CP_QuickFOV(ui_object_t *ob);
void    CP_VideoModeInfo(ui_object_t *ob);
void    CP_ResolutionList(ui_object_t *ob);
void    CP_SetDefaultVidMode(ui_object_t *ob);
void    CP_SetVidMode(ui_object_t *ob);
void    CP_VidModeChanged(ui_object_t *ob);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

char    panel_buttons[NUM_CP_BUTTONS] = { true };   // The first is active.
char    panel_sv_password[100], panel_res_x[40], panel_res_y[40];
int     panel_fullscreen, panel_bpp;
int     panel_help_active = false;
int     panel_help_offset = 0;  // Zero means the help is completely hidden.
byte    panel_show_help = true; // cvar
byte    panel_show_tips = true; // cvar
ui_object_t *panel_help_source;
HelpId panel_help;

cvarbutton_t cvarbuttons[] = {
    {0, "con-var-silent"},
    {0, "con-dump"},
    {0, "con-fps"},
    {0, "con-text-shadow"},
    {0, "ui-panel-help"},
    {0, "ui-panel-tips"},
    {0, "input-mouse-filter"},
    {0, "input-joy"},
    {0, "net-nosleep"},
    {0, "net-dev"},
    {0, "net-queue-show"},
    {0, "sound-16bit"},
    {0, "sound-3d"},
    {0, "sound-info"},
    {0, "vid-vsync", "VSync on", "VSync off"},
    {0, "vid-fsaa", "Antialias", "No antialias"},
    {0, "rend-particle"},
    {0, "rend-camera-smooth"},
    {0, "rend-mobj-smooth-turn"},
    {0, "rend-sprite-precache"},
    {0, "rend-sprite-noz"},
    {0, "rend-sprite-blend"},
    {0, "rend-model"},
    {0, "rend-model-inter"},
    {0, "rend-model-precache"},
    {0, "rend-model-mirror-hud"},
    {0, "rend-model-shiny-multitex", "Shiny", "Shiny"},
    {0, "rend-tex"},
    {0, "rend-tex-filter-sprite", "Sprite", "Sprite"},
    {0, "rend-tex-filter-mag", "World", "World"},
    {0, "rend-tex-filter-ui", "UI", "UI"},
    {0, "rend-tex-filter-smart"},
    {0, "rend-tex-detail"},
    {0, "rend-tex-detail-multitex", "Detail", "Detail"},
    {0, "rend-tex-anim-smooth"},
    {0, "rend-light"},
    {0, "rend-light-decor"},
    {0, "rend-light-multitex", "Lights", "Lights"},
    {0, "rend-halo-realistic"},
    {0, "rend-glow-wall"},
    {0, "rend-info-tris"},
    {0, "rend-shadow"},
    {0, "rend-fakeradio"},
    {0, "input-mouse-x-flags", "Invert", "Invert", IDA_INVERT },
    {0, "input-mouse-x-flags", "Disable", "Disable", IDA_DISABLED },
    {0, "input-mouse-y-flags", "Invert", "Invert", IDA_INVERT },
    {0, "input-mouse-y-flags", "Disable", "Disable", IDA_DISABLED },
    {0, 0}
};

uidata_button_t btn_bpp = {&panel_bpp, "32", "16"};
uidata_button_t btn_fullscreen = {&panel_fullscreen, "Yes", "No"};

uidata_listitem_t lstit_con_completion[] = {
    {"List with values", 0},
    {"Cycle through", 1}
};
uidata_list_t lst_con_completion = {
    lstit_con_completion, NUMITEMS(lstit_con_completion), (void*) "con-completion"
};

uidata_listitem_t lstit_music_source[] = {
    {"MUS lumps", 0},
    {"External files", 1},
    {"CD", 2}
};
uidata_list_t lst_music_source = {
    lstit_music_source, NUMITEMS(lstit_music_source), (void*) "music-source"
};

uidata_listitem_t lstit_sound_rate[] = {
    {"11025 Hz (1x)", 11025},
    {"22050 Hz (2x)", 22050},
    {"44100 Hz (4x)", 44100}
};
uidata_list_t lst_sound_rate = {
    lstit_sound_rate, NUMITEMS(lstit_sound_rate), (void*) "sound-rate"
};

uidata_listitem_t lstit_smooth_move[] = {
    {"Disabled", 0},
    {"Models only", 1},
    {"Models and sprites", 2}
};
uidata_list_t lst_smooth_move = {
    lstit_smooth_move, NUMITEMS(lstit_smooth_move), (void*) "rend-mobj-smooth-move"
};

uidata_listitem_t lstit_sprite_align[] = {
    {"Camera", 0},
    {"View plane", 1},
    {"Camera (limited)", 2},
    {"View plane (limited)", 3}
};
uidata_list_t lst_sprite_align = {
    lstit_sprite_align, NUMITEMS(lstit_sprite_align), (void*) "rend-sprite-align"
};

uidata_listitem_t lstit_mipmap[] = {
    {"No filter, no mip", 0},
    {"Linear filter, no mip", 1},
    {"No filter, near mip", 2},
    {"Linear filter, near mip", 3},
    {"No filter, linear mip", 4},
    {"Linear filter, linear mip", 5}
};
uidata_list_t lst_mipmap = {
    lstit_mipmap, NUMITEMS(lstit_mipmap), (void*) "rend-tex-mipmap"
};

uidata_listitem_t lstit_blend[] = {
    {"Multiply", 0},
    {"Add", 1},
    {"Process wo/rendering", 2}
};
uidata_list_t lst_blend = {
    lstit_blend, NUMITEMS(lstit_blend), (void*) "rend-light-blend"
};

uidata_listitem_t* lstit_resolution; /* = {
    // 5:4
    {"1280 x 1024 (5:4 SXGA)", RES(1280, 1024)},
    {"2560 x 2048 (5:4 QSXGA)", RES(2560, 2048)},
    // 4:3
    {"320 x 240 (4:3 QVGA)", RES(320, 240)},
    {"640 x 480 (4:3 VGA)", RES(640, 480)},
    {"768 x 576 (4:3 PAL)", RES(768, 576)},
    {"800 x 600 (4:3 SVGA)", RES(800, 600)},
    {"1024 x 768 (4:3 XGA)", RES(1024, 768)},
    {"1152 x 864 (4:3)", RES(1152, 864)},
    {"1280 x 960 (4:3)", RES(1280, 960)},
    {"1400 x 1050 (4:3 SXGA+)", RES(1400, 1050)},
    {"1600 x 1200 (4:3 UXGA)", RES(1600, 1200)},
    {"2048 x 1536 (4:3 QXGA)", RES(2048, 1536)},
    // 3:2
    {"720 x 480 (3:2 NTSC)", RES(720, 480)},
    {"1152 x 768 (3:2)", RES(1152, 768)},
    {"1280 x 854 (3:2)", RES(1280, 854)},
    {"1440 x 960 (3:2)", RES(1440, 960)},
    // 16:10
    {"320 x 200 (16:10 CGA)", RES(320, 200)},
    {"800 x 500 (16:10)", RES(800, 500)},
    {"1024 x 640 (16:10)", RES(1024, 640)},
    {"1280 x 800 (16:10)", RES(1280, 800)},
    {"1440 x 900 (16:10 WXGA)", RES(1440, 900)},
    {"1600 x 1000 (16:10)", RES(1600, 1000)},
    {"1680 x 1050 (16:10 WSXGA+)", RES(1680, 1050)},
    {"1920 x 1200 (16:10 WUXGA)", RES(1920, 1200)},
    {"2560 x 1600 (16:10 WQXGA)", RES(2560, 1600)},
    // 16:9
    {"854 x 480 (16:9 WVGA)", RES(854, 480)},
    {"1280 x 720 (16:9 WXGA/HD720)", RES(1280, 720)},
    {"1920 x 1080 (16:9 HD1080)", RES(1920, 1080)}
};*/
uidata_list_t lst_resolution = {
    NULL, 0 // dynamically populated
    //   lstit_resolution, NUMITEMS(lstit_resolution)
};

uidata_slider_t sld_con_alpha = { 0, 1, 0, .01f, true, (void*) "con-alpha" };
uidata_slider_t sld_con_light = { 0, 1, 0, .01f, true, (void*) "con-light" };
uidata_slider_t sld_keywait1 = { 50, 1000, 0, 1, false, (void*) "input-key-delay1" };
uidata_slider_t sld_keywait2 = { 20, 1000, 0, 1, false, (void*) "input-key-delay2" };
uidata_slider_t sld_mouse_x_scale = { 0, .01f, 0, .00005f, true, (void*) "input-mouse-x-scale" };
uidata_slider_t sld_mouse_y_scale = { 0, .01f, 0, .00005f, true, (void*) "input-mouse-y-scale" };
uidata_slider_t sld_client_pos_interval =
    { 0, 70, 0, 1, false, (void*) "client-pos-interval" };
uidata_slider_t sld_server_frame_interval =
    { 0, 35, 0, 1, false, (void*) "server-frame-interval" };
uidata_slider_t sld_sound_volume = { 0, 255, 0, 1, false, (void*) "sound-volume" };
uidata_slider_t sld_music_volume = { 0, 255, 0, 1, false, (void*) "music-volume" };
uidata_slider_t sld_reverb_volume =
    { 0, 1, 0, .01f, true, (void*) "sound-reverb-volume" };
uidata_slider_t sld_particle_max =
    { 0, 10000, 0, 10, false, (void*) "rend-particle-max", (char*) "Unlimited" };
uidata_slider_t sld_particle_rate =
    { .1f, 10, 0, .01f, true, (void*) "rend-particle-rate" };
uidata_slider_t sld_particle_diffuse =
    { 0, 20, 0, .01f, true, (void*) "rend-particle-diffuse" };
uidata_slider_t sld_particle_visnear =
    { 0, 1000, 0, 1, false, (void*) "rend-particle-visible-near", (char*)"Disabled" };
uidata_slider_t sld_model_far =
    { 0, 3000, 0, 1, false, (void*) "rend-model-distance", (char*)"Unlimited" };
uidata_slider_t sld_model_lights = { 0, 10, 0, 1, false, (void*) "rend-model-lights" };
uidata_slider_t sld_model_lod =
    { 0, 1000, 0, 1, true, (void*) "rend-model-lod", (char*)"No LOD" };
uidata_slider_t sld_detail_scale =
    { .1f, 32, 0, .01f, true, (void*) "rend-tex-detail-scale" };
uidata_slider_t sld_detail_strength =
    { 0, 2, 0, .01f, true, (void*) "rend-tex-detail-strength" };
uidata_slider_t sld_detail_far =
    { 1, 1000, 0, 1, true, (void*) "rend-tex-detail-far" };
uidata_slider_t sld_tex_quality = { 0, 8, 0, 1, false, (void*) "rend-tex-quality" };
uidata_slider_t sld_tex_aniso = { -1, 4, 0, 1, false, (void*) "rend-tex-filter-anisotropic", (char*)"Best Available"};
uidata_slider_t sld_light_bright =
    { 0, 1, 0, .01f, true, (void*) "rend-light-bright" };
uidata_slider_t sld_light_scale =
    { .1f, 10, 0, .01f, true, (void*) "rend-light-radius-scale" };
uidata_slider_t sld_light_radmax =
    { 64, 512, 0, 1, false, (void*) "rend-light-radius-max" };
uidata_slider_t sld_light_max =
    { 0, 2000, 0, 1, false, (void*) "rend-light-num", (char*)"Unlimited" };
uidata_slider_t sld_light_glow_strength =
    { 0, 2, 0, .01f, true, (void*) "rend-glow" };
uidata_slider_t sld_light_fog_bright =
    { 0, 1, 0, .01f, true, (void*) "rend-glow-fog-bright" };
uidata_slider_t sld_light_ambient =
    { 0, 255, 0, 1, false, (void*) "rend-light-ambient" };
uidata_slider_t sld_light_compression =
    { -1, 1, 0, 0.1f, true, (void*) "rend-light-compression" };
uidata_slider_t sld_glow_height = { 0, 1024, 0, 1, false, (void*) "rend-glow-height" };
uidata_slider_t sld_glow_scale = { .1f, 10, 0, 0.1f, true, (void*) "rend-glow-scale" };
uidata_slider_t sld_fov = { 1, 179, 0, .01f, true, (void*) "rend-camera-fov" };
uidata_slider_t sld_sky_distance =
    { 1, 10000, 0, 10, true, (void*) "rend-sky-distance" };
uidata_slider_t sld_shadow_dark =
    { 0, 2, 0, .01f, true, (void*) "rend-shadow-darkness" };
uidata_slider_t sld_shadow_far = { 0, 3000, 0, 1, false, (void*) "rend-shadow-far" };
uidata_slider_t sld_shadow_radmax =
    { 0, 128, 0, 1, false, (void*) "rend-shadow-radius-max" };
uidata_slider_t sld_fakeradio_dark =
    { 0, 2, 0, .01f, true, (void*) "rend-fakeradio-darkness" };
uidata_slider_t sld_vid_gamma = { .1f, 3, 0, .01f, true, (void*) "vid-gamma" };
uidata_slider_t sld_vid_contrast = { .1f, 3, 0, .01f, true, (void*) "vid-contrast" };
uidata_slider_t sld_vid_bright = { -.5f, .5f, 0, .01f, true, (void*) "vid-bright" };
uidata_slider_t sld_halo = { 0, 5, 0, 1, false, (void*) "rend-halo", (char*) "None" };
uidata_slider_t sld_halo_bright = { 0, 100, 0, 1, false, (void*) "rend-halo-bright" };
uidata_slider_t sld_halo_occlusion =
    { 1, 256, 0, 1, false, (void*) "rend-halo-occlusion" };
uidata_slider_t sld_halo_size = { 0, 100, 0, 1, false, (void*) "rend-halo-size" };
uidata_slider_t sld_halo_seclimit =
    { 0, 10, 0, .01f, true, (void*) "rend-halo-secondary-limit" };
uidata_slider_t sld_halo_dimfar =
    { 0, 200, 0, .01f, true, (void*) "rend-halo-dim-far" };
uidata_slider_t sld_halo_dimnear =
    { 0, 200, 0, .01f, true, (void*) "rend-halo-dim-near" };
uidata_slider_t sld_halo_zmagdiv =
    { 1, 200, 0, .01f, true, (void*) "rend-halo-zmag-div" };
uidata_slider_t sld_halo_radmin =
    { 1, 80, 0, .01f, true, (void*) "rend-halo-radius-min" };
uidata_slider_t sld_sprite_lights = { 0, 10, 0, 1, false, (void*) "rend-sprite-lights" };

uidata_edit_t ed_server_password =
    { panel_sv_password, 100, (void*) "server-password" };
uidata_edit_t ed_res_x = { panel_res_x, 40 };
uidata_edit_t ed_res_y = { panel_res_y, 40 };

ui_page_t page_panel;

/* *INDENT-OFF* */
ui_object_t ob_panel[] =
{
    { UI_BUTTON2,   1,  UIF_LEFT_ALIGN, 10, 210, 240, 60,   "Video",    UIButton_Drawer, UIButton_Responder, 0,             CP_ChooseGroup, &panel_buttons[0] },
    { UI_BUTTON2,   1,  UIF_LEFT_ALIGN, 10, 275, 240, 60,   "Audio",    UIButton_Drawer, UIButton_Responder, 0,             CP_ChooseGroup, &panel_buttons[1] },
    { UI_BUTTON2,   1,  UIF_LEFT_ALIGN, 10, 340, 240, 60,   "Input",    UIButton_Drawer, UIButton_Responder, 0,             CP_ChooseGroup, &panel_buttons[2] },
    { UI_BUTTON2,   1,  UIF_LEFT_ALIGN, 10, 405, 240, 60,   "Graphics", UIButton_Drawer, UIButton_Responder, 0,             CP_ChooseGroup, &panel_buttons[3] },
    { UI_BUTTON2,   1,  UIF_LEFT_ALIGN, 40, 467, 210, 60,   "Lights",   UIButton_Drawer, UIButton_Responder, 0,             CP_ChooseGroup, &panel_buttons[4] },
    { UI_BUTTON2,   1,  UIF_LEFT_ALIGN, 40, 529, 210, 60,   "Halos",    UIButton_Drawer, UIButton_Responder, 0,             CP_ChooseGroup, &panel_buttons[5] },
    { UI_BUTTON2,   1,  UIF_LEFT_ALIGN, 40, 591, 210, 60,   "Textures", UIButton_Drawer, UIButton_Responder, 0,             CP_ChooseGroup, &panel_buttons[6] },
    { UI_BUTTON2,   1,  UIF_LEFT_ALIGN, 40, 653, 210, 60,   "Objects",  UIButton_Drawer, UIButton_Responder, 0,             CP_ChooseGroup, &panel_buttons[7] },
    { UI_BUTTON2,   1,  UIF_LEFT_ALIGN, 40, 715, 210, 60,   "Particles", UIButton_Drawer, UIButton_Responder, 0,            CP_ChooseGroup, &panel_buttons[8] },
    { UI_BUTTON2,   1,  UIF_LEFT_ALIGN, 10, 780, 240, 60,   "Network",  UIButton_Drawer, UIButton_Responder, 0,             CP_ChooseGroup, &panel_buttons[9] },
    { UI_BUTTON2,   1,  UIF_LEFT_ALIGN, 10, 845, 240, 60,   "Console",  UIButton_Drawer, UIButton_Responder, 0,             CP_ChooseGroup, &panel_buttons[10] },
    { UI_BUTTON,    0,  UIF_NEVER_FADE, 10, 940, 240, 60,   "Close Panel (Esc)", UIButton_Drawer, UIButton_Responder, 0,    CP_ClosePanel },
    { UI_BOX,       0,  0,              8, -20, 250, 250,   "",         CP_DrawLogo },
    { UI_BOX,       0,  CPID_FRAME,     280, 55, 720, 945,  "",         CP_DrawBorder },

    { UI_META,      2 },
    { UI_TEXT,      0,  0,              280, 0, 0, 50,      "Video Options", UIText_BrightDrawer },
    { UI_TEXT,      0,  0,              300, 70, 0, 55,     "Quality",  UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 70, 145, 55,   "vid-fsaa", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_BUTTON2,   0,  0,              830, 70, 145, 55,   "vid-vsync", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 130, 0, 55,    "Gamma correction", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 130, 300, 55,   "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_vid_gamma },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 190, 0, 55,    "Display contrast", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 190, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_vid_contrast },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 250, 0, 55,    "Display brightness", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 250, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_vid_bright },
    { UI_TEXT,      0,  0,              300, 310, 0, 55,    "Current video mode", UIText_Drawer },
    { UI_BOX,       0,  0,              680, 310, 0, 60,    "current",  CP_VideoModeInfo },
    { UI_TEXT,      0,  0,              300, 370, 0, 55,    "Resolution", UIText_Drawer },
    { UI_LIST,      0,  CPID_RES_LIST,  680, 370, 300, 175, "",         UIList_Drawer, UIList_Responder, UIList_Ticker, CP_ResolutionList, &lst_resolution },
    { UI_TEXT,      0,  0,              300, 550, 0, 55,    "Custom resolution", UIText_Drawer },
    { UI_EDIT,      0,  CPID_RES_X,     680, 550, 130, 55,  "",         UIEdit_Drawer, UIEdit_Responder, 0, CP_VidModeChanged, &ed_res_x },
    { UI_TEXT,      0,  0,              826, 550, 0, 55,    "x", UIText_Drawer },
    { UI_EDIT,      0,  CPID_RES_Y,     850, 550, 130, 55,  "",         UIEdit_Drawer, UIEdit_Responder, 0, CP_VidModeChanged, &ed_res_y },
    { UI_TEXT,      0,  0,              300, 610, 0, 55,    "Fullscreen", UIText_Drawer },
    { UI_BUTTON2EX, 0,  0,              680, 610, 130, 55,  "",    UIButton_Drawer, UIButton_Responder, 0, CP_VidModeChanged, &btn_fullscreen },
    { UI_TEXT,      0,  0,              300, 670, 0, 55,    "Color depth", UIText_Drawer },
    { UI_BUTTON2EX, 0,  0,              680, 670, 130, 55,  "",         UIButton_Drawer, UIButton_Responder, 0, CP_VidModeChanged, &btn_bpp },
    { UI_TEXT,      0,  0,              300, 730, 0, 55,    "Default video mode", UIText_Drawer },
    { UI_BOX,       0,  0,              680, 730, 0, 55,    "default",  CP_VideoModeInfo },
    { UI_BUTTON,    0,  0,              680, 790, 170, 60,  "Set Default", UIButton_Drawer, UIButton_Responder, 0, CP_SetDefaultVidMode },
    { UI_TEXT,      0,  0,              300, 910, 0, 55,    "Change to", UIText_Drawer },
    { UI_BUTTON,    0,  CPID_SET_RES,   680, 910, 300, 60,  "",         UIButton_Drawer, UIButton_Responder, 0, CP_SetVidMode },

    { UI_META,      3 },
    { UI_TEXT,      0,  0,              280, 0, 0, 50,      "Audio Options", UIText_BrightDrawer },
    { UI_TEXT,      0,  0,              300, 70, 0, 55,     "Sound volume", UIText_Drawer },
    { UI_SLIDER,    0,  0,              680, 70, 300, 55,   "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_sound_volume },
    { UI_TEXT,      0,  0,              300, 130, 0, 55,    "Music volume", UIText_Drawer },
    { UI_SLIDER,    0,  0,              680, 130, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_music_volume },
    { UI_TEXT,      0,  0,              300, 190, 0, 55,    "Preferred music source", UIText_Drawer },
    { UI_LIST,      0,  0,              680, 190, 300, 150, "",         UIList_Drawer, UIList_Responder, UIList_Ticker, CP_CvarList, &lst_music_source },
    { UI_TEXT,      0,  0,              300, 345, 0, 55,    "16-bit sound effects", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 345, 70, 55,   "sound-16bit", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  0,              300, 405, 0, 55,    "Sound effects sample rate", UIText_Drawer },
    { UI_LIST,      0,  0,              680, 405, 300, 150, "",         UIList_Drawer, UIList_Responder, UIList_Ticker, CP_CvarList, &lst_sound_rate },
    { UI_TEXT,      0,  0,              300, 560, 0, 55,    "3D sounds", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 560, 70, 55,   "sound-3d", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  0,              300, 620, 0, 55,    "General reverb strength", UIText_Drawer },
    { UI_SLIDER,    0,  0,              680, 620, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_reverb_volume },
    { UI_TEXT,      0,  0,              300, 680, 0, 55,    "Show status of channels", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 680, 70, 55,   "sound-info", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },

    { UI_META,      4 },
    { UI_TEXT,      0,  0,              280, 0, 0, 50,      "Input Options", UIText_BrightDrawer },
    { UI_TEXT,      0,  0,              300, 70, 0, 55,     "Mouse X sensitivity", UIText_Drawer },
    { UI_SLIDER,    0,  0,              680, 70, 300, 55,   "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_mouse_x_scale },
    { UI_TEXT,      0,  0,              300, 130, 0, 55,    "Mouse X mode", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 130, 80, 55,   "input-mouse-x-flags", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton, 0, IDA_INVERT },
    { UI_BUTTON2,   0,  0,              765, 130, 80, 55,   "input-mouse-x-flags", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton, 0, IDA_DISABLED },
    { UI_TEXT,      0,  0,              300, 190, 0, 55,    "Mouse Y sensitivity", UIText_Drawer },
    { UI_SLIDER,    0,  0,              680, 190, 300, 55,   "",        UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_mouse_y_scale },
    { UI_TEXT,      0,  0,              300, 250, 0, 55,    "Mouse Y mode", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 250, 80, 55,   "input-mouse-y-flags", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton, 0, IDA_INVERT },
    { UI_BUTTON2,   0,  0,              765, 250, 80, 55,   "input-mouse-y-flags", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton, 0, IDA_DISABLED },
    { UI_META,      4,  0,              0, 60 },
    { UI_TEXT,      0,  0,              300, 250, 0, 55,    "Enable joystick", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 250, 70, 55,   "input-joy", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  0,              300, 430, 0, 55,    "Key repeat delay (ms)", UIText_Drawer },
    { UI_SLIDER,    0,  0,              680, 430, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_keywait1 },
    { UI_TEXT,      0,  0,              300, 490, 0, 55,    "Key repeat rate (ms)", UIText_Drawer },
    { UI_SLIDER,    0,  0,              680, 490, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_keywait2 },

    { UI_META,      5 },
    { UI_TEXT,      0,  0,              280, 0, 0, 50,      "Graphics Options", UIText_BrightDrawer },
    { UI_META,      5,  0,              0, -60 },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 130, 0, 55,    "Field Of View angle", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 130, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_fov },
    { UI_BUTTON,    0,  UIF_FADE_AWAY,  680, 190, 70, 60,   "90",       UIButton_Drawer, UIButton_Responder, 0, CP_QuickFOV },
    { UI_BUTTON,    0,  UIF_FADE_AWAY,  755, 190, 70, 60,   "95",       UIButton_Drawer, UIButton_Responder, 0, CP_QuickFOV },
    { UI_BUTTON,    0,  UIF_FADE_AWAY,  830, 190, 70, 60,   "100",      UIButton_Drawer, UIButton_Responder, 0, CP_QuickFOV },
    { UI_BUTTON,    0,  UIF_FADE_AWAY,  905, 190, 70, 60,   "110",      UIButton_Drawer, UIButton_Responder, 0, CP_QuickFOV },
    { UI_TEXT,      0,  0,              300, 255, 0, 55,    "Mirror player weapon models", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 255, 70, 55,   "rend-model-mirror-hud", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_META,      5,  0,              0, 60 },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 255, 0, 55,    "Sky sphere radius", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 255, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_sky_distance },
    { UI_TEXT,      0,  0,              300, 315, 0, 55,    "Objects cast shadows", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 315, 70, 55,   "rend-shadow", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 375, 0, 55,    "Shadow darkness factor", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 375, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_shadow_dark },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 435, 0, 55,    "Shadow visible distance", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 435, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_shadow_far },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 495, 0, 55,    "Maximum shadow radius", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 495, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_shadow_radmax },
    { UI_TEXT,      0,  0,              300, 555, 0, 55,    "Simulate radiosity", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 555, 70, 55,   "rend-fakeradio", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 615, 0, 55,    "Radiosity shadow darkness", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 615, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_fakeradio_dark },

    { UI_META,      6 },
    { UI_TEXT,      0,  0,              280, 0, 0, 50,      "Graphics Options: Lights", UIText_BrightDrawer },
    { UI_TEXT,      0,  0,              300, 70, 0, 55,     "Enable dynamic lights", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 70, 70, 55,    "rend-light", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  0,              300, 130, 0, 55,    "Blending mode", UIText_Drawer },
    { UI_LIST,      0,  0,              680, 130, 300, 115, "",         UIList_Drawer, UIList_Responder, UIList_Ticker, CP_CvarList, &lst_blend },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 250, 0, 55,    "Dynamic light brightness", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 250, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_light_bright },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 310, 0, 55,    "Dynamic light radius factor", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 310, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_light_scale },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 370, 0, 55,    "Maximum dynamic light radius", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 370, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_light_radmax },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 430, 0, 55,    "Maximum number of dynamic lights", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 430, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_light_max },
    { UI_META,      6,  0,              0, -120 },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 610, 0, 55,    "Ambient light level", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 610, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_light_ambient },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 670, 0, 55,    "Light range compression", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 670, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_light_compression },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 730, 0, 55,    "Material glow strength", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 730, 300, 55,  "rend-glow", UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_light_glow_strength },
    { UI_TEXT,      0,  0,              300, 790, 0, 55,    "Floor/ceiling glow on walls", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 790, 70, 55,   "rend-glow-wall", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 850, 0, 55,    "Maximum floor/ceiling glow height", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 850, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_glow_height },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 910, 0, 55,    "Floor/ceiling glow height factor", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 910, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_glow_scale },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 970, 0, 55,    "Glow brightness in fog", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 970, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_light_fog_bright },
    { UI_TEXT,      0,  0,              300, 1030, 0, 55,    "Enable decorations", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 1030, 70, 55,   "rend-light-decor", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },

    { UI_META,      7 },
    { UI_TEXT,      0,  0,              280, 0, 0, 50,      "Graphics Options: Halos", UIText_BrightDrawer },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 70, 0, 55,     "Number of flares per halo", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 70, 300, 55,   "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_halo },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 130, 0, 55,    "Use realistic halos", UIText_Drawer },
    { UI_BUTTON2,   0,  UIF_FADE_AWAY,  680, 130, 70, 55,   "rend-halo-realistic", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_META,      7,  0,              0,   60 },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 130, 0, 55,    "Halo brightness", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 130, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_halo_bright },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 190, 0, 55,    "Halo size factor", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 190, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_halo_size },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 250, 0, 55,    "Occlusion fade speed", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 250, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_halo_occlusion },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 310, 0, 55,    "Minimum halo radius", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 310, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_halo_radmin },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 370, 0, 55,    "Flare visibility limitation", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 370, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_halo_seclimit },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 430, 0, 55,    "Halo fading start", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 430, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_halo_dimnear },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 490, 0, 55,    "Halo fading end", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 490, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_halo_dimfar },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 550, 0, 55,    "Z magnification divisor", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 550, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_halo_zmagdiv },

    { UI_META,      8 },
    { UI_TEXT,      0,  0,              280, 0, 0, 50,      "Graphics Options: Textures", UIText_BrightDrawer },
    { UI_TEXT,      0,  0,              300, 70, 0, 55,     "Enable textures", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 70, 70, 55,    "rend-tex", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  0,              300, 130, 0, 55,    "Multitexturing", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 130, 95, 55,   "rend-tex-detail-multitex", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_BUTTON2,   0,  0,              780, 130, 95, 55,   "rend-light-multitex", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_BUTTON2,   0,  0,              880, 130, 95, 55,   "rend-model-shiny-multitex", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  0,              300, 190, 0, 55,    "Smooth texture animation", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 190, 70, 55,   "rend-tex-anim-smooth", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_META,      8,  0,              0, 120 },
    { UI_TEXT,      0,  0,              300, 130, 0, 55,    "Mipmapping filter", UIText_Drawer },
    { UI_LIST,      0,  0,              680, 130, 300, 175, "",         UIList_Drawer, UIList_Responder, UIList_Ticker, CP_CvarList, &lst_mipmap },
    { UI_TEXT,      0,  0,              300, 310, 0, 55,    "Texture quality", UIText_Drawer },
    { UI_SLIDER,    0,  0,              680, 310, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_tex_quality },
    { UI_TEXT,      0,  0,              300, 370, 0, 55,    "Smart texture filtering", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 370, 70, 55,   "rend-tex-filter-smart", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_META,      8,  0,              0,   180 },
    { UI_TEXT,      0,  0,              300, 370, 0, 55,    "Bilinear filtering", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 370, 95, 55,   "rend-tex-filter-sprite", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_BUTTON2,   0,  0,              780, 370, 95, 55,   "rend-tex-filter-mag", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_BUTTON2,   0,  0,              880, 370, 95, 55,   "rend-tex-filter-ui", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 430, 0, 55,    "Anisotropic filtering", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 430, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_tex_aniso },
    { UI_TEXT,      0,  0,              300, 490, 0, 55,    "Enable detail textures", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 490, 70, 55,   "rend-tex-detail", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 550, 0, 55,    "Detail texture scaling factor", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 550, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_detail_scale },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 610, 0, 55,    "Detail texture contrast", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 610, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_detail_strength },

    { UI_META,      9 },
    { UI_TEXT,      0,  0,              280, 0, 0, 50,      "Graphics Options: Objects", UIText_BrightDrawer },
    { UI_TEXT,      0,  0,              300, 70, 0, 55,     "Enable 3D models", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 70, 70, 55,    "rend-model", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  0,              300, 130, 0, 55,    "Interpolate between frames", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 130, 70, 55,   "rend-model-inter", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 190, 0, 55,    "3D model visibility limit", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 190, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_model_far },
    { UI_TEXT,      0,  0,              300, 250, 0, 55,    "Precache 3D models", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 250, 70, 55,   "rend-model-precache", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 310, 0, 55,    "Max dynamic lights on 3D models", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 310, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_model_lights },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 370, 0, 55,    "LOD level zero distance", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 370, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_model_lod },
    { UI_TEXT,      0,  0,              300, 430, 0, 55,    "Precache sprites (slow)", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 430, 70, 55,   "rend-sprite-precache", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  0,              300, 490, 0, 55,    "Disable Z-writes for sprites", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 490, 70, 55,   "rend-sprite-noz", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  0,              300, 550, 0, 55,    "Additive blending for sprites", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 550, 70, 55,   "rend-sprite-blend", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 610, 0, 55,    "Max dynamic lights on sprites", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 610, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_sprite_lights },
    { UI_TEXT,      0,  0,              300, 670, 0, 55,    "Align sprites to...", UIText_Drawer },
    { UI_LIST,      0,  0,              680, 670, 300, 115, "",         UIList_Drawer, UIList_Responder, UIList_Ticker, CP_CvarList, &lst_sprite_align },
    { UI_TEXT,      0,  0,              300, 790, 0, 55,    "Smooth actor rotation", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 790, 70, 55,   "rend-mobj-smooth-turn", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  0,              300, 850, 0, 55,    "Smooth actor movement", UIText_Drawer },
    { UI_LIST,      0,  0,              680, 850, 300, 115, "",         UIList_Drawer, UIList_Responder, UIList_Ticker, CP_CvarList, &lst_smooth_move },

    { UI_META,      10 },
    { UI_TEXT,      0,  0,              280, 0, 0, 50,      "Graphics Options: Particles", UIText_BrightDrawer },
    { UI_TEXT,      0,  0,              300, 70, 0, 55,     "Enable particle effects", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 70, 70, 55,    "rend-particle", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 130, 0, 55,    "Maximum number of particles", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 130, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_particle_max },
    { UI_TEXT,      0,  0,              300, 190, 0, 55,    "Spawn rate factor", UIText_Drawer },
    { UI_SLIDER,    0,  0,              680, 190, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_particle_rate },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 250, 0, 55,    "Near diffusion factor", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 250, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_particle_diffuse },
    { UI_TEXT,      0,  UIF_FADE_AWAY,  300, 310, 0, 55,    "Near clip distance", UIText_Drawer },
    { UI_SLIDER,    0,  UIF_FADE_AWAY,  680, 310, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_particle_visnear },

    { UI_META,      11 },
    { UI_TEXT,      0,  0,              280, 0, 0, 50,      "Network Options", UIText_BrightDrawer },
    { UI_TEXT,      0,  0,              300, 70, 0, 55,     "Continuous screen refresh", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 70, 70, 55,    "net-nosleep", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  0,              300, 130, 0, 55,    "Show development info", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 130, 70, 55,   "net-dev", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  0,              300, 190, 0, 55,    "Server login password", UIText_Drawer },
    { UI_EDIT,      0,  0,              680, 190, 300, 55,  "",         UIEdit_Drawer, UIEdit_Responder, 0, CP_CvarEdit, &ed_server_password },
    { UI_TEXT,      0,  0,              300, 250, 0, 55,    "Cl-to-sv pos transmit tics", UIText_Drawer },
    { UI_SLIDER,    0,  0,              680, 250, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_client_pos_interval },
    { UI_TEXT,      0,  0,              300, 310, 0, 55,    "Frame interval tics", UIText_Drawer },
    { UI_SLIDER,    0,  0,              680, 310, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_server_frame_interval },

    { UI_META,      12 },
    { UI_TEXT,      0,  0,              280, 0, 0, 50,      "Console Options", UIText_BrightDrawer },
    { UI_TEXT,      0,  0,              300, 70, 0, 55,     "Display FPS counter", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 70, 70, 55,    "con-fps",  UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_META,      12, 0,              0, 60 },
    { UI_TEXT,      0,  0,              300, 130, 0, 55,    "Display Control Panel help window", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 130, 70, 55,   "ui-panel-help", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  0,              300, 190, 0, 55,    "Display help indicators", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 190, 70, 55,   "ui-panel-tips", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_META,      12, 0,              0, 180 },
    { UI_TEXT,      0,  0,              300, 130, 0, 55,    "Silent console variables", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 130, 70, 55,   "con-var-silent", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  0,              300, 190, 0, 55,    "Dump messages to Doomsday.out", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 190, 70, 55,   "con-dump", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  0,              300, 310, 0, 55,    "Command completion with Tab", UIText_Drawer },
    { UI_LIST,      0,  0,              680, 310, 300, 120, "",         UIList_Drawer, UIList_Responder, UIList_Ticker, CP_CvarList, &lst_con_completion },
    { UI_TEXT,      0,  0,              300, 435, 0, 55,    "Background opacity", UIText_Drawer },
    { UI_SLIDER,    0,  0,              680, 435, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_con_alpha },
    { UI_TEXT,      0,  0,              300, 495, 0, 55,    "Background light", UIText_Drawer },
    { UI_SLIDER,    0,  0,              680, 495, 300, 55,  "",         UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_con_light },
    { UI_TEXT,      0,  0,              300, 555, 0, 55,    "Console text has shadows", UIText_Drawer },
    { UI_BUTTON2,   0,  0,              680, 555, 70, 55,   "con-text-shadow", UIButton_Drawer, UIButton_Responder, 0, CP_CvarButton },
    { UI_TEXT,      0,  0,              300, 615, 0, 55,    "Activation key", UIText_Drawer },
    { UI_FOCUSBOX,  0,  0,              680, 615, 70, 55,   "con-key-activate", CP_KeyGrabDrawer, CP_KeyGrabResponder },
    { UI_TEXT,      0,  0,              680, 670, 0, 40,    "Click the box, press a key.", UIText_Drawer },

    { UI_NONE }
};
/* *INDENT-ON* */

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void CP_Register(void)
{
    // Cvars
    C_VAR_BYTE("ui-panel-help", &panel_show_help, 0, 0, 1);
    C_VAR_BYTE("ui-panel-tips", &panel_show_tips, 0, 0, 1);

    // Ccmds
    C_CMD_FLAGS("panel", NULL, OpenPanel, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
}

void CP_ClosePanel(ui_object_t* ob)
{
    UI_End();
}

void CP_ChooseGroup(ui_object_t* ob)
{
    int i;

    memset(panel_buttons, 0, sizeof(panel_buttons));
    UI_FlagGroup(ob_panel, 1, UIF_ACTIVE, UIFG_CLEAR);
    *(char *) ob->data = true;
    ob->flags |= UIF_ACTIVE;
    // Hide/show the option controls.
    for(i = 0; i < NUM_CP_BUTTONS; ++i)
        UI_FlagGroup(ob_panel, 2 + i, UIF_HIDDEN, !panel_buttons[i]);
}

void CP_DrawLogo(ui_object_t* ob)
{
    UI_DrawLogo(&ob->geometry.origin, &ob->geometry.size);
}

void CP_DrawBorder(ui_object_t* ob)
{
    int b = UI_BORDER;
    ui_object_t* it;
    HelpId help_ptr;
    Point2Raw origin;
    Size2Raw size;
    boolean shown;

    UIFrame_Drawer(ob);

    // Draw help window visual cues.
    if(panel_show_tips)
    {
        GL_BlendMode(BM_ADD);
        glEnable(GL_TEXTURE_2D);
        for(it = ob_panel; it->type; it++)
        {
            if(it->flags & UIF_HIDDEN || it->group < 2 || it->type != UI_TEXT)
                continue;

            // Try to find help for this.
            help_ptr = DH_Find(it->text);
            if(help_ptr)
            {
                shown = (panel_help == help_ptr && panel_help_active);

                origin.x = ob->geometry.origin.x + b;
                origin.y = it->geometry.origin.y + it->geometry.size.height / 2 - UI_FontHeight() / 2;
                size.width  = 2 * UI_FontHeight();
                size.height = UI_FontHeight();
                UI_HorizGradient(&origin, &size, UI_Color(UIC_BRD_HI), 0, shown ? .8f : .2f, 0);
            }
        }
        glDisable(GL_TEXTURE_2D);
        GL_BlendMode(BM_NORMAL);
    }
}

void CP_CvarButton(ui_object_t *ob)
{
    cvarbutton_t *cb = (cvarbutton_t *) ob->data;
    cvar_t     *var = Con_FindVariable(cb->cvarname);
    int         value;

    strcpy(ob->text, cb->active? cb->yes : cb->no);

    if(!var)
        return;

    if(cb->mask)
    {
        value = Con_GetInteger(cb->cvarname);
        if(cb->active)
        {
            value |= cb->mask;
        }
        else
        {
            value &= ~cb->mask;
        }
    }
    else
    {
        value = cb->active;
    }

    Con_SetInteger2(cb->cvarname, value, SVF_WRITE_OVERRIDE);
}

void CP_CvarList(ui_object_t *ob)
{
    uidata_list_t *list = (uidata_list_t *) ob->data;
    cvar_t* var = Con_FindVariable((char const *) list->data);
    int value = ((uidata_listitem_t *) list->items)[list->selection].data;

    if(list->selection < 0) return;
    if(!var) return;

    CVar_SetInteger2(var, value, SVF_WRITE_OVERRIDE);
}

void CP_CvarEdit(ui_object_t *ob)
{
    uidata_edit_t* ed = (uidata_edit_t *) ob->data;
    cvar_t* var = Con_FindVariable((char const *) ed->data);
    if(NULL == var) return;
    CVar_SetString2(var, ed->ptr, SVF_WRITE_OVERRIDE);
}

void CP_CvarSlider(ui_object_t *ob)
{
    uidata_slider_t *slid = (uidata_slider_t *) ob->data;
    cvar_t* var = Con_FindVariable((char const *) slid->data);
    float value = slid->value;

    if(NULL == var) return;

    if(!slid->floatmode)
    {
        value += (slid->value < 0? -.5f : .5f);
    }

    switch(var->type)
    {
    case CVT_FLOAT:
        if(slid->step >= .01f)
        {
            CVar_SetFloat2(var, (int) (100 * value) / 100.0f, SVF_WRITE_OVERRIDE);
        }
        else
        {
            CVar_SetFloat2(var, value, SVF_WRITE_OVERRIDE);
        }
        break;
    case CVT_INT:
        CVar_SetInteger2(var, (int) value, SVF_WRITE_OVERRIDE);
        break;
    case CVT_BYTE:
        CVar_SetInteger2(var, (byte) value, SVF_WRITE_OVERRIDE);
        break;
    default:
        break;
    }
}

int CP_KeyGrabResponder(ui_object_t *ob, ddevent_t *ev)
{
    if(IS_TOGGLE_DOWN(ev) &&
       ((ev->device == IDEV_MOUSE && UI_MouseInside(ob)) ||
        (ev->device == IDEV_KEYBOARD && IS_ACTKEY(ev->toggle.id))))
    {
        // We want the focus.
        return true;
    }
    // Only does something when has the focus.
    if(!(ob->flags & UIF_FOCUS))
        return false;

    if(IS_KEY_DOWN(ev))
    {
        Con_SetInteger2(ob->text, ev->toggle.id, SVF_WRITE_OVERRIDE);
        // All keydown events are eaten.
        // Note that the UI responder eats all Tabs!
        return true;
    }
    return false;
}

void CP_KeyGrabDrawer(ui_object_t* ob)
{
    boolean sel = (ob->flags & UIF_FOCUS) != 0;
    float alpha = (ob->flags & UIF_DISABLED ? .2f : 1);
    char buf[80];
    byte key = Con_GetByte(ob->text);
    const char* name;
    Point2Raw labelOrigin;

    glEnable(GL_TEXTURE_2D);
    UI_GradientEx(&ob->geometry.origin, &ob->geometry.size, UI_BORDER,
        UI_Color(UIC_SHADOW), 0, 1, 0);
    UI_Shade(&ob->geometry.origin, &ob->geometry.size, UI_BORDER,
        UI_Color(UIC_BRD_HI), UI_Color(UIC_BRD_LOW), alpha / 3, -1);
    UI_DrawRectEx(&ob->geometry.origin, &ob->geometry.size, UI_BORDER * (sel ? -1 : 1), false,
        UI_Color(UIC_BRD_HI), NULL, alpha, -1);

    name = B_ShortNameForKey((int) key);
    if(name)
        sprintf(buf, "%s", name);
    else if(key > 32 && key < 128)
        sprintf(buf, "%c", (char) key);
    else
        sprintf(buf, "%i", key);

    FR_SetFont(fontVariable[FS_LIGHT]);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    labelOrigin.x = ob->geometry.origin.x + ob->geometry.size.width  / 2;
    labelOrigin.y = ob->geometry.origin.y + ob->geometry.size.height / 2;

    UI_TextOutEx2(buf, &labelOrigin, UI_Color(UIC_TEXT), alpha, 0, DTF_ONLY_SHADOW);
    glDisable(GL_TEXTURE_2D);
}

void CP_QuickFOV(ui_object_t* ob)
{
    Con_SetFloat2("rend-camera-fov", sld_fov.value = atoi(ob->text), SVF_WRITE_OVERRIDE);
}

void CP_VideoModeInfo(ui_object_t* ob)
{
    Point2Raw textOrigin;
    char buf[80];
    assert(ob);

    if(!strcmp(ob->text, "default"))
    {
        sprintf(buf, "%i x %i x %i (%s)", defResX, defResY, defBPP,
            defFullscreen? "fullscreen":"windowed");
    }
    else
    {
        boolean fullscreen = Window_IsFullscreen(Window_Main());

        sprintf(buf, "%i x %i x %i (%s)", Window_Width(theWindow),
            Window_Height(theWindow), Window_ColorDepthBits(theWindow),
            (fullscreen? "fullscreen" : "windowed"));
    }

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glEnable(GL_TEXTURE_2D);
    FR_SetFont(fontVariable[FS_LIGHT]);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    textOrigin.x = ob->geometry.origin.x;
    textOrigin.y = ob->geometry.origin.y + ob->geometry.size.height / 2;

    UI_TextOutEx2(buf, &textOrigin, UI_Color(UIC_TEXT), 1, ALIGN_LEFT, DTF_ONLY_SHADOW);
    glDisable(GL_TEXTURE_2D);
}

void CP_UpdateSetVidModeButton(int w, int h, int bpp, boolean fullscreen)
{
    boolean cFullscreen = Window_IsFullscreen(Window_Main());
    ui_object_t* ob;

    ob = UI_FindObject(ob_panel, CPG_VIDEO, CPID_SET_RES);

    bpp = (bpp? 32 : 16);
    sprintf(ob->text, "%i x %i x %i (%s)", w, h, bpp,
            fullscreen? "fullscreen" : "windowed");

    if(w == Window_Width(theWindow) && h == Window_Height(theWindow) &&
       bpp == Window_ColorDepthBits(theWindow) && fullscreen == cFullscreen)
        ob->flags |= UIF_DISABLED;
    else
        ob->flags &= ~UIF_DISABLED;
}

void CP_ResolutionList(ui_object_t* ob)
{
    uidata_list_t* list = (uidata_list_t *) ob->data;
    int seldata = ((uidata_listitem_t *) list->items)[list->selection].data;

    sprintf(panel_res_x, "%i", seldata & 0xffff);
    sprintf(panel_res_y, "%i", seldata >> 16);

    strcpy(UI_FindObject(ob_panel, ob->group, CPID_RES_X)->text, panel_res_x);
    strcpy(UI_FindObject(ob_panel, ob->group, CPID_RES_Y)->text, panel_res_y);

    CP_VidModeChanged(ob);
}

void CP_SetDefaultVidMode(ui_object_t* ob)
{
    int x = atoi(panel_res_x), y = atoi(panel_res_y);
    if(!x || !y) return;

    defResX = x;
    defResY = y;
    defBPP = (panel_bpp? 32 : 16);
    defFullscreen = panel_fullscreen;
}

void CP_SetVidMode(ui_object_t* ob)
{
    int x = atoi(panel_res_x), y = atoi(panel_res_y);
    int bpp = (panel_bpp? 32 : 16);

    if(!x || !y) return;
    if(x < SCREENWIDTH || y < SCREENHEIGHT) return;

    ob->flags |= UIF_DISABLED;

    {
        int attribs[] = {
            DDWA_WIDTH, x,
            DDWA_HEIGHT, y,
            DDWA_COLOR_DEPTH_BITS, bpp,
            DDWA_FULLSCREEN, panel_fullscreen != 0,
            DDWA_END
        };
        Window_ChangeAttributes(Window_Main(), attribs);
    }
}

void CP_VidModeChanged(ui_object_t *ob)
{
    CP_UpdateSetVidModeButton(atoi(panel_res_x), atoi(panel_res_y),
                              panel_bpp, panel_fullscreen);
}

/**
 * Returns the object, if any, the mouse is currently hovering on. The
 * check is based on the coordinates of the Text object.
 */
ui_object_t* CP_FindHover(void)
{
    ui_object_t* ob;
    Size2Raw size;

    for(ob = ob_panel; ob->type; ob++)
    {
        if(ob->flags & UIF_HIDDEN || ob->type != UI_TEXT || ob->group < 2 ||
           ob->relx < 280)
            continue;

        // Extend the detection area to the right edge of the screen.
        size.width  = UI_ScreenW(1000);
        size.height = ob->geometry.size.height;
        if(UI_MouseInsideBox(&ob->geometry.origin, &size))
            return ob;
    }
    return NULL;
}

/**
 * Track the mouse and move the documentation window as needed.
 */
void CP_Ticker(ui_page_t* page)
{
    ui_object_t* ob;
    HelpId help;
    int off;

    // Normal ticker actions first.
    UIPage_Ticker(page);

    // Check if the mouse is inside the options box.
    ob = UI_FindObject(page->_objects, 0, CPID_FRAME);
    if(!UI_MouseInside(ob) || !panel_show_help)
    {
        panel_help_active = false;
    }
    else
    {
        // The mouse is inside the options box, so we may need to display
        // the options box or change its text. Detect which object the
        // mouse is on.
        if((ob = CP_FindHover()))
        {
            // Change the text.
            if((help = DH_Find(ob->text)))
            {
                panel_help = help;
                panel_help_source = ob;
            }

            if(UI_MouseResting(page))
            {
                // The mouse has been paused on a text, activate help.
                panel_help_active = true;
            }

            if(!help)
                panel_help_active = false;
        }
    }

    // Should we move the help box?
    off = 0;
    if(panel_help_active && UI_Alpha() >= 1.0)
    {
        if(panel_help_offset < HELP_OFFSET)
            off = (HELP_OFFSET - panel_help_offset) / 2;
        if(off < 4)
            off = 4;
    }
    else                        // Help should be hidden.
    {
        if(panel_help_offset > 0)
            off = -panel_help_offset / 2;
        if(off > -4)
            off = -4;
    }
    panel_help_offset += off;
    if(panel_help_offset > HELP_OFFSET)
        panel_help_offset = HELP_OFFSET;
    if(panel_help_offset < 0)
        panel_help_offset = 0;
}

int CP_LabelText(char const *label, char const *text, Point2Raw const *origin_, Size2Raw const *size_, float alpha)
{
    ui_color_t* color = UI_Color(UIC_TEXT);
    Point2Raw origin;
    Size2Raw size;
    int ind;

    DENG_ASSERT(origin_ && size_);

    origin.x = origin_->x;
    origin.y = origin_->y;
    size.width  = size_->width;
    size.height = size_->height;

    FR_SetFont(fontVariable[FS_NORMAL]);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetColorAndAlpha(color->red, color->green, color->blue, .5f * alpha * UI_Alpha());

    FR_DrawText(label, &origin);

    ind = FR_TextWidth(label);
    origin.x += ind;
    size.width -= ind;

    return UI_TextOutWrapEx(text, &origin, &size, color, alpha);
}

void CP_Drawer(ui_page_t* page)
{
    float alpha = panel_help_offset / (float) HELP_OFFSET;
    int bor, lineHeight, verticalSpacing;
    Point2Raw origin, end;
    Size2Raw size;
    char const *str;

    // First call the regular drawer.
    UIPage_Drawer(page);

    // Project home.
    glEnable(GL_TEXTURE_2D);
    FR_SetFont(fontVariable[FS_LIGHT]);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    origin.x = UI_ScreenW(1000) - UI_BORDER;
    origin.y = UI_ScreenY(25);
    UI_TextOutEx2(DENGPROJECT_HOMEURL, &origin, UI_Color(UIC_TEXT), 0.2f, ALIGN_RIGHT, DTF_ONLY_SHADOW);

    // Is the help box visible?
    if(panel_help_offset <= 0 || !panel_help_source)
    {
        glDisable(GL_TEXTURE_2D);
        return;
    }

    // Help box placement.
    bor = 2 * UI_BORDER / 3;

    origin.x = -UI_BORDER;
    origin.y = UI_ScreenY(0);
    size.width  = HELP_OFFSET;
    size.height = UI_ScreenH(920);

    UI_GradientEx(&origin, &size, UI_BORDER, UI_Color(UIC_HELP), UI_Color(UIC_HELP), alpha, alpha);
    UI_DrawRectEx(&origin, &size, UI_BORDER, false, UI_Color(UIC_BRD_HI), NULL, alpha, -1);

    origin.x += UI_BORDER + 2 * bor;
    origin.y += UI_BORDER;
    size.width  -= UI_BORDER + 4 * bor;
    size.height -= 4 * bor;

    // The title (with shadow).
    FR_SetFont(fontVariable[FS_BOLD]);
    lineHeight = FR_SingleLineHeight("Help");
    verticalSpacing = lineHeight / 4;
    origin.y = UI_TextOutWrapEx(panel_help_source->text, &origin, &size, UI_Color(UIC_TITLE), alpha);
    origin.y += lineHeight + 3;

    end.x = origin.x + size.width;
    end.y = origin.y;
    UI_Line(&origin, &end, UI_Color(UIC_TEXT), 0, alpha * .5f, 0);
    origin.y += verticalSpacing;

    // Cvar?
    str = DH_GetString(panel_help, HST_CONSOLE_VARIABLE);
    if(str)
    {
        origin.y = CP_LabelText("CVar: ", str, &origin, &size, alpha);
        origin.y += lineHeight + verticalSpacing;
    }

    // Default?
    str = DH_GetString(panel_help, HST_DEFAULT_VALUE);
    if(str)
    {
        origin.y = CP_LabelText("Default: ", str, &origin, &size, alpha);
        origin.y += lineHeight + verticalSpacing;
    }

    // Information.
    str = DH_GetString(panel_help, HST_DESCRIPTION);
    if(str)
    {
        FR_SetFont(fontVariable[FS_LIGHT]);
        UI_TextOutWrapEx(str, &origin, &size, UI_Color(UIC_TEXT), alpha);
    }
    glDisable(GL_TEXTURE_2D);
}

/**
 * Initializes all slider objects.
 */
void CP_InitCvarSliders(ui_object_t *ob)
{
    for(; ob->type; ob++)
    {
        if(ob->action == CP_CvarSlider)
        {
            uidata_slider_t *slid = (uidata_slider_t *) ob->data;

            if(slid->floatmode)
                slid->value = Con_GetFloat((char const *) slid->data);
            else
                slid->value = Con_GetInteger((char const *) slid->data);
        }
    }
}

static void populateDisplayResolutions(void)
{
    int i, k, p = 0;

    lstit_resolution = (uidata_listitem_t *) Z_Recalloc(lstit_resolution,
                                  sizeof(*lstit_resolution) * DisplayMode_Count(),
                                  PU_APPSTATIC);

    for(i = 0; i < DisplayMode_Count(); ++i)
    {
        const DisplayMode* mode = DisplayMode_ByIndex(i);
        int spec = RES(mode->width, mode->height);

        // Make sure we haven't added this size yet (many with different refresh rates).
        for(k = 0; k < p; ++k)
        {
            if(lstit_resolution[k].data == spec) break;
        }
        if(k < p) continue; // Already got it.

        dd_snprintf(lstit_resolution[p].text, sizeof(lstit_resolution[p].text),
                    "%i x %i (%i:%i)", mode->width, mode->height, mode->ratioX, mode->ratioY);
        lstit_resolution[p].data = spec;
        ++p;
    }

    lst_resolution.items = lstit_resolution;
    lst_resolution.count = p;
}

/**
 * Initialize and open the Control Panel.
 */
D_CMD(OpenPanel)
{
    ui_object_t* ob, *foc;
    uidata_list_t* list;
    cvarbutton_t* cvb;
    int i;

    Con_Execute(CMDS_DDAY, "conclose", true, false);

    populateDisplayResolutions();

    // The help window is hidden.
    panel_help_active = false;
    panel_help_offset = 0;
    panel_help_source = NULL;

    UI_InitPage(&page_panel, ob_panel);
    page_panel.ticker = CP_Ticker;
    page_panel.drawer = CP_Drawer;
    if(argc != 2)
    {
        // Choose the group that was last visible.
        foc = NULL;
        for(i = 0; i < NUM_CP_BUTTONS; ++i)
            if(panel_buttons[i])
                CP_ChooseGroup(foc = ob_panel + i);
    }
    else
    {
        // With an argument, choose the appropriate group.
        foc = NULL;
        for(i = 0; i < NUM_CP_BUTTONS; ++i)
        {
            if(!stricmp(ob_panel[i].text, argv[1]))
            {
                CP_ChooseGroup(foc = ob_panel + i);
                break;
            }
        }

        if(!foc)
            CP_ChooseGroup(foc = ob_panel);
    }

    // Set default Yes/No strings.
    for(cvb = cvarbuttons; cvb->cvarname; cvb++)
    {
        if(!cvb->yes)
            cvb->yes = "Yes";
        if(!cvb->no)
            cvb->no = "No";
    }

    // Set cvarbutton data pointers.
    // This is only done the first time "panel" is issued.
    for(i = 0, ob = ob_panel; ob_panel[i].type; ++i, ob++)
    {
        if(ob->action == CP_CvarButton)
        {
            if(ob->data)
            {
                // This button has already been initialized.
                cvb = (cvarbutton_t *) ob->data;
                cvb->active = (Con_GetByte(cvb->cvarname) & (ob->data2? ob->data2 : ~0)) != 0;
                strcpy(ob->text, cvb->active ? cvb->yes : cvb->no);
                continue;
            }
            // Find the cvarbutton representing this one.
            for(cvb = cvarbuttons; cvb->cvarname; cvb++)
            {
                if(!strcmp(ob->text, cvb->cvarname) && ob->data2 == cvb->mask)
                {
                    cvb->active = (Con_GetByte(cvb->cvarname) & (ob->data2? ob->data2 : ~0)) != 0;
                    ob->data = cvb;
                    strcpy(ob->text, cvb->active ? cvb->yes : cvb->no);
                    break;
                }
            }
        }
        else if(ob->action == CP_CvarList)
        {
            list = (uidata_list_t *) ob->data;
            // Choose the correct list item based on the value of the cvar.
            list->selection = UI_ListFindItem(ob, Con_GetInteger((char const *) list->data));
        }
        else if(ob->action == CP_CvarEdit)
        {
            uidata_edit_t *ed = (uidata_edit_t *) ob->data;

            strncpy(ed->ptr, Con_GetString((char const *) ed->data), ed->maxlen);
        }
    }
    CP_InitCvarSliders(ob_panel);

    // Update width the current resolution.
    {
        boolean cFullscreen = Window_IsFullscreen(Window_Main());

        ob = UI_FindObject(ob_panel, CPG_VIDEO, CPID_RES_LIST);
        list = (uidata_list_t *) ob->data;
        list->selection = UI_ListFindItem(ob,
            RES(Window_Width(theWindow), Window_Height(theWindow)));
        if(list->selection == -1)
        {
            // Then use a reasonable default.
            list->selection = UI_ListFindItem(ob, RES(640, 480));
        }
        panel_fullscreen = cFullscreen;
        panel_bpp = (Window_ColorDepthBits(theWindow) == 32? 1 : 0);
        CP_ResolutionList(ob);
    }

    UI_PageInit(true, true, false, false, false);
    UI_SetPage(&page_panel);
    UI_Focus(foc);
    return true;
}
