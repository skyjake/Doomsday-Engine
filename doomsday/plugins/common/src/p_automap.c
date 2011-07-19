/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2008-2011 Daniel Swanson <danij@dengine.net>
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
 * p_automap.c: The automap.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "doomsday.h" // temporary, to be removed.

#include "p_automap.h"

// MACROS ------------------------------------------------------------------

#define LERP(start, end, pos) (end * pos + start * (1 - pos))

// Translate between frame and map distances:
#define FTOM(map, x) ((x) * (map)->scaleFTOM)
#define MTOF(map, x) ((x) * (map)->scaleMTOF)

// Map boundry plane idents:
#define BOXTOP              (0)
#define BOXBOTTOM           (1)
#define BOXLEFT             (2)
#define BOXRIGHT            (3)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static void rotate2D(float* x, float* y, float angle)
{
#define PI                  3.141592657

    float               tmpx;

    tmpx = (float) ((*x * cos(angle/180 * PI)) - (*y * sin(angle/180 * PI)));
    *y   = (float) ((*x * sin(angle/180 * PI)) + (*y * cos(angle/180 * PI)));
    *x = tmpx;

#undef PI
}

/**
 * Calculate the min/max scaling factors.
 *
 * Take the distance from the bottom left to the top right corners and
 * choose a max scaling factor such that this distance is short than both
 * the automap window width and height.
 */
static void calcViewScaleFactors(automap_t* map)
{
    float               dx, dy, dist, a, b;

    if(!map)
        return; // hmm...

    dx = map->bounds[BOXRIGHT] - map->bounds[BOXLEFT];
    dy = map->bounds[BOXTOP]   - map->bounds[BOXBOTTOM];
    dist = (float) sqrt(dx * dx + dy * dy);
    if(dist < 0)
        dist = -dist;

    a = map->window.width  / dist;
    b = map->window.height / dist;

    map->minScaleMTOF = (a < b ? a : b);
    map->maxScaleMTOF = map->window.height / map->minScale;

    map->updateViewScale = false;
}

void Automap_SetWorldBounds(automap_t* map, float lowX, float hiX,
                            float lowY, float hiY)
{
    if(!map)
        return;

    map->bounds[BOXLEFT]   = lowX;
    map->bounds[BOXTOP]    = hiY;
    map->bounds[BOXRIGHT]  = hiX;
    map->bounds[BOXBOTTOM] = lowY;

    map->updateViewScale = true;
}

void Automap_SetMinScale(automap_t* map, const float scale)
{
    if(!map)
        return;

    map->minScale = MAX_OF(1, scale);
    map->updateViewScale = true;
}

/**
 * @param max           Maximum view position delta, in world units.
 */
void Automap_SetMaxLocationTargetDelta(automap_t* map, float max)
{
    if(!map)
        return;

    map->maxViewPositionDelta = MINMAX_OF(0, max, 32768*2);
}

void Automap_Open(automap_t* map, int yes, int fast)
{
    if(!map)
        return;

    if(yes == map->active)
        return; // No change.

    map->targetAlpha = (yes? 1.f : 0.f);
    if(fast)
    {
        map->alpha = map->oldAlpha = map->targetAlpha;
    }
    else
    {
        // Reset the timer.
        map->oldAlpha = map->alpha;
        map->alphaTimer = 0.f;
    }

    map->active = (yes? true : false);
}

