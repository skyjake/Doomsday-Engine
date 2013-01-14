/**\file ui_mpi.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
 * Multiplayer Setup Interface
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include "de_base.h"
#include "de_system.h"
#include "de_filesys.h"
#include "de_console.h"
#include "de_ui.h"

#include "network/net_main.h"
#include "network/net_event.h"

// MACROS ------------------------------------------------------------------

#define MAX_FOUND           32
#define NUMITEMS(x)         (sizeof(x)/sizeof(uidata_listitem_t))

#define UIF_SERVER_LIST     UIF_ID0
#define UIG_CONNECT         6

// TYPES -------------------------------------------------------------------

typedef enum searchmode_e {
    SEARCH_MASTER,
    SEARCH_CUSTOM
} searchmode_t;

typedef struct serverstrings_s {
    char    desc[90];
    char    version[20];
    char    ping[20];
    char    plugin[80];
    char    names[256];
    char    pwads[256];
    char    warning[128];
} serverstrings_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void    MPIGotoPage(ui_object_t *ob);
void    MPIGoBack(ui_object_t *ob);
void    MPIChooseMode(ui_object_t *ob);
void    MPIToggleMasterItems(ui_object_t *ob);
void    MPIStartServer(ui_object_t *ob);
void    MPIShowProtocolSettings(ui_object_t *ob);
void    MPISetupProtocol(ui_object_t *ob);
void    MPISearch(ui_object_t *ob);
void    MPIRetrieve(ui_object_t *ob);
void    MPIUpdateServerList(void);
void    MPIUpdateFound(ui_object_t *ob);
void    MPIConnect(ui_object_t *ob);
void    MPIHelpDrawer(ui_object_t *ob);
void    MPIUpdateServerInfo(ui_object_t *ob);
void    MPIServerInfoDrawer(ui_object_t *ob);
void    MPIUpdatePublicButton(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

char    str_server[101];
char    str_desc[201];
char    str_masterip[128];
char    str_ipaddr[128];
char    str_ipport[128];
//char    str_myudp[51];

serverstrings_t str_sinfo;

uidata_slider_t sld_player_limit =
    { 0, 16, 0, 1, false, (void *) "server-player-limit" };

uidata_listitem_t lstit_found[MAX_FOUND];

static boolean mode_buttons[2] = { true, false };

static uidata_edit_t ed_server = { str_server, 100 };
static uidata_edit_t ed_desc = { str_desc, 200 };
static uidata_edit_t ed_masterip = { str_masterip, 127 };
static uidata_list_t lst_found = { lstit_found, 0 };
static uidata_edit_t ed_ipsearch = { str_ipaddr, 127 };
static uidata_edit_t ed_portsearch = { str_ipport, 127 };
//static uidata_edit_t ed_myudp = { str_myudp, 50 };

static ui_page_t page_server, page_client;

static ui_object_t ob_server[] = {  // Server setup
    {UI_TEXT, 0, 0, 50, 200, 0, 70, "Server name", UIText_Drawer},
    {UI_EDIT, 0, 0, 320, 200, 500, 70, "", UIEdit_Drawer, UIEdit_Responder, 0,
     0, &ed_server},
    {UI_TEXT, 0, 0, 50, 300, 0, 70, "Description", UIText_Drawer},
    {UI_EDIT, 0, 0, 320, 300, 580, 70, "", UIEdit_Drawer, UIEdit_Responder, 0,
     0, &ed_desc},
    {UI_TEXT, 0, 0, 50, 400, 0, 70, "Max. players", UIText_Drawer},
    {UI_SLIDER, 0, 0, 320, 405, 350, 60, "", UISlider_Drawer,
     UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_player_limit},
    {UI_TEXT, 0, 0, 50, 600, 0, 70, "Register on master?", UIText_Drawer},
    {UI_BUTTON2, 2, 0, 320, 600, 100, 70, "Yes", UIButton_Drawer,
     UIButton_Responder, 0, MPIToggleMasterItems, &masterAware},
    {UI_TEXT, 1, 0, 50, 700, 0, 70, "Master TCP/IP address", UIText_Drawer},
    {UI_EDIT, 1, 0, 320, 700, 350, 70, "", UIEdit_Drawer, UIEdit_Responder, 0,
     0, &ed_masterip},

    /* The port settings and "Start" button */
    {UI_BOX, 0, 0, 0, 910, 1000, 90, "", UIFrame_Drawer},
    {UI_TEXT, 0, 0, 20, 920, 0, 70, "Incoming TCP port", UIText_Drawer},
    {UI_EDIT, 0, 0, 230, 920, 110, 70, "", UIEdit_Drawer, UIEdit_Responder, 0,
     0, &ed_portsearch},
    {UI_TEXT, 0, 0, 420, 920, 0, 70, "In/out UDP port", UIText_Drawer},
    /*{UI_EDIT, 0, 0, 600, 920, 110, 70, "", UIEdit_Drawer, UIEdit_Responder, 0,
     0, &ed_myudp},*/

    {UI_BUTTON, 0, UIF_DEFAULT, 800, 920, 180, 70, "Start", UIButton_Drawer,
     UIButton_Responder, 0, MPIStartServer},

    {UI_NONE}
};

