/* $Id$ */
#ifndef __MD2TOOL_H__

#define MD2TOOL_VERSION     "1.2.0"
#define PI                  3.14159265f

enum { VX, VY, VZ };

// Error codes.
enum
{
    MTERR_INVALID_OPTION,
    MTERR_BAD_MAGIC,
    MTERR_INVALID_SKIN_NUMBER,
    MTERR_INVALID_FRAME_NUMBER,
    MTERR_NO_FILES,
    MTERR_LISTFILE_NA
};

#define MD2_MAGIC           0x32504449
#define NUMVERTEXNORMALS    162

#define MAX_TRIANGLES       4096
#define MAX_VERTS           2048
#define MAX_FRAMES          512
#define MAX_MD2SKINS        32
#define MAX_SKINNAME        64

// "DMDM" = Doomsday/Detailed MoDel Magic
#define DMD_MAGIC           0x4D444D44
#define MAX_LODS            4

typedef unsigned char byte;
typedef unsigned int boolean;

#define true 1
#define false 0

typedef struct {
    float pos[3];
} vector_t;

typedef struct {
    int magic;
    int version;
    int skinWidth;
    int skinHeight;
    int frameSize;
    int numSkins;
    int numVertices;
    int numTexCoords;
    int numTriangles;
    int numGlCommands;
    int numFrames;
    int offsetSkins;
    int offsetTexCoords;
    int offsetTriangles;
    int offsetFrames;
    int offsetGlCommands;
    int offsetEnd;
} md2_header_t;

typedef struct {
    byte vertex[3];
    byte lightNormalIndex;
} md2_vertex_t;

typedef struct {
    float scale[3];
    float translate[3];
    char name[16];
    md2_vertex_t vertices[1];
} md2_frame_t;

#define MD2_FRAMESIZE(vtxcount) ((int)sizeof(md2_frame_t) + \
  (int)sizeof(md2_vertex_t) * ((vtxcount)-1))

typedef struct {
    char name[64];
} md2_skin_t;

typedef struct {
    short vertexIndices[3];
    short textureIndices[3];
} md2_triangle_t;

typedef struct {
    short s, t;
} md2_textureCoordinate_t;

typedef struct {
    float s, t;
    int vertexIndex;
} md2_glCommandVertex_t;

#if 0
typedef struct {
    char            fileName[256];  /* Name of the md2 file. */
    md2_header_t    header;         /* A copy of the header. */
    md2_skin_t      *skins;
    md2_textureCoordinate_t *texCoords;
    md2_triangle_t  *triangles;
    md2_frame_t     *frames;
    int             *glCommands;
} model_t;
#endif

typedef struct {
    short   index_xyz[3];
    short   index_st[3];
} dtriangle_t;


// DMD ----------------------------------------------------------------------

typedef struct {
    int magic;
    int version;
    int flags;
} dmd_header_t;

// Chunk types.
enum
{
    DMC_END,    // Must be the last chunk.
    DMC_INFO    // Required; will be expected to exist.
};

typedef struct {
    int type;
    int length; // Next chunk follows...
} dmd_chunk_t;

typedef struct { // DMC_INFO
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

#pragma pack(1)
typedef struct {
    byte vertex[3];
    unsigned short normal;          // Yaw and pitch.
} dmd_vertex_t;

typedef struct {
    float scale[3];
    float translate[3];
    char name[16];
    dmd_vertex_t vertices[1];
} dmd_frame_t;

#define DMD_FRAMESIZE(vtxcount) ((int)sizeof(dmd_frame_t) + \
  (int)sizeof(dmd_vertex_t) * ((vtxcount)-1))
#pragma pack()

typedef struct {
    char name[64];
} dmd_skin_t;

typedef struct {
    short vertexIndices[3];
    short textureIndices[3];
} dmd_triangle_t;

typedef struct {
    short s, t;
} dmd_textureCoordinate_t;

typedef struct {
    float s, t;
    int vertexIndex;
} dmd_glCommandVertex_t;

typedef struct {
    dmd_triangle_t  *triangles;
    int             *glCommands;
} dmodel_lod_t;

typedef struct {
    char            fileName[256];  // Name of the dmd/md2 file.
    int             modified;
    dmd_header_t    header;
    dmd_info_t      info;
    dmd_levelOfDetail_t lodinfo[MAX_LODS];
    dmd_skin_t      *skins;
    dmd_textureCoordinate_t *texCoords;
    dmd_frame_t     *frames;
    dmodel_lod_t    lods[MAX_LODS];
} model_t;

typedef struct {
    dtriangle_t     tri;
    int             group;
} optriangle_t;

#endif
