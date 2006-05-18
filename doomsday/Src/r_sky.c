/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * r_sky.c: Sky Management
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"
#include "de_console.h"

#include "cl_def.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

skymodel_t skyModels[NUM_SKY_MODELS];
boolean skyModelsInited = false;
boolean alwaysDrawSphere = false;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * The sky models are set up using the data in the definition.
 */
void R_SetupSkyModels(ded_mapinfo_t * info)
{
    int     i;
    ded_skymodel_t *def;
    skymodel_t *sky;

    // Clear the whole sky models data.
    memset(skyModels, 0, sizeof(skyModels));

    // Normally the sky sphere is not drawn if models are in use.
    alwaysDrawSphere = (info->flags & MIF_DRAW_SPHERE) != 0;

    // The normal sphere is used if no models will be set up.
    skyModelsInited = false;

    for(i = 0, def = info->sky_models, sky = skyModels; i < NUM_SKY_MODELS;
        i++, def++, sky++)
    {
        // Is the model ID set?
        if((sky->model = R_CheckIDModelFor(def->id)) == NULL)
            continue;

        // There is a model here.
        skyModelsInited = true;

        sky->def = def;
        sky->maxTimer = (int) (TICSPERSEC * def->frame_interval);
        sky->yaw = def->yaw;
        sky->frame = sky->model->sub[0].frame;
    }
}

/*
 * Prepare all sky model skins.
 */
void R_PrecacheSky(void)
{
    int     i;
    skymodel_t *sky;

    if(!skyModelsInited)
        return;

    for(i = 0, sky = skyModels; i < NUM_SKY_MODELS; i++, sky++)
    {
        if(!sky->def)
            continue;
        R_PrecacheModelSkins(sky->model);
    }
}

/*
 * Animate sky models.
 */
void R_SkyTicker(void)
{
    int     i;
    skymodel_t *sky;

    if(!skyModelsInited || clientPaused)
        return;

    for(i = 0, sky = skyModels; i < NUM_SKY_MODELS; i++, sky++)
    {
        if(!sky->def)
            continue;

        // Turn the model.
        sky->yaw += sky->def->yaw_speed / TICSPERSEC;

        // Is it time to advance to the next frame?
        if(sky->maxTimer > 0 && ++sky->timer >= sky->maxTimer)
        {
            sky->timer = 0;
            sky->frame++;

            // Execute a console command?
            if(sky->def->execute)
                Con_Execute(CMDS_DED, sky->def->execute, true);
        }
    }
}