void Automap_RunTic(automap_t* map)
{
    float               width, height, scale;

    if(!map)
        return;

    if(map->updateViewScale)
        calcViewScaleFactors(map);

    // Window position and dimensions.
    if(!map->fullScreenMode)
    {
        automapwindow_t*    win = &map->window;

        win->posTimer += .4f;
        if(win->posTimer >= 1)
        {
            win->x = win->targetX;
            win->y = win->targetY;
            win->width = win->targetWidth;
            win->height = win->targetHeight;
        }
        else
        {
            win->x = LERP(win->oldX, win->targetX, win->posTimer);
            win->y = LERP(win->oldY, win->targetY, win->posTimer);
            win->width = LERP(win->oldWidth, win->targetWidth, win->posTimer);
            win->height = LERP(win->oldHeight, win->targetHeight, win->posTimer);
        }
    }

    // Map viewer location.
    map->viewTimer += .4f;
    if(map->viewTimer >= 1)
    {
        map->viewX = map->targetViewX;
        map->viewY = map->targetViewY;
    }
    else
    {
        map->viewX = LERP(map->oldViewX, map->targetViewX, map->viewTimer);
        map->viewY = LERP(map->oldViewY, map->targetViewY, map->viewTimer);
    }
    // Move the parallax layer.
    map->viewPLX = map->viewX / 4000;
    map->viewPLY = map->viewY / 4000;

    // Map view scale (zoom).
    map->viewScaleTimer += .4f;
    if(map->viewScaleTimer >= 1)
    {
        map->viewScale = map->targetViewScale;
    }
    else
    {
        map->viewScale =
            LERP(map->oldViewScale, map->targetViewScale, map->viewScaleTimer);
    }

    // Map view rotation.
    map->angleTimer += .4f;
    if(map->angleTimer >= 1)
    {
        map->angle = map->targetAngle;
    }
    else
    {
        float diff, startAngle = map->oldAngle, endAngle = map->targetAngle;

        if(endAngle > startAngle)
        {
            diff = endAngle - startAngle;
            if(diff > 180)
                endAngle = startAngle - (360 - diff);
        }
        else
        {
            diff = startAngle - endAngle;
            if(diff > 180)
                endAngle = startAngle + (360 - diff);
        }

        map->angle = LERP(startAngle, endAngle, map->angleTimer);
        if(map->angle < 0)
            map->angle += 360;
        else if(map->angle > 360)
            map->angle -= 360;
    }

    //
    // Activate the new scale, position etc.
    //
    scale = map->viewScale;

    // Scaling multipliers.
    map->scaleMTOF = scale;
    map->scaleFTOM = 1.0f / map->scaleMTOF;

    width = Automap_FrameToMap(map, map->window.width);
    height = Automap_FrameToMap(map, map->window.height);

    // Calculate the in-view, AABB.
    {   // Rotation-aware.
#define ADDTOBOX(b, x, y) if((x) < (b)[BOXLEFT]) \
    (b)[BOXLEFT] = (x); \
    else if((x) > (b)[BOXRIGHT]) \
        (b)[BOXRIGHT] = (x); \
    if((y) < (b)[BOXBOTTOM]) \
        (b)[BOXBOTTOM] = (y); \
    else if((y) > (b)[BOXTOP]) \
        (b)[BOXTOP] = (y);

    float           angle;
    float           v[2];

    angle = map->angle;

    v[0] = -width / 2;
    v[1] = -height / 2;
    rotate2D(&v[0], &v[1], angle);
    v[0] += map->viewX;
    v[1] += map->viewY;

    map->viewAABB[BOXLEFT] = map->viewAABB[BOXRIGHT]  = v[0];
    map->viewAABB[BOXTOP]  = map->viewAABB[BOXBOTTOM] = v[1];

    v[0] = width / 2;
    v[1] = -height / 2;
    rotate2D(&v[0], &v[1], angle);
    v[0] += map->viewX;
    v[1] += map->viewY;
    ADDTOBOX(map->viewAABB, v[0], v[1]);

    v[0] = -width / 2;
    v[1] = height / 2;
    rotate2D(&v[0], &v[1], angle);
    v[0] += map->viewX;
    v[1] += map->viewY;
    ADDTOBOX(map->viewAABB, v[0], v[1]);

    v[0] = width / 2;
    v[1] = height / 2;
    rotate2D(&v[0], &v[1], angle);
    v[0] += map->viewX;
    v[1] += map->viewY;
    ADDTOBOX(map->viewAABB, v[0], v[1]);

#undef ADDTOBOX
    }
}

/**
 * Translates from map to automap window coordinates.
 */
float Automap_MapToFrame(const automap_t* map, float val)
{
    if(!map)
        return 1;

    return MTOF(map, val);
}

/**
 * Translates from automap window to map coordinates.
 */
float Automap_FrameToMap(const automap_t* map, float val)
{
    if(!map)
        return 1;

    return FTOM(map, val);
}

void Automap_SetWindowTarget(automap_t* map, int x, int y, int w, int h)
{
    automapwindow_t*    win;

    if(!map)
        return;

    // Are we in fullscreen mode?
    // If so, setting the window size is not allowed.
    if(map->fullScreenMode)
        return;

    win = &map->window;

    // Already at this target?
    if(x == win->targetX && y == win->targetY &&
       w == win->targetWidth && h == win->targetHeight)
        return;

    win->oldX = win->x;
    win->oldY = win->y;
    win->oldWidth = win->width;
    win->oldHeight = win->height;
    // Restart the timer.
    win->posTimer = 0;

    win->targetX = (float) x;
    win->targetY = (float) y;
    win->targetWidth = (float) w;
    win->targetHeight = (float) h;
}

