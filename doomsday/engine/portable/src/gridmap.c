/**
 * @file gridmap.h
 * Gridmap implementation. @ingroup data
 *
 * @authors Copyright &copy; 2009-2012 Daniel Swanson <danij@dengine.net>
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

#include <math.h>
#include <string.h>

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h" // For debug display
#include "de_refresh.h" // For debug display
#include "de_render.h" // For debug display
#include "m_vector.h" // For debug display

#include "gridmap.h"

/// Space dimension ordinals.
enum { X = 0, Y };

/// TreeCell child quadrant identifiers.
typedef enum {
    TOPLEFT = 0,
    TOPRIGHT,
    BOTTOMLEFT,
    BOTTOMRIGHT
} Quadrant;

/**
 * TreeCell. Used to represent a subquadrant within the owning Gridmap.
 */
typedef struct treecell_s
{
    /// User data associated with the cell. Note that only leafs can have
    /// associated user data.
    void* userData;

    /// Origin of this cell in Gridmap space [x,y].
    uint origin[2];

    /// Size of this cell in Gridmap space (width=height).
    uint size;

    /// Child cells of this, one for each subquadrant.
    struct treecell_s* children[4];
} TreeCell;

/**
 * Gridmap implementation. Designed around that of a Region Quadtree
 * with inherent sparsity and compression potential.
 */
struct gridmap_s
{
    /// Dimensions of the space we are indexing (in cells).
    uint dimensions[2];

    /// Zone memory tag used for both the Gridmap and user data.
    int zoneTag;

    /// Size of the memory block to be allocated for each leaf.
    size_t sizeOfCell;

    /// Root cell for our Quadtree. Allocated along with the Gridmap instance.
    TreeCell root;
};

/**
 * Initialize @a cell. Assumes that @a cell has not yet been initialized
 * (i.e., existing references to child cells will be overwritten).
 *
 * @param cell        TreeCell instance to be initialized.
 * @param x           X coordinate in Gridmap space.
 * @param y           Y coordinate in Gridmap space.
 * @param size        Size in Gridmap space units.
 *
 * @return  Same as @a cell for caller convenience.
 */
static TreeCell* initCell(TreeCell* cell, uint x, uint y, uint size)
{
    assert(cell);
    cell->origin[X] = x;
    cell->origin[Y] = y;
    cell->size = size;
    cell->userData = NULL;
    memset(cell->children, 0, sizeof cell->children);
    return cell;
}

/**
 * Construct and initialize a new TreeCell.
 *
 * @param x            X coordinate in Gridmap space.
 * @param y            Y coordinate in Gridmap space.
 * @param size         Size in Gridmap space units.
 * @param zoneTag      Zone memory tag to associate with the new TreeCell.
 *
 * @return  Newly allocated and initialized TreeCell instance.
 */
static TreeCell* newCell(uint x, uint y, uint size, int zoneTag)
{
    TreeCell* cell = Z_Malloc(sizeof *cell, zoneTag, NULL);
    if(!cell) Con_Error("Gridmap::newCell: Failed on allocation of %lu bytes for new Cell.", (unsigned long) sizeof *cell);
    return initCell(cell, x, y, size);
}

static void deleteCell(TreeCell* cell)
{
    assert(cell);
    // Deletion is a depth-first traversal.
    if(cell->children[TOPLEFT])     deleteCell(cell->children[TOPLEFT]);
    if(cell->children[TOPRIGHT])    deleteCell(cell->children[TOPRIGHT]);
    if(cell->children[BOTTOMLEFT])  deleteCell(cell->children[BOTTOMLEFT]);
    if(cell->children[BOTTOMRIGHT]) deleteCell(cell->children[BOTTOMRIGHT]);
    if(cell->userData) Z_Free(cell->userData);
    Z_Free(cell);
}

/// @return  @c true= @a cell is a a leaf (i.e., equal to a unit in Gridmap space).
static boolean isLeaf(TreeCell* cell)
{
    assert(cell);
    return cell->size == 1;
}

/**
 * Depth-first traversal of the children of this cell, making a callback
 * for each cell. Iteration ends when all selected cells have been visited
 * or a callback returns a non-zero value.
 *
 * @param cell          TreeCell to traverse.
 * @param leafOnly      Caller is only interested in leaves.
 * @param callback      Callback function.
 * @param parameters    Passed to the callback.
 *
 * @return  Zero iff iteration completed wholly, else the value returned by the
 *          last callback made.
 */
