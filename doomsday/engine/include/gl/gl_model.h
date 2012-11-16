/**\file gl_model.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * 3D Model Constants and Data Structures
 *
 * Supported model formats: MD2 and DMD2.
 */

#ifndef LIBDENG_MODEL_H
#define LIBDENG_MODEL_H

#include "dd_types.h"
#include "resource/r_data.h"

#define MD2_MAGIC           0x32504449
#define NUMVERTEXNORMALS    162
//#define MAX_MODELS          768

// "DMDM" = Doomsday/Detailed MoDel Magic
#define DMD_MAGIC           0x4D444D44
#define MAX_LODS            4

typedef struct {
    int             magic;
    int             version;
    int             skinWidth;
    int             skinHeight;
    int             frameSize;
    int             numSkins;
    int             numVertices;
    int             numTexCoords;
    int             numTriangles;
    int             numGlCommands;
    int             numFrames;
    int             offsetSkins;
    int             offsetTexCoords;
    int             offsetTriangles;
    int             offsetFrames;
    int             offsetGlCommands;
    int             offsetEnd;
} md2_header_t;

typedef struct {
    byte            vertex[3];
    byte            lightNormalIndex;
} md2_triangleVertex_t;

typedef struct {
    float           scale[3];
    float           translate[3];
    char            name[16];
    md2_triangleVertex_t vertices[1];
} md2_packedFrame_t;

typedef struct {
    float           vertex[3];
    byte            lightNormalIndex;
} md2_modelVertex_t;

// Translated frame (vertices in model space).
typedef struct {
    char            name[16];
    md2_modelVertex_t *vertices;
} md2_frame_t;

typedef struct {
    char            name[256];
    int             id;
} md2_skin_t;

typedef struct {
    short           vertexIndices[3];
    short           textureIndices[3];
} md2_triangle_t;

typedef struct {
    short           s, t;
} md2_textureCoordinate_t;

typedef struct {
    float           s, t;
    int             vertexIndex;
} md2_glCommandVertex_t;

//===========================================================================
// DMD (Detailed/Doomsday Models)
//===========================================================================

typedef struct {
    int magic;
    int version;
    int flags;
} dmd_header_t;

// Chunk types.
enum {
    DMC_END, /// Must be the last chunk.
    DMC_INFO /// Required; will be expected to exist.
};

#pragma pack(1)
typedef struct {
    int type;
    int length; /// Next chunk follows...
} dmd_chunk_t;

typedef struct {
    int skinWidth;
    int skinHeight;
    int frameSize;
    int numSkins;
    int numVertices;
    int numTexCoords;
    int numFrames;
    int numLODs;
    int offsetSkins;
    int offsetTexCoords;
    int offsetFrames;
    int offsetLODs;
    int offsetEnd;
} dmd_info_t;

typedef struct {
    int numTriangles;
    int numGlCommands;
    int offsetTriangles;
    int offsetGlCommands;
} dmd_levelOfDetail_t;

typedef struct {
    byte vertex[3];
    unsigned short normal; /// Yaw and pitch.
} dmd_packedVertex_t;

typedef struct {
    float scale[3];
    float translate[3];
    char name[16];
    dmd_packedVertex_t vertices[1];
} dmd_packedFrame_t;

typedef struct {
    char name[256];
    struct texture_s* texture;
} dmd_skin_t;

typedef struct {
    short vertexIndices[3];
    short textureIndices[3];
} dmd_triangle_t;

typedef struct {
    float s, t;
    int vertexIndex;
} dmd_glCommandVertex_t;

typedef struct {
    int* glCommands;
} dmd_lod_t;
#pragma pack()

typedef struct model_vertex_s {
    float xyz[3];
} model_vertex_t;

typedef struct model_frame_s {
    char name[16];
    model_vertex_t* vertices;
    model_vertex_t* normals;
    float min[3], max[3];
} model_frame_t;

typedef struct model_s {
    boolean loaded;
    uint model; ///< Id of the model in the repository.
    dmd_header_t header;
    dmd_info_t info;
    dmd_skin_t* skins;
    model_frame_t* frames;
    dmd_levelOfDetail_t lodInfo[MAX_LODS];
    dmd_lod_t lods[MAX_LODS];
    char* vertexUsage; ///< Bitfield for each vertex.
    boolean allowTexComp; ///< Allow texture compression with this.
} model_t;

//extern model_t* modellist[MAX_MODELS];
extern float avertexnormals[NUMVERTEXNORMALS][3];

#endif /* LIBDENG_MODEL_H */