static ui_object_t ob_client[] = {
    {UI_TEXT, 0, 0, 0, 0, 0, 70, "Get servers from:", UIText_Drawer},
    {UI_BUTTON2, 1, UIF_ACTIVE, 200, 0, 180, 70, "Master Server",
     UIButton_Drawer, UIButton_Responder, 0, MPIChooseMode, &mode_buttons[0]},
    {UI_BUTTON2, 2, 0, 390, 0, 180, 70, "Custom Search", UIButton_Drawer,
     UIButton_Responder, 0, MPIChooseMode, &mode_buttons[1]},
    {UI_BOX, 0, 0, 0, 80, 1000, 90, "", UIFrame_Drawer},

    /* Search Parameters: Master Server */
    {UI_TEXT, 3, 0, 20, 90, 0, 70, "Master server's address", UIText_Drawer},
    {UI_EDIT, 3, 0, 290, 90, 500, 70, "", UIEdit_Drawer, UIEdit_Responder, 0,
     0, &ed_masterip},

    /* Search Parameters: Custom Search */
    {UI_TEXT, 4, 0, 20, 90, 0, 70, "Query address (IP or hostname)",
     UIText_Drawer},
    {UI_EDIT, 4, 0, 360, 90, 290, 70, "", UIEdit_Drawer, UIEdit_Responder, 0,
     0, &ed_ipsearch},
    {UI_TEXT, 4, 0, 680, 90, 0, 70, "Server's TCP port", UIText_Drawer},
    {UI_EDIT, 4, 0, 870, 90, 110, 70, "", UIEdit_Drawer, UIEdit_Responder, 0,
     0, &ed_portsearch},

    {UI_BUTTON, 0, UIF_DEFAULT, 0, 180, 180, 70, "Update", UIButton_Drawer,
     UIButton_Responder, 0, MPISearch},

    /* The List of Servers */
    {UI_LIST, 0, UIF_SERVER_LIST, 190, 180, 810, 300, "", UIList_Drawer,
     UIList_Responder, MPIUpdateFound, MPIUpdateServerInfo, &lst_found},
    {UI_TEXT, 0, 0, 20, 490, 0, 70, "Description", UIText_Drawer},
    {UI_TEXT, 0, 0, 190, 490, 570, 70, "", MPIServerInfoDrawer, 0, 0, 0,
     str_sinfo.desc},
    {UI_TEXT, 0, 0, 20, 570, 0, 70, "Game", UIText_Drawer},
    {UI_TEXT, 0, 0, 190, 570, 250, 70, "", MPIServerInfoDrawer, 0, 0, 0,
     str_sinfo.plugin},
    {UI_TEXT, 0, 0, 460, 570, 0, 70, "Version", UIText_Drawer},
    {UI_TEXT, 0, 0, 560, 570, 200, 70, "", MPIServerInfoDrawer, 0, 0, 0,
     str_sinfo.version},
    {UI_TEXT, 0, 0, 20, 650, 0, 70, "Setup", UIText_Drawer},
    {UI_TEXT, 0, 0, 190, 650, 570, 120, "", MPIServerInfoDrawer, 0, 0, 0,
     str_sinfo.pwads},
    {UI_TEXT, 0, 0, 20, 780, 0, 70, "Players", UIText_Drawer},
    {UI_TEXT, 0, 0, 190, 780, 570, 120, "", MPIServerInfoDrawer, 0, 0, 0,
     str_sinfo.names},

    {UI_TEXT, 5, UIF_DISABLED, 770, 490, 0, 70, "Warnings", UIText_Drawer},
    {UI_TEXT, 5, UIF_DISABLED, 770, 560, 230, 340, "", MPIServerInfoDrawer,
     0, 0, 0, str_sinfo.warning},

    //{UI_TEXT, 0, 0, 20, 870, 0, 70, "Ping", UIText_Drawer},
    //{UI_TEXT, 0, 0, 190, 870, 200, 70, "", MPIServerInfoDrawer, 0, 0, 0,
    // str_sinfo.ping},

    //{UI_BOX, 0, 0, 0, 910, 1000, 90, "", UIFrame_Drawer},
    //{UI_TEXT, 0, 0, 20, 920, 0, 70, "My UDP port", UIText_Drawer},
    /*{UI_EDIT, 0, 0, 170, 920, 110, 70, "", UIEdit_Drawer, UIEdit_Responder, 0,
     0, &ed_myudp},*/
    /*{UI_TEXT, 0, 0, 290, 920, 0, 70,
     "(must be open for incoming and outgoing traffic)", UIText_Drawer},*/

    {UI_BUTTON, UIG_CONNECT, 0, 190, 920, 360, 70, "Connect to Server", UIButton_Drawer,
     UIButton_Responder, 0, MPIConnect},

    {UI_NONE}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static searchmode_t searchMode = SEARCH_MASTER;
static boolean lookedForHosts = false;
static boolean retrieving = false;
static unsigned int myCrc = 0;
static char warningString[256];

// CODE --------------------------------------------------------------------

/*
 * Change the search mode.
 */
void MPIChooseMode(ui_object_t *ob)
{
    int     i;

    memset(mode_buttons, 0, sizeof(mode_buttons));
    UI_FlagGroup(ob_client, 1, UIF_ACTIVE, UIFG_CLEAR);
    UI_FlagGroup(ob_client, 2, UIF_ACTIVE, UIFG_CLEAR);
    *(boolean *) ob->data = true;
    ob->flags |= UIF_ACTIVE;

    // Hide/show the option controls.
    for(i = 1; i <= 2; i++)
        UI_FlagGroup(ob_client, i + 2, UIF_HIDDEN, !mode_buttons[i - 1]);

    // Update the search mode.
    searchMode = (mode_buttons[0] ? SEARCH_MASTER : SEARCH_CUSTOM);

    // Update the list of found hosts.
    MPIUpdateServerList();
}

/*
 * Clear all the strings that display information about currently
 * selected server.
 */
void MPIClearServerInfo(void)
{
    memset(&str_sinfo, 0, sizeof(str_sinfo));
}

/*
 * Copies string from src to dest, replacing 'match' chars with 'rep'.
 */
void MPITranslateString(char *dest, const char *src, char match,
                        const char *rep)
{
    char   *dp = dest;
    const char *sp = src;

    for(; *sp; sp++)
        if(*sp == match)
        {
            strcpy(dp, rep);
            dp += strlen(rep);
        }
        else
        {
            *dp++ = *sp;
        }
    *dp = 0;
}

/*
 * Update the strings that display information about currently selected
 * server. This is called when the server selection changes in the list.
 */
void MPIUpdateServerInfo(ui_object_t *ob)
{
    serverinfo_t info;
    char    str[256];
    int     success;

    // If nothing is currently selected, clear the info fields.
    if(lst_found.selection < 0)
    {
        MPIClearServerInfo();
        return;
    }

    if(retrieving)
        return;

    success =
        (searchMode ==
         SEARCH_MASTER ? N_MasterGet(lstit_found[lst_found.selection].data2,
                                     &info) :
         N_GetHostInfo(lstit_found[lst_found.selection].data2, &info));

    if(!success)
    {
        MPIClearServerInfo();
        return;
    }

    strcpy(str_sinfo.desc, info.description);
    sprintf(str_sinfo.version, "%i", info.version);
    //  if(info.ping)
    //      sprintf(str_sinfo.ping, "%i ms", info.ping);
    //  else
    //      strcpy(str_sinfo.ping, "?");
    strcpy(str_sinfo.plugin, info.plugin);
    MPITranslateString(str_sinfo.names, info.clientNames, ';', ", ");
    MPITranslateString(str, info.pwads, ';', ", ");
    strcpy(str_sinfo.pwads, info.gameIdentityKey);
    if(info.gameConfig[0])
    {
        strcat(str_sinfo.pwads, " ");
        strcat(str_sinfo.pwads, info.gameConfig);
    }
    if(info.pwads[0])
    {
        strcat(str_sinfo.pwads, " (");
        strcat(str_sinfo.pwads, str);
        strcat(str_sinfo.pwads, ")");
    }

    sprintf(warningString,
            "This server is using %x, " "but you have %x. "
            "Errors may occur during game play.", info.loadedFilesCRC, myCrc);

    // Show IWAD warning?
    if(!(lst_found.count >= 1 && lst_found.selection >= 0 &&
        lstit_found[lst_found.selection].data != -1 &&
        lstit_found[lst_found.selection].data != (int) myCrc))
    {
        UI_FlagGroup(ob_client, 5, UIF_DISABLED, true);
        strcpy(str_sinfo.warning, "");
    }
    else
    {
        UI_FlagGroup(ob_client, 5, UIF_DISABLED, false);
        strcpy(str_sinfo.warning, warningString);
    }
}

/*
 * Draw a framed text box.
 */
void MPIServerInfoDrawer(ui_object_t* ob)
{
    UI_DrawHelpBox(&ob->geometry.origin, &ob->geometry.size, ob->flags & UIF_DISABLED ? .2f : 1, (char *) ob->data);
}

void MPIToggleMasterItems(ui_object_t* ob)
{
    UI_FlagGroup(page_server._objects, 1, UIF_DISABLED, UIFG_XOR);
    MPIUpdatePublicButton();
}

void MPIGotoPage(ui_object_t* ob)
{
    UI_SetPage((ui_page_t*) ob->data);
}

void MPIGoBack(ui_object_t* ob)
{
    if(!UI_CurrentPage()->previous)
        UI_End();
    else
        UI_SetPage(UI_CurrentPage()->previous);
}

void MPIStartServer(ui_object_t* ob)
{
    N_ShutdownService();
    Con_SetInteger2("net-port-data", strtol(str_ipport, 0, 0), SVF_WRITE_OVERRIDE);
    N_InitService(true);

    // Update the variables.
    Con_SetString2("server-name", str_server, SVF_WRITE_OVERRIDE);
    Con_SetString2("server-info", str_desc, SVF_WRITE_OVERRIDE);
    Con_SetString2("net-master-address", str_masterip, SVF_WRITE_OVERRIDE);

    // Start the server.
    Con_Execute(CMDS_DDAY,"net server start", false, false);

    UI_End();
}

void MPIFinishCustomServerSearch(int nodeId, const byte* data, int size)
{
    N_ClientHandleResponseToInfoQuery(nodeId, data, size);
    MPIUpdateServerList();
}

void MPISearch(ui_object_t *ob)
{
    if(retrieving)
        return;

    // Network services are naturally needed for searching.
    if(!N_IsAvailable())
        N_InitService(false);

    if(searchMode == SEARCH_CUSTOM)
    {
        // This is a synchronous operation.
        N_LookForHosts(str_ipaddr, strtol(str_ipport, 0, 0), MPIFinishCustomServerSearch);
        lookedForHosts = true;
    }
    else
    {
        // Get servers from the master.
        MPIRetrieve(ob);
    }
}

/*
 * Formats the given serverinfo_t into a list-viewable tabbed string.
 */
void MPIFormatServerInfo(char* dest, serverinfo_t *info)
{
    sprintf(dest, "%s\t%i / %i players\t%s\t%s", info->name, info->numPlayers,
            info->maxPlayers, info->map, info->gameIdentityKey);
}

/*
 * Fill the server list with the list of currently known servers.
 */
void MPIUpdateServerList(void)
{
    int     num, i, k;
    serverinfo_t info;
    ui_object_t *listObject = UI_FindObject(ob_client, 0, UIF_SERVER_LIST);

    if(searchMode == SEARCH_CUSTOM)
    {
        num = N_GetHostCount();
        if(!num)
        {
            lst_found.selection = -1;
            if(lookedForHosts)
            {
                lst_found.count = 1;
                lstit_found[0].data = -1;
                sprintf(lstit_found[0].text, "(No response from %s)",
                        str_ipaddr);
            }
            else
            {
                lst_found.count = 0;
            }
            UI_FlagGroup(ob_client, UIG_CONNECT, UIF_DISABLED, true);
        }
        else
        {
            if(num > MAX_FOUND)
                num = MAX_FOUND;
            for(i = 0; i < num; i++)
            {
                memset(&info, 0, sizeof(info));
                N_GetHostInfo(i, &info);
                MPIFormatServerInfo(lstit_found[i].text, &info);
                lstit_found[i].data = info.loadedFilesCRC;
                lstit_found[i].data2 = i;
            }
            lst_found.count = num;
            lst_found.selection = 0;
            UI_FlagGroup(ob_client, UIG_CONNECT, UIF_DISABLED, false);
        }
    }
    else if(searchMode == SEARCH_MASTER)
    {
        num = N_MasterGet(0, 0);
        for(i = 0, k = 0; i < num && k < MAX_FOUND; i++)
        {
            N_MasterGet(i, &info);

            // Is this suitable?
            if(info.version != DOOMSDAY_VERSION || stricmp(info.gameIdentityKey, Str_Text(Game_IdentityKey(App_CurrentGame()))) || !info.canJoin)
            {
                Con_Message("Server %s filtered out:\n", info.name);
                Con_Message("  remote = %i, local = %i\n", info.version, DOOMSDAY_VERSION);
                Con_Message("  remote = %s, local = %s\n", info.gameIdentityKey, Str_Text(Game_IdentityKey(App_CurrentGame())));
                Con_Message("  can join = %i\n", info.canJoin);
                continue;
            }

            MPIFormatServerInfo(lstit_found[k].text, &info);
            lstit_found[k].data = info.loadedFilesCRC;
            // Connection will be formed using this index.
            lstit_found[k].data2 = i;
            k++;
        }
        lst_found.count = k;
        UI_FlagGroup(ob_client, UIG_CONNECT, UIF_DISABLED, !k);
    }

    // Auto-select.
    if(lst_found.selection < 0 && lst_found.count)
        lst_found.selection = 0;

    // Make sure the current selection isn't past the end of the list.
    if(lst_found.selection >= lst_found.count)
        lst_found.selection = -1;

    UI_InitColumns(listObject);
    MPIUpdateServerInfo(NULL);
}

/*
 * "Located Servers" list ticker.
 */
void MPIUpdateFound(ui_object_t *ob)
{
    // Call list ticker.
    UIList_Ticker(ob);

    if(retrieving && N_MADone())
    {
        // The list has been retrieved.
        retrieving = false;

        if(searchMode == SEARCH_MASTER)
        {
            MPIUpdateServerList();
        }
    }
}

void MPIRetrieveServersFromMaster()
{
    ui_object_t *list = UI_FindObject(ob_client, 0, UIF_SERVER_LIST);

    if(retrieving)
        return;                 // Already retrieving!

    retrieving = true;

    // Disable Connect button.
    UI_FlagGroup(ob_client, UIG_CONNECT, UIF_DISABLED, true);
    MPIClearServerInfo();
    lst_found.selection = -1;
    lst_found.count = 1;
    strcpy(lstit_found[0].text, "(Retrieving servers from master...)");
    lstit_found[0].data = -1;
    UI_InitColumns(list);

    // Update master settings.
    Con_SetString2("net-master-address", str_masterip, SVF_WRITE_OVERRIDE);

    // Get the list.
    N_MAPost(MAC_REQUEST);
    N_MAPost(MAC_WAIT);
}

void MPIRetrieve(ui_object_t *ob)
{
    MPIRetrieveServersFromMaster();
}

void MPIConnect(ui_object_t *ob)
{
    char    buf[80];

    N_ShutdownService();
    N_InitService(false);

    sprintf(buf, "net %sconnect %i", searchMode == SEARCH_MASTER ? "m" : "",
            lstit_found[lst_found.selection].data2);
    if(Con_Execute(CMDS_DDAY,buf, false, false))
    {
        // Success.
        UI_End();
    }
}

/*
 * Draw the per-page information about the currently displayed protocol.
 * The text is retrieved from the Help strings.
 */
#if 0
void MPIHelpDrawer(ui_object_t *ob)
{
    int     yPos[4] = { 250, 80, 530, 900 };
    int     selection = lst_protocol.selection;
    static int lastSelection = -1;
    static void *help = NULL;
    char   *text;

    // Has the page changed?
    if(lastSelection != selection)
    {
        // Retrieve new help text.
        lastSelection = selection;
        help = DH_Find(lstit_protocols[selection].text);
    }

    if((text = DH_GetString(help, HST_DESCRIPTION)) != NULL)
    {
        FR_SetFont(fontVariable[FS_LIGHT]);
        FR_LoadDefaultAttrib();
        UI_TextOutWrap(text, ob->x, UI_ScreenY(yPos[selection]), ob->w,
                       UI_ScreenH(980 - yPos[selection]));
    }
}
#endif

void MPIUpdatePublicButton(void)
{
    strcpy(UI_FindObject(ob_server, 2, 0)->text, masterAware ? "Yes" : "No");
}

/*
 * The GUI interface for setting up a server/client.
 */
void DD_NetSetup(int serverMode)
{
    if(!DD_GameLoaded())
    {
        Con_Message("%s setup can only be activated when a game is loaded.\n", serverMode? "Server" : "Client");
        return;
    }

    lookedForHosts = false;

    if(serverMode)
    {
        // Prepare Server Setup.
        UI_InitPage(&page_server, ob_server);
        strcpy(str_server, serverName);
        strcpy(str_desc, serverInfo);

        // Disable the Master Address and Port edit boxes.
        UI_FlagGroup(ob_server, 1, UIF_DISABLED, !masterAware);

        MPIUpdatePublicButton();
    }
    else
    {
        // Prepare Client Setup.
        UI_InitPage(&page_client, ob_client);
        strcpy(str_ipaddr, nptIPAddress);

        UI_FlagGroup(ob_client, 1, UIF_ACTIVE, searchMode == SEARCH_MASTER);
        UI_FlagGroup(ob_client, 2, UIF_ACTIVE, searchMode != SEARCH_MASTER);
        UI_FlagGroup(ob_client, 3, UIF_HIDDEN, searchMode != SEARCH_MASTER);
        UI_FlagGroup(ob_client, 4, UIF_HIDDEN, searchMode == SEARCH_MASTER);
        lst_found.selection = -1;
        lst_found.count = 0;
        myCrc = F_LoadedFilesCRC();
        UI_FlagGroup(ob_client, 5, UIF_DISABLED, true); // warnings
        UI_FlagGroup(ob_client, UIG_CONNECT, UIF_DISABLED, true);
        MPIUpdateServerList();
    }

    sprintf(str_ipport, "%i", (nptIPPort ? nptIPPort : DEFAULT_TCP_PORT));
    //sprintf(str_myudp, "%i", 0); // (nptUDPPort ? nptUDPPort : DEFAULT_UDP_PORT));
    strcpy(str_masterip, masterAddress);

    // Prepare Protocol Setup.
    //UI_InitPage(&page_protocol, ob_protocol);
    //strcpy(page_protocol.title, "Network Setup");
    //page_protocol.previous = server_mode ? &page_server : &page_client;
    //sprintf(str_ipport, "%.10i", nptIPPort);
    //lst_protocol.selection = nptActive;

    UI_PageInit(true, true, false, false, false);
    UI_SetPage(serverMode ? &page_server : &page_client);

    CP_InitCvarSliders(ob_server);

    // Automatically look for servers on the master.
    if(!serverMode && searchMode == SEARCH_MASTER)
    {
        MPIRetrieveServersFromMaster();
    }
}