static int iterateCell(TreeCell* cell, boolean leafOnly,
    int (C_DECL *callback) (TreeCell* cell, void* parameters), void* parameters)
{
    int result = false; // Continue traversal.
    assert(cell && callback);

    if(!isLeaf(cell))
    {
        if(cell->children[TOPLEFT])
        {
            result = iterateCell(cell->children[TOPLEFT], leafOnly, callback, parameters);
            if(result) return result;
        }
        if(cell->children[TOPRIGHT])
        {
            result = iterateCell(cell->children[TOPRIGHT], leafOnly, callback, parameters);
            if(result) return result;
        }
        if(cell->children[BOTTOMLEFT])
        {
            result = iterateCell(cell->children[BOTTOMLEFT], leafOnly, callback, parameters);
            if(result) return result;
        }
        if(cell->children[BOTTOMRIGHT])
        {
            result = iterateCell(cell->children[BOTTOMRIGHT], leafOnly, callback, parameters);
            if(result) return result;
        }
    }
    if(!leafOnly || isLeaf(cell))
    {
        result = callback(cell, parameters);
    }
    return result;
}

static void deleteTree(Gridmap* gm)
{
    assert(gm);
    // The root cell is allocated along with Gridmap.
    if(gm->root.children[TOPLEFT])     deleteCell(gm->root.children[TOPLEFT]);
    if(gm->root.children[TOPRIGHT])    deleteCell(gm->root.children[TOPRIGHT]);
    if(gm->root.children[BOTTOMLEFT])  deleteCell(gm->root.children[BOTTOMLEFT]);
    if(gm->root.children[BOTTOMRIGHT]) deleteCell(gm->root.children[BOTTOMRIGHT]);
    if(gm->root.userData) Z_Free(gm->root.userData);
}

static TreeCell* findLeafDescend(Gridmap* gm, TreeCell* cell, uint x, uint y, boolean alloc)
{
    Quadrant q;
    assert(cell);

    if(isLeaf(cell))
    {
        return cell;
    }

    // Into which quadrant do we need to descend?
    if(x < cell->origin[X] + (cell->size >> 1))
    {
        q = (y < cell->origin[Y] + (cell->size >> 1))? TOPLEFT  : BOTTOMLEFT;
    }
    else
    {
        q = (y < cell->origin[Y] + (cell->size >> 1))? TOPRIGHT : BOTTOMRIGHT;
    }

    // Has this quadrant been initialized yet?
    if(!cell->children[q])
    {
        uint subOrigin[2], subSize;

        // Are we allocating cells?
        if(!alloc) return NULL;

        // Subdivide this cell and construct the new.
        subSize = cell->size >> 1;
        switch(q)
        {
        case TOPLEFT:
            subOrigin[X] = cell->origin[X];
            subOrigin[Y] = cell->origin[Y];
            break;
        case TOPRIGHT:
            subOrigin[X] = cell->origin[X] + subSize;
            subOrigin[Y] = cell->origin[Y];
            break;
        case BOTTOMLEFT:
            subOrigin[X] = cell->origin[X];
            subOrigin[Y] = cell->origin[Y] + subSize;
            break;
        case BOTTOMRIGHT:
            subOrigin[X] = cell->origin[X] + subSize;
            subOrigin[Y] = cell->origin[Y] + subSize;
            break;
        default:
            Con_Error("Gridmap::findUserDataAdr: Invalid quadrant %i.", (int) q);
            exit(1); // Unreachable.
        }
        cell->children[q] = newCell(subOrigin[X], subOrigin[Y], subSize, gm->zoneTag);
    }

    return findLeafDescend(gm, cell->children[q], x, y, alloc);
}

static TreeCell* findLeaf(Gridmap* gm, uint x, uint y, boolean alloc)
{
    assert(gm);
    return findLeafDescend(gm, &gm->root, x, y, alloc);
}

static uint ceilPow2(uint unit)
{
    uint cumul;
    for(cumul = 1; unit > cumul; cumul <<= 1);
    return cumul;
}