void Automap_GetWindow(const automap_t* map, float* x, float* y, float* w,
                       float* h)
{
    if(!map)
        return;

    if(x) *x = map->window.x;
    if(y) *y = map->window.y;
    if(w) *w = map->window.width;
    if(h) *h = map->window.height;
}

void Automap_SetLocationTarget(automap_t* map, float x, float y)
{
    boolean             instantChange = false;

    if(!map)
        return;

    x = MINMAX_OF(-32768, x, 32768);
    y = MINMAX_OF(-32768, y, 32768);

    // Already at this target?
    if(x == map->targetViewX && y == map->targetViewY)
        return;

    if(map->maxViewPositionDelta > 0)
    {
        float               dx, dy, dist;

        dx = map->viewX - x;
        dy = map->viewY - y;
        dist = (float) sqrt(dx * dx + dy * dy);
        if(dist < 0)
            dist = -dist;

        if(dist > map->maxViewPositionDelta)
            instantChange = true;
    }

    if(instantChange)
    {
        map->viewX = map->oldViewX = map->targetViewX = x;
        map->viewY = map->oldViewY = map->targetViewY = y;
    }
    else
    {
        map->oldViewX = map->viewX;
        map->oldViewY = map->viewY;
        map->targetViewX = x;
        map->targetViewY = y;
        // Restart the timer.
        map->viewTimer = 0;
    }
}

void Automap_GetLocation(const automap_t* map, float* x, float* y)
{
    if(!map)
        return;

    if(x) *x = map->viewX;
    if(y) *y = map->viewY;
}

void Automap_GetViewParallaxPosition(const automap_t* map, float* x, float* y)
{
    if(!map)
        return;

    if(x) *x = map->viewPLX;
    if(y) *y = map->viewPLY;
}

/**
 * @return              Current alpha level of the automap.
 */
float Automap_GetViewAngle(const automap_t* map)
{
    if(!map)
        return 0;

    return map->angle;
}

void Automap_SetViewScaleTarget(automap_t* map, float scale)
{
    if(!map)
        return;

    if(map->updateViewScale)
        calcViewScaleFactors(map);

    scale = MINMAX_OF(map->minScaleMTOF, scale, map->maxScaleMTOF);

    // Already at this target?
    if(scale == map->targetViewScale)
        return;

    map->oldViewScale = map->viewScale;
    // Restart the timer.
    map->viewScaleTimer = 0;

    map->targetViewScale = scale;
}

void Automap_SetViewAngleTarget(automap_t* map, float angle)
{
    if(!map)
        return;

    // Already at this target?
    if(angle == map->targetAngle)
        return;

    map->oldAngle = map->angle;
    map->targetAngle = MINMAX_OF(0, angle, 359.9999f);

    // Restart the timer.
    map->angleTimer = 0;
}

float Automap_MapToFrameMultiplier(const automap_t* map)
{
    if(!map)
        return 1;

    return map->scaleMTOF;
}

/**
 * @return              True if the specified map is currently active.
 */
int Automap_IsActive(const automap_t* map)
{
    if(!map)
        return false;

    return map->active;
}

void Automap_GetInViewAABB(const automap_t* map, float* lowX, float* hiX,
                           float* lowY, float* hiY)
{
    if(!map)
        return;

   if(lowX) *lowX = map->viewAABB[BOXLEFT];
   if(hiX)  *hiX  = map->viewAABB[BOXRIGHT];
   if(lowY) *lowY = map->viewAABB[BOXBOTTOM];
   if(hiY)  *hiY  = map->viewAABB[BOXTOP];
}

void Automap_ClearMarks(automap_t* map)
{
    uint                i;

    if(!map)
        return;

    for(i = 0; i < MAX_MAP_POINTS; ++i)
        map->markpointsUsed[i] = false;
    map->markpointnum = 0;
}

unsigned int Automap_GetNumMarks(const automap_t* map)
{
    unsigned int        i, numUsed = 0;

    if(!map)
        return numUsed;

    for(i = 0; i < MAX_MAP_POINTS; ++i)
        if(map->markpointsUsed[i])
            numUsed++;

    return numUsed;
}

/**
 * Adds a marker at the current location.
 */
