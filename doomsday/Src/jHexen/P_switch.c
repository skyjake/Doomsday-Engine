
//**************************************************************************
//**
//** p_switch.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile$
//** $Revision$
//** $Date$
//** $Author$
//**
//**************************************************************************

/*
 * Switches, buttons. Two-state animation. Exits.
 */

// HEADER FILES ------------------------------------------------------------

#include "h2def.h"
#include "jHexen/p_local.h"
#include "jHexen/soundst.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

button_t buttonlist[MAXBUTTONS];

switchlist_t switchInfo[] = {
    {"SW_1_UP", "SW_1_DN", SFX_SWITCH1},
    {"SW_2_UP", "SW_2_DN", SFX_SWITCH1},
    {"VALVE1", "VALVE2", SFX_VALVE_TURN},
    {"SW51_OFF", "SW51_ON", SFX_SWITCH2},
    {"SW52_OFF", "SW52_ON", SFX_SWITCH2},
    {"SW53_UP", "SW53_DN", SFX_ROPE_PULL},
    {"PUZZLE5", "PUZZLE9", SFX_SWITCH1},
    {"PUZZLE6", "PUZZLE10", SFX_SWITCH1},
    {"PUZZLE7", "PUZZLE11", SFX_SWITCH1},
    {"PUZZLE8", "PUZZLE12", SFX_SWITCH1},

    {"\0", "\0", 0}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int *switchlist;
static int max_numswitches;
static int numswitches;

// CODE --------------------------------------------------------------------

/*
 * Called at game initialization or when the engine's state must be updated
 * (eg a new WAD is loaded at runtime). This routine will populate the list
 * of known switches and buttons. This enables their texture to change when
 * activated, and in the case of buttons, change back after a timeout.
 */
void P_InitSwitchList(void)
{
    int     i;
    int     index;

    for(index = 0, i = 0; ; ++i)
    {
        if(index+1 >= max_numswitches)
        {
            switchlist = realloc(switchlist, sizeof *switchlist *
                (max_numswitches = max_numswitches ? max_numswitches*2 : 8));
        }

        if(!switchInfo[i].soundID)
            break;

        switchlist[index++] = R_CheckTextureNumForName(switchInfo[i].name1);
        switchlist[index++] = R_CheckTextureNumForName(switchInfo[i].name2);
    }

    numswitches = index / 2;
    switchlist[index] = -1;
}

/*
 * Start a button (retriggerable switch) counting down till it turns off.
 *
 * Passed the linedef the button is on, which texture on the sidedef contains
 * the button, the texture number of the button, and the time the button is
 * to remain active in gametics.
 */
void P_StartButton(line_t *line, bwhere_e w, int texture, int time)
{
    int     i;

    for(i = 0; i < MAXBUTTONS; i++)
    {
        if(!buttonlist[i].btimer) // use first unused element of list
        {
            buttonlist[i].line = line;
            buttonlist[i].where = w;
            buttonlist[i].btexture = texture;
            buttonlist[i].btimer = time;
            buttonlist[i].soundorg =
                P_GetPtrp(P_GetPtrp(line, DMU_FRONT_SECTOR), DMU_SOUND_ORIGIN);
            return;
        }
    }

    Con_Error("P_StartButton: no button slots left!");
}

/*
 * Function that changes wall texture.
 * Tell it if switch is ok to use again (1=yes, it's a button).
 */
void P_ChangeSwitchTexture(line_t *line, int useAgain)
{
    int     texTop;
    int     texMid;
    int     texBot;
    int     i;
    side_t*     sdef = P_GetPtrp(line, DMU_SIDE0);
    sector_t*   frontsector = P_GetPtrp(line, DMU_FRONT_SECTOR);

#if 0
    if(!useAgain)
        P_XLine(line)->special = 0;
#endif

    texTop = P_GetIntp(sdef, DMU_TOP_TEXTURE);
    texMid = P_GetIntp(sdef, DMU_MIDDLE_TEXTURE);
    texBot = P_GetIntp(sdef, DMU_BOTTOM_TEXTURE);

    for(i = 0; i < numswitches * 2; i++)
    {
        if(switchlist[i] == texTop)
        {
            S_StartSound(switchInfo[i / 2].soundID,
                         P_GetPtrp(frontsector, DMU_SOUND_ORIGIN));

            P_SetIntp(sdef, DMU_TOP_TEXTURE, switchlist[i ^ 1]);

            if(useAgain)
                P_StartButton(line, SWTCH_TOP, switchlist[i], BUTTONTIME);

            return;
        }
        else
        {
            if(switchlist[i] == texMid)
            {
                S_StartSound(switchInfo[i / 2].soundID,
                             P_GetPtrp(frontsector, DMU_SOUND_ORIGIN));

                P_SetIntp(sdef, DMU_MIDDLE_TEXTURE, switchlist[i ^ 1]);

                if(useAgain)
                    P_StartButton(line, SWTCH_MIDDLE, switchlist[i], BUTTONTIME);

                return;
            }
            else
            {
                if(switchlist[i] == texBot)
                {
                    S_StartSound(switchInfo[i / 2].soundID,
                                 P_GetPtrp(frontsector, DMU_SOUND_ORIGIN));

                    P_SetIntp(sdef, DMU_BOTTOM_TEXTURE, switchlist[i ^ 1]);

                    if(useAgain)
                        P_StartButton(line, SWTCH_BOTTOM, switchlist[i], BUTTONTIME);

                    return;
                }
            }
        }
    }
}