Gridmap* Gridmap_New(uint width, uint height, size_t cellSize, int zoneTag)
{
    Gridmap* gm = Z_Calloc(sizeof *gm, zoneTag, 0);
    uint size;
    if(!gm) Con_Error("Gridmap::New: Failed on allocation of %lu bytes for new Gridmap.", (unsigned long) sizeof *gm);

    gm->dimensions[X] = width;
    gm->dimensions[Y] = height;
    gm->sizeOfCell = cellSize;
    gm->zoneTag = zoneTag;

    // Quadtree must subdivide the space equally into 1x1 unit cells.
    size = ceilPow2(MAX_OF(width, height));
    initCell(&gm->root, 0, 0, size);

    return gm;
}

void Gridmap_Delete(Gridmap* gm)
{
    assert(gm);
    deleteTree(gm);
    Z_Free(gm);
}

uint Gridmap_Width(const Gridmap* gm)
{
    assert(gm);
    return gm->dimensions[X];
}

uint Gridmap_Height(const Gridmap* gm)
{
    assert(gm);
    return gm->dimensions[Y];
}

void Gridmap_Size(const Gridmap* gm, uint widthHeight[2])
{
    assert(gm);
    if(!widthHeight) return;
    widthHeight[X] = gm->dimensions[X];
    widthHeight[Y] = gm->dimensions[Y];
}

void* Gridmap_CellXY(Gridmap* gm, uint x, uint y, boolean alloc)
{
    TreeCell* cell;
    assert(gm);

    // Outside our boundary?
    if(x >= gm->dimensions[X] || y >= gm->dimensions[Y]) return NULL;

    // Try to locate this leaf (may fail if not present and we are
    // not allocating user data (there will be no corresponding cell)).
    cell = findLeaf(gm, x, y, alloc);
    if(!cell) return NULL;

    // Exisiting user data for this cell?
    if(cell->userData) return cell->userData;

    // Allocate new user data?
    if(!alloc) return NULL;
    return cell->userData = Z_Calloc(gm->sizeOfCell, gm->zoneTag, 0);
}

void* Gridmap_Cell(Gridmap* gm, uint const coords[2], boolean alloc)
{
    return Gridmap_CellXY(gm, coords[X], coords[Y], alloc);
}

typedef struct {
    Gridmap_IterateCallback callback;
    void* callbackParamaters;
} actioncallback_paramaters_t;

/**
 * Callback actioner. Executes the callback and then returns the result
 * to the current iteration to determine if it should continue.
 */
static int actionCallback(TreeCell* cell, void* parameters)
{
    actioncallback_paramaters_t* p = (actioncallback_paramaters_t*) parameters;
    assert(cell && p);
    if(cell->userData)
        return p->callback(cell->userData, p->callbackParamaters);
    return 0; // Continue traversal.
}

int Gridmap_Iterate2(Gridmap* gm, Gridmap_IterateCallback callback,
    void* parameters)
{
    actioncallback_paramaters_t p;
    assert(gm);
    p.callback = callback;
    p.callbackParamaters = parameters;
    return iterateCell(&gm->root, true/*only leaves*/, actionCallback, (void*)&p);
}

int Gridmap_Iterate(Gridmap* gm, Gridmap_IterateCallback callback)
{
    return Gridmap_Iterate2(gm, callback, NULL/*no params*/);
}

int Gridmap_BlockIterate2(Gridmap* gm, const GridmapBlock* block_,
    Gridmap_IterateCallback callback, void* parameters)
{
    GridmapBlock block;
    TreeCell* cell;
    uint x, y;
    int result;
    assert(gm);

    // Clip coordinates to our boundary dimensions (the underlying
    // Quadtree is normally larger than this so we cannot use the
    // dimensions of the root cell here).
    memcpy(&block, block_, sizeof block);
    Gridmap_ClipBlock(gm, &block);

    // Traverse cells in the block.
    /// @optimize: We could avoid repeatedly descending the tree...
    for(y = block.minY; y <= block.maxY; ++y)
    for(x = block.minX; x <= block.maxX; ++x)
    {
        cell = findLeaf(gm, x, y, false);
        if(!cell || !cell->userData) continue;

        result = callback(cell->userData, parameters);
        if(result) return result;
    }
    return false;
}

int Gridmap_BlockIterate(Gridmap* gm, const GridmapBlock* block,
    Gridmap_IterateCallback callback)
{
    return Gridmap_BlockIterate2(gm, block, callback, NULL/*no params*/);
}