int Automap_AddMark(automap_t* map, float x, float y, float z)
{
    unsigned int        num;
    automappoint_t*     point;

    if(!map)
        return -1;

    num = map->markpointnum;
    point = &map->markpoints[num];
    point->pos[0] = x;
    point->pos[1] = y;
    point->pos[2] = z;
    map->markpointsUsed[num] = true;
    map->markpointnum = (map->markpointnum + 1) % MAX_MAP_POINTS;

    return num;
}

int Automap_GetMark(const automap_t* map, unsigned int mark, float* x,
                    float* y, float* z)
{
    if(!map)
        return false;

    if(!x && !y && !z)
        return false;

    if(mark < MAX_MAP_POINTS && map->markpointsUsed[mark])
    {
        const automappoint_t* point = &map->markpoints[mark];

        if(x) *x = point->pos[0];
        if(y) *y = point->pos[1];
        if(z) *z = point->pos[2];

        return true;
    }

    return false;
}

/**
 * Toggles between active and max zoom.
 */
void Automap_ToggleZoomMax(automap_t* map)
{
    if(!map)
        return;

    if(map->updateViewScale)
        calcViewScaleFactors(map);

    // When switching to max scale mode, store the old scale.
    if(!map->forceMaxScale)
        map->priorToMaxScale = map->viewScale;

    map->forceMaxScale = !map->forceMaxScale;
    Automap_SetViewScaleTarget(map, (map->forceMaxScale? 0 : map->priorToMaxScale));
}

/**
 * Toggles follow mode.
 */
void Automap_ToggleFollow(automap_t* map)
{
    if(!map)
        return;

    map->panMode = !map->panMode;
}

void Automap_SetViewRotate(automap_t* map, int on)
{
    if(!map)
        return;

    map->rotate = on;
}

void Automap_SetWindowFullScreenMode(automap_t* map, int value)
{
    if(!map)
        return;

    if(value < 0 || value > 2)
    {
#if _DEBUG
Con_Error("Automap::SetWindowFullScreenMode: Unknown value %i.", value);
#endif
        return;
    }

    if(value == 2) // toggle
        map->fullScreenMode = !map->fullScreenMode;
    else
        map->fullScreenMode = value;
}

int Automap_IsMapWindowInFullScreenMode(const automap_t* map)
{
    if(!map)
        return false;

    return map->fullScreenMode;
}

/**
 * Set the alpha level of the automap. Alpha levels below one automatically
 * show the game view in addition to the automap.
 *
 * @param alpha         Alpha level to set the automap too (0...1)
 */
void Automap_SetOpacityTarget(automap_t* map, float alpha)
{
    if(!map)
        return;
    alpha = MINMAX_OF(0, alpha, 1);

    // Already at this target?
    if(alpha == map->targetAlpha)
        return;

    map->oldAlpha = map->alpha;
    // Restart the timer.
    map->alphaTimer = 0;

    map->targetAlpha = alpha;
}

/**
 * @return              Current alpha level of the automap.
 */
float Automap_GetOpacity(const automap_t* map)
{
    if(!map)
        return 0;

    return map->alpha;
}

int Automap_GetFlags(const automap_t* map)
{
    if(!map)
        return 0;

    return map->flags;
}

/**
 * @param flags         AMF_* flags.
 */
void Automap_SetFlags(automap_t* map, int flags)
{
    if(!map)
        return;

    map->flags = flags;
}

void Automap_UpdateWindow(automap_t* map, float newX, float newY,
                          float newWidth, float newHeight)
{
    automapwindow_t*   win;

    if(!map)
        return;

    win = &map->window;

    if(newX != win->x || newY != win->y ||
       newWidth != win->width || newHeight != win->height)
    {
        if(map->fullScreenMode)
        {
            // In fullscreen mode we always snap straight to the
            // new dimensions.
            win->x = win->oldX = win->targetX = newX;
            win->y = win->oldY = win->targetY = newY;
            win->width = win->oldWidth = win->targetWidth = newWidth;
            win->height = win->oldHeight = win->targetHeight = newHeight;
        }
        else
        {
            // Snap dimensions if new scale is smaller.
            if(newX > win->x)
                win->x = win->oldX = win->targetX = newX;
            if(newY > win->y)
                win->y = win->oldY = win->targetY = newY;
            if(newWidth < win->width)
                win->width = win->oldWidth = win->targetWidth = newWidth;
            if(newHeight < win->height)
                win->height = win->oldHeight = win->targetHeight = newHeight;
        }

        // Now the screen dimensions have changed we have to update scaling
        // factors accordingly.
        map->updateViewScale = true;
    }
}
