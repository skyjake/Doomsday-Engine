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
    GridmapCoord origin[2];

    /// Size of this cell in Gridmap space (width=height).
    GridmapCoord size;

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
    GridmapCoord dimensions[2];

    /// Zone memory tag used for both the Gridmap and user data.
    int zoneTag;

    /// Size of the memory block to be allocated for each leaf.
    size_t sizeOfCell;

    /// Root tree for our Quadtree. Allocated along with the Gridmap instance.
    TreeCell root;
};

/**
 * Initialize @a tree. Assumes that @a tree has not yet been initialized
 * (i.e., existing references to child cells will be overwritten).
 *
 * @param tree        TreeCell instance to be initialized.
 * @param x           X coordinate in Gridmap space.
 * @param y           Y coordinate in Gridmap space.
 * @param size        Size in Gridmap space units.
 *
 * @return  Same as @a tree for caller convenience.
 */
static TreeCell* initCell(TreeCell* tree, GridmapCoord x, GridmapCoord y, GridmapCoord size)
{
    assert(tree);
    tree->origin[X] = x;
    tree->origin[Y] = y;
    tree->size = size;
    tree->userData = NULL;
    memset(tree->children, 0, sizeof tree->children);
    return tree;
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
static TreeCell* newCell(GridmapCoord x, GridmapCoord y, GridmapCoord size, int zoneTag)
{
    TreeCell* tree = Z_Malloc(sizeof *tree, zoneTag, NULL);
    if(!tree) Con_Error("Gridmap::newCell: Failed on allocation of %lu bytes for new Cell.", (unsigned long) sizeof *tree);
    return initCell(tree, x, y, size);
}

static void deleteCell(TreeCell* tree)
{
    assert(tree);
    // Deletion is a depth-first traversal.
    if(tree->children[TOPLEFT])     deleteCell(tree->children[TOPLEFT]);
    if(tree->children[TOPRIGHT])    deleteCell(tree->children[TOPRIGHT]);
    if(tree->children[BOTTOMLEFT])  deleteCell(tree->children[BOTTOMLEFT]);
    if(tree->children[BOTTOMRIGHT]) deleteCell(tree->children[BOTTOMRIGHT]);
    if(tree->userData) Z_Free(tree->userData);
    Z_Free(tree);
}

/// @return  @c true= @a tree is a a leaf (i.e., equal to a unit in Gridmap space).
static boolean isLeaf(TreeCell* tree)
{
    assert(tree);
    return tree->size == 1;
}

/**
 * Depth-first traversal of the children of this tree, making a callback
 * for each cell. Iteration ends when all selected cells have been visited
 * or a callback returns a non-zero value.
 *
 * @param tree          TreeCell to traverse.
 * @param leafOnly      Caller is only interested in leaves.
 * @param callback      Callback function.
 * @param parameters    Passed to the callback.
 *
 * @return  Zero iff iteration completed wholly, else the value returned by the
 *          last callback made.
 */
static int iterateCell(TreeCell* tree, boolean leafOnly,
    int (C_DECL *callback) (TreeCell* tree, void* parameters), void* parameters)
{
    int result = false; // Continue traversal.
    assert(tree && callback);

    if(!isLeaf(tree))
    {
        if(tree->children[TOPLEFT])
        {
            result = iterateCell(tree->children[TOPLEFT], leafOnly, callback, parameters);
            if(result) return result;
        }
        if(tree->children[TOPRIGHT])
        {
            result = iterateCell(tree->children[TOPRIGHT], leafOnly, callback, parameters);
            if(result) return result;
        }
        if(tree->children[BOTTOMLEFT])
        {
            result = iterateCell(tree->children[BOTTOMLEFT], leafOnly, callback, parameters);
            if(result) return result;
        }
        if(tree->children[BOTTOMRIGHT])
        {
            result = iterateCell(tree->children[BOTTOMRIGHT], leafOnly, callback, parameters);
            if(result) return result;
        }
    }
    if(!leafOnly || isLeaf(tree))
    {
        result = callback(tree, parameters);
    }
    return result;
}

static void deleteTree(Gridmap* gm)
{
    assert(gm);
    // The root tree is allocated along with Gridmap.
    if(gm->root.children[TOPLEFT])     deleteCell(gm->root.children[TOPLEFT]);
    if(gm->root.children[TOPRIGHT])    deleteCell(gm->root.children[TOPRIGHT]);
    if(gm->root.children[BOTTOMLEFT])  deleteCell(gm->root.children[BOTTOMLEFT]);
    if(gm->root.children[BOTTOMRIGHT]) deleteCell(gm->root.children[BOTTOMRIGHT]);
    if(gm->root.userData) Z_Free(gm->root.userData);
}

static TreeCell* findLeafDescend(Gridmap* gm, TreeCell* tree, GridmapCoord x, GridmapCoord y, boolean alloc)
{
    Quadrant q;
    assert(tree);

    if(isLeaf(tree))
    {
        return tree;
    }

    // Into which quadrant do we need to descend?
    if(x < tree->origin[X] + (tree->size >> 1))
    {
        q = (y < tree->origin[Y] + (tree->size >> 1))? TOPLEFT  : BOTTOMLEFT;
    }
    else
    {
        q = (y < tree->origin[Y] + (tree->size >> 1))? TOPRIGHT : BOTTOMRIGHT;
    }

    // Has this quadrant been initialized yet?
    if(!tree->children[q])
    {
        GridmapCoord subOrigin[2], subSize;

        // Are we allocating cells?
        if(!alloc) return NULL;

        // Subdivide this tree and construct the new.
        subSize = tree->size >> 1;
        switch(q)
        {
        case TOPLEFT:
            subOrigin[X] = tree->origin[X];
            subOrigin[Y] = tree->origin[Y];
            break;
        case TOPRIGHT:
            subOrigin[X] = tree->origin[X] + subSize;
            subOrigin[Y] = tree->origin[Y];
            break;
        case BOTTOMLEFT:
            subOrigin[X] = tree->origin[X];
            subOrigin[Y] = tree->origin[Y] + subSize;
            break;
        case BOTTOMRIGHT:
            subOrigin[X] = tree->origin[X] + subSize;
            subOrigin[Y] = tree->origin[Y] + subSize;
            break;
        default:
            Con_Error("Gridmap::findUserDataAdr: Invalid quadrant %i.", (int) q);
            exit(1); // Unreachable.
        }
        tree->children[q] = newCell(subOrigin[X], subOrigin[Y], subSize, gm->zoneTag);
    }

    return findLeafDescend(gm, tree->children[q], x, y, alloc);
}

static TreeCell* findLeaf(Gridmap* gm, GridmapCoord x, GridmapCoord y, boolean alloc)
{
    assert(gm);
    return findLeafDescend(gm, &gm->root, x, y, alloc);
}

static GridmapCoord ceilPow2(GridmapCoord unit)
{
    GridmapCoord cumul;
    for(cumul = 1; unit > cumul; cumul <<= 1);
    return cumul;
}

Gridmap* Gridmap_New(GridmapCoord width, GridmapCoord height, size_t cellSize, int zoneTag)
{
    Gridmap* gm = Z_Calloc(sizeof *gm, zoneTag, 0);
    GridmapCoord size;
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

GridmapCoord Gridmap_Width(const Gridmap* gm)
{
    assert(gm);
    return gm->dimensions[X];
}

GridmapCoord Gridmap_Height(const Gridmap* gm)
{
    assert(gm);
    return gm->dimensions[Y];
}

void Gridmap_Size(const Gridmap* gm, GridmapCoord widthHeight[])
{
    assert(gm);
    if(!widthHeight) return;
    widthHeight[X] = gm->dimensions[X];
    widthHeight[Y] = gm->dimensions[Y];
}

void* Gridmap_Cell(Gridmap* gm, const_GridmapCell cell, boolean alloc)
{
    TreeCell* tree;
    assert(gm);

    // Outside our boundary?
    if(cell[X] >= gm->dimensions[X] || cell[Y] >= gm->dimensions[Y]) return NULL;

    // Try to locate this leaf (may fail if not present and we are
    // not allocating user data (there will be no corresponding cell)).
    tree = findLeaf(gm, cell[X], cell[Y], alloc);
    if(!tree) return NULL;

    // Exisiting user data for this cell?
    if(tree->userData) return tree->userData;

    // Allocate new user data?
    if(!alloc) return NULL;
    return tree->userData = Z_Calloc(gm->sizeOfCell, gm->zoneTag, 0);
}

void* Gridmap_CellXY(Gridmap* gm, GridmapCoord x, GridmapCoord y, boolean alloc)
{
    GridmapCell cell;
    cell[X] = x;
    cell[Y] = y;
    return Gridmap_Cell(gm, cell, alloc);
}

typedef struct {
    Gridmap_IterateCallback callback;
    void* callbackParamaters;
} actioncallback_paramaters_t;

/**
 * Callback actioner. Executes the callback and then returns the result
 * to the current iteration to determine if it should continue.
 */
static int actionCallback(TreeCell* tree, void* parameters)
{
    actioncallback_paramaters_t* p = (actioncallback_paramaters_t*) parameters;
    assert(tree && p);
    if(tree->userData)
        return p->callback(tree->userData, p->callbackParamaters);
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

int Gridmap_BlockIterate2(Gridmap* gm, const GridmapCellBlock* block_,
    Gridmap_IterateCallback callback, void* parameters)
{
    GridmapCellBlock block;
    TreeCell* tree;
    GridmapCoord x, y;
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
        tree = findLeaf(gm, x, y, false);
        if(!tree || !tree->userData) continue;

        result = callback(tree->userData, parameters);
        if(result) return result;
    }
    return false;
}

int Gridmap_BlockIterate(Gridmap* gm, const GridmapCellBlock* block,
    Gridmap_IterateCallback callback)
{
    return Gridmap_BlockIterate2(gm, block, callback, NULL/*no parameters*/);
}

int Gridmap_BlockXYIterate2(Gridmap* gm, GridmapCoord minX, GridmapCoord minY,
    GridmapCoord maxX, GridmapCoord maxY, Gridmap_IterateCallback callback, void* parameters)
{
    GridmapCellBlock block;
    GridmapBlock_SetCoordsXY(&block, minX, maxX, minY, maxY);
    return Gridmap_BlockIterate2(gm, &block, callback, parameters);
}

int Gridmap_BlockXYIterate(Gridmap* gm, GridmapCoord minX, GridmapCoord minY,
    GridmapCoord maxX, GridmapCoord maxY, Gridmap_IterateCallback callback)
{
    return Gridmap_BlockXYIterate2(gm, minX, minY, maxX, maxY, callback, NULL/*no parameters*/);
}

boolean Gridmap_ClipBlock(Gridmap* gm, GridmapCellBlock* block)
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

void GridmapBlock_SetCoords(GridmapCellBlock* block, const_GridmapCell min, const_GridmapCell max)
{
    assert(block);
    if(min)
    {
        block->minX = min[X];
        block->minY = min[Y];
    }
    if(max)
    {
        block->maxX = max[X];
        block->maxY = max[Y];
    }
}

void GridmapBlock_SetCoordsXY(GridmapCellBlock* block, GridmapCoord minX, GridmapCoord minY,
    GridmapCoord maxX, GridmapCoord maxY)
{
    GridmapCoord min[2], max[2];
    min[X] = minX;
    min[Y] = minY;
    max[X] = maxX;
    max[Y] = maxY;
    GridmapBlock_SetCoords(block, min, max);
}

#define UNIT_WIDTH                     1
#define UNIT_HEIGHT                    1

static int drawCell(TreeCell* tree, void* parameters)
{
    vec2f_t topLeft, bottomRight;

    V2f_Set(topLeft, UNIT_WIDTH * tree->origin[X], UNIT_HEIGHT * tree->origin[Y]);
    V2f_Set(bottomRight, UNIT_WIDTH  * (tree->origin[X] + tree->size),
                         UNIT_HEIGHT * (tree->origin[Y] + tree->size));

    glBegin(GL_LINE_LOOP);
        glVertex2fv((GLfloat*)topLeft);
        glVertex2f(bottomRight[X], topLeft[Y]);
        glVertex2fv((GLfloat*)bottomRight);
        glVertex2f(topLeft[X], bottomRight[Y]);
    glEnd();
    return 0; // Continue iteration.
}

void Gridmap_DebugDrawer(const Gridmap* gm)
{
    GLfloat oldColor[4];
    vec2f_t start, end;
    assert(gm);

    // We'll be changing the color, so query the current and restore later.
    glGetFloatv(GL_CURRENT_COLOR, oldColor);

    /**
     * Draw our Quadtree.
     */
    glColor4f(1.f, 1.f, 1.f, 1.f / gm->root.size);
    iterateCell(&((Gridmap*)gm)->root, false/*all cells*/, drawCell, NULL/*no parameters*/);

    /**
     * Draw our bounds.
     */
    V2f_Set(start, 0, 0);
    V2f_Set(end, UNIT_WIDTH * gm->dimensions[X], UNIT_HEIGHT * gm->dimensions[Y]);

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