int Gridmap_BlockXYIterate2(Gridmap* gm, uint minX, uint minY, uint maxX, uint maxY,
    Gridmap_IterateCallback callback, void* parameters)
{
    GridmapBlock block;
    GridmapBlock_SetCoordsXY(&block, minX, maxX, minY, maxY);
    return Gridmap_BlockIterate2(gm, &block, callback, parameters);
}

int Gridmap_BlockXYIterate(Gridmap* gm, uint minX, uint minY, uint maxX, uint maxY,
    Gridmap_IterateCallback callback)
{
    return Gridmap_BlockXYIterate2(gm, minX, minY, maxX, maxY, callback, NULL/*no params*/);
}

boolean Gridmap_ClipBlock(Gridmap* gm, GridmapBlock* block)
{
    boolean adjusted = false;
    assert(gm);
    if(block)
    {
        if(block->minX >= gm->dimensions[X])
        {
            block->minX = gm->dimensions[X]-1;
            adjusted = true;
        }
        if(block->minY >= gm->dimensions[Y])
        {
            block->minY = gm->dimensions[Y]-1;
            adjusted = true;
        }
        if(block->maxX >= gm->dimensions[X])
        {
            block->maxX = gm->dimensions[X]-1;
            adjusted = true;
        }
        if(block->maxY >= gm->dimensions[Y])
        {
            block->maxY = gm->dimensions[Y]-1;
            adjusted = true;
        }
    }
    return adjusted;
}

void GridmapBlock_SetCoords(GridmapBlock* block, uint const min[2], uint const max[2])
{
    assert(block);
    if(min)
    {
        block->minX = min[0];
        block->minY = min[1];
    }
    if(max)
    {
        block->maxX = max[0];
        block->maxY = max[1];
    }
}

void GridmapBlock_SetCoordsXY(GridmapBlock* block, uint minX, uint minY, uint maxX, uint maxY)
{
    uint min[2], max[2];
    min[0] = minX;
    min[1] = minY;
    max[0] = maxX;
    max[1] = maxY;
    GridmapBlock_SetCoords(block, min, max);
}

#define UNIT_WIDTH                     1
#define UNIT_HEIGHT                    1

static int drawCell(TreeCell* cell, void* parameters)
{
    vec2_t topLeft, bottomRight;

    V2_Set(topLeft, UNIT_WIDTH * cell->origin[X], UNIT_HEIGHT * cell->origin[Y]);
    V2_Set(bottomRight, UNIT_WIDTH  * (cell->origin[X] + cell->size),
                        UNIT_HEIGHT * (cell->origin[Y] + cell->size));

    glBegin(GL_LINE_LOOP);
        glVertex2fv((GLfloat*)topLeft);
        glVertex2f(bottomRight[X], topLeft[Y]);
        glVertex2fv((GLfloat*)bottomRight);
        glVertex2f(topLeft[X], bottomRight[Y]);
    glEnd();
    return 0; // Continue iteration.
}

void Gridmap_DebugDrawer(Gridmap* gm)
{
    GLfloat oldColor[4];
    vec2_t start, end;
    assert(gm);

    // We'll be changing the color, so query the current and restore later.
    glGetFloatv(GL_CURRENT_COLOR, oldColor);

    /**
     * Draw our Quadtree.
     */
    glColor4f(1.f, 1.f, 1.f, 1.f / gm->root.size);
    iterateCell(&gm->root, false/*all cells*/, drawCell, NULL/*no parameters*/);

    /**
     * Draw our bounds.
     */
    V2_Set(start, 0, 0);
    V2_Set(end, UNIT_WIDTH * gm->dimensions[X], UNIT_HEIGHT * gm->dimensions[Y]);

    glColor3f(1, .5f, .5f);
    glBegin(GL_LINES);
        glVertex2f(start[X], start[Y]);
        glVertex2f(  end[X], start[Y]);

        glVertex2f(  end[X], start[Y]);
        glVertex2f(  end[X],   end[Y]);

        glVertex2f(  end[X],   end[Y]);
        glVertex2f(start[X],   end[Y]);

        glVertex2f(start[X],   end[Y]);
        glVertex2f(start[X], start[Y]);
    glEnd();

    // Restore GL state.
    glColor4fv(oldColor);
}

#undef UNIT_HEIGHT
#undef UNIT_WIDTH
