/** @file models.cpp  3D model resource loading.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "de_platform.h"
#include "resource/models.h"

#include "dd_main.h" // App_FileSystem()
#include "con_bar.h"
#include "con_main.h"
#include "def_main.h"

#include "filesys/fs_main.h"

#ifdef __CLIENT__
// For smart caching logics:
#  include "MaterialSnapshot"

#  include "render/billboard.h" // Rend_SpriteMaterialSpec()
#  include "render/r_main.h" // frameTimePos, precacheSkins
#  include "render/rend_model.h"
#endif

#include "world/p_object.h"

#include <de/App>

#include <de/mathutil.h> // M_CycleIntoRange()
#include <de/memory.h>
#include <cmath>

using namespace de;

static Model *loadModel(String path);
static bool recogniseDmd(de::FileHandle &file);
static bool recogniseMd2(de::FileHandle &file);
static void loadDmd(de::FileHandle &file, Model &mdl);
static void loadMd2(de::FileHandle &file, Model &mdl);

#define NUMVERTEXNORMALS 162
static float avertexnormals[NUMVERTEXNORMALS][3] = {
#include "tab_anorms.h"
};

/**
 * Calculate vertex normals. Only with -renorm.
 */
#if 0 // unused atm.
static void rebuildNormals(model_t &mdl)
{
    // Renormalizing?
    if(!CommandLine_Check("-renorm")) return;

    int const tris  = mdl.lodInfo[0].numTriangles;
    int const verts = mdl.info.numVertices;

    vector_t* normals = (vector_t*) Z_Malloc(sizeof(vector_t) * tris, PU_APPSTATIC, 0);
    vector_t norm;
    int cnt;

    // Calculate the normal for each vertex.
    for(int i = 0; i < mdl.info.numFrames; ++i)
    {
        model_vertex_t* list = mdl.frames[i].vertices;

        for(int k = 0; k < tris; ++k)
        {
            dmd_triangle_t const& tri = mdl.lods[0].triangles[k];

            // First calculate surface normals, combine them to vertex ones.
            V3f_PointCrossProduct(normals[k].pos,
                                  list[tri.vertexIndices[0]].vertex,
                                  list[tri.vertexIndices[2]].vertex,
                                  list[tri.vertexIndices[1]].vertex);
            V3f_Normalize(normals[k].pos);
        }

        for(int k = 0; k < verts; ++k)
        {
            memset(&norm, 0, sizeof(norm));
            cnt = 0;

            for(int j = 0; j < tris; ++j)
            {
                dmd_triangle_t const& tri = mdl.lods[0].triangles[j];

                for(int n = 0; n < 3; ++n)
                {
                    if(tri.vertexIndices[n] == k)
                    {
                        cnt++;
                        for(int n = 0; n < 3; ++n)
                        {
                            norm.pos[n] += normals[j].pos[n];
                        }
                        break;
                    }
                }
            }

            if(!cnt) continue; // Impossible...

            // Calculate the average.
            for(int n = 0; n < 3; ++n)
            {
                norm.pos[n] /= cnt;
            }

            // Normalize it.
            V3f_Normalize(norm.pos);
            memcpy(list[k].normal, norm.pos, sizeof(norm.pos));
        }
    }

    Z_Free(normals);
}
#endif

static void defineAllSkins(Model &mdl)
{
    String const &modelFilePath = findModelPath(mdl.modelId());

    int numFoundSkins = 0;
    for(int i = 0; i < mdl.skinCount(); ++i)
    {
        ModelSkin &skin = mdl.skin(i);

        if(skin.name.isEmpty())
            continue;

        try
        {
            de::Uri foundResourceUri(Path(findSkinPath(skin.name, modelFilePath)));

            skin.texture = App_ResourceSystem().defineTexture("ModelSkins", foundResourceUri);

            // We have found one more skin for this model.
            numFoundSkins += 1;
        }
        catch(FS1::NotFoundError const&)
        {
            LOG_WARNING("Failed to locate \"%s\" (#%i) for model \"%s\", ignoring.")
                << skin.name << i << NativePath(modelFilePath).pretty();
        }
    }

    if(!numFoundSkins)
    {
        // Lastly try a skin named similarly to the model in the same directory.
        de::Uri searchPath(modelFilePath.fileNamePath() / modelFilePath.fileNameWithoutExtension(), RC_GRAPHIC);

        try
        {
            String foundPath = App_FileSystem().findPath(searchPath, RLF_DEFAULT,
                                                         App_ResourceClass(RC_GRAPHIC));
            // Ensure the found path is absolute.
            foundPath = App_BasePath() / foundPath;

            defineSkinAndAddToModelIndex(mdl, foundPath);
            // We have found one more skin for this model.
            numFoundSkins = 1;

            LOG_INFO("Assigned fallback skin \"%s\" to index #0 for model \"%s\".")
                << NativePath(foundPath).pretty()
                << NativePath(modelFilePath).pretty();
        }
        catch(FS1::NotFoundError const&)
        {} // Ignore this error.
    }

    if(!numFoundSkins)
    {
        LOG_WARNING("Failed to locate a skin for model \"%s\". This model will be rendered without a skin.")
            << NativePath(modelFilePath).pretty();
    }
}

static Model *interpretDmd(de::FileHandle &hndl, String path, modelid_t modelId)
{
    if(recogniseDmd(hndl))
    {
        LOG_VERBOSE("Interpreted \"" + NativePath(path).pretty() + "\" as a DMD model.");
        Model *mdl = modelForId(modelId, true/*create*/);
        loadDmd(hndl, *mdl);
        return mdl;
    }
    return 0;
}

static Model *interpretMd2(de::FileHandle &hndl, String path, modelid_t modelId)
{
    if(recogniseMd2(hndl))
    {
        LOG_VERBOSE("Interpreted \"" + NativePath(path).pretty() + "\" as a MD2 model.");
        Model *mdl = modelForId(modelId, true/*create*/);
        loadMd2(hndl, *mdl);
        return mdl;
    }
    return 0;
}

struct ModelFileType
{
    /// Symbolic name of the resource type.
    String const name;

    /// Known file extension.
    String const ext;

    Model *(*interpretFunc)(de::FileHandle &hndl, String path, modelid_t modelId);
};

// Model resource types.
static ModelFileType const modelTypes[] = {
    { "DMD",    ".dmd",     interpretDmd },
    { "MD2",    ".md2",     interpretMd2 },
    { "",       "",         0 } // Terminate.
};

static ModelFileType const *guessModelFileTypeFromFileName(String filePath)
{
    // An extension is required for this.
    String ext = filePath.fileNameExtension();
    if(!ext.isEmpty())
    {
        for(int i = 0; !modelTypes[i].name.isEmpty(); ++i)
        {
            ModelFileType const &type = modelTypes[i];
            if(!type.ext.compareWithoutCase(ext))
            {
                return &type;
            }
        }
    }
    return 0; // Unknown.
}

static Model *interpretModel(de::FileHandle &hndl, String path, modelid_t modelId)
{
    // Firstly try the interpreter for the guessed resource types.
    ModelFileType const *rtypeGuess = guessModelFileTypeFromFileName(path);
    if(rtypeGuess)
    {
        if(Model *mdl = rtypeGuess->interpretFunc(hndl, path, modelId))
            return mdl;
    }

    // Not yet interpreted - try each recognisable format in order.
    // Try each recognisable format instead.
    for(int i = 0; !modelTypes[i].name.isEmpty(); ++i)
    {
        ModelFileType const &modelType = modelTypes[i];

        // Already tried this?
        if(&modelType == rtypeGuess) continue;

        if(Model *mdl = modelType.interpretFunc(hndl, path, modelId))
            return mdl;
    }

    return 0;
}

/**
 * Finds the existing model or loads in a new one.
 */
static Model *loadModel(String path)
{
    // Have we already loaded this?
    modelid_t modelId = modelRepository->intern(path);
    Model *mdl = Models_Model(modelId);
    if(mdl) return mdl; // Yes.

    try
    {
        // Attempt to interpret and load this model file.
        QScopedPointer<de::FileHandle> hndl(&App_FileSystem().openFile(path, "rb"));

        mdl = interpretModel(*hndl, path, modelId);

        // We're done with the file.
        App_FileSystem().releaseFile(hndl->file());

        // Loaded?
        if(mdl)
        {
            defineAllSkins(*mdl);

            // Enlarge the vertex buffers in preparation for drawing of this model.
            if(!Rend_ModelExpandVertexBuffers(mdl->vertexCount()))
            {
                LOG_WARNING("Model \"%s\" contains more than %u max vertices (%i), it will not be rendered.")
                    << NativePath(path).pretty()
                    << uint(RENDER_MAX_MODEL_VERTS) << mdl->vertexCount();
            }

            return mdl;
        }
    }
    catch(FS1::NotFoundError const &er)
    {
        LOG_WARNING(er.asText() + ", ignoring.");
    }

    return 0;
}

static void *allocAndLoad(de::FileHandle &file, int offset, int len)
{
    uint8_t *ptr = (uint8_t *) M_Malloc(len);
    file.seek(offset, SeekSet);
    file.read(ptr, len);
    return ptr;
}

//
#define MD2_MAGIC 0x32504449

#pragma pack(1)
struct md2_header_t
{
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
};
#pragma pack()

static bool readMd2Header(de::FileHandle &file, md2_header_t &hdr)
{
    size_t readBytes = file.read((uint8_t *)&hdr, sizeof(md2_header_t));
    if(readBytes < sizeof(md2_header_t)) return false;

    hdr.magic            = littleEndianByteOrder.toNative(hdr.magic);
    hdr.version          = littleEndianByteOrder.toNative(hdr.version);
    hdr.skinWidth        = littleEndianByteOrder.toNative(hdr.skinWidth);
    hdr.skinHeight       = littleEndianByteOrder.toNative(hdr.skinHeight);
    hdr.frameSize        = littleEndianByteOrder.toNative(hdr.frameSize);
    hdr.numSkins         = littleEndianByteOrder.toNative(hdr.numSkins);
    hdr.numVertices      = littleEndianByteOrder.toNative(hdr.numVertices);
    hdr.numTexCoords     = littleEndianByteOrder.toNative(hdr.numTexCoords);
    hdr.numTriangles     = littleEndianByteOrder.toNative(hdr.numTriangles);
    hdr.numGlCommands    = littleEndianByteOrder.toNative(hdr.numGlCommands);
    hdr.numFrames        = littleEndianByteOrder.toNative(hdr.numFrames);
    hdr.offsetSkins      = littleEndianByteOrder.toNative(hdr.offsetSkins);
    hdr.offsetTexCoords  = littleEndianByteOrder.toNative(hdr.offsetTexCoords);
    hdr.offsetTriangles  = littleEndianByteOrder.toNative(hdr.offsetTriangles);
    hdr.offsetFrames     = littleEndianByteOrder.toNative(hdr.offsetFrames);
    hdr.offsetGlCommands = littleEndianByteOrder.toNative(hdr.offsetGlCommands);
    hdr.offsetEnd        = littleEndianByteOrder.toNative(hdr.offsetEnd);
    return true;
}

/// @todo We only really need to read the magic bytes and the version here.
static bool recogniseMd2(de::FileHandle &file)
{
    md2_header_t hdr;
    size_t initPos = file.tell();
    // Seek to the start of the header.
    file.seek(0, SeekSet);
    bool result = (readMd2Header(file, hdr) && LONG(hdr.magic) == MD2_MAGIC);
    // Return the stream to its original position.
    file.seek(initPos, SeekSet);
    return result;
}

#pragma pack(1)
struct md2_triangleVertex_t
{
    byte vertex[3];
    byte normalIndex;
};

struct md2_packedFrame_t
{
    float scale[3];
    float translate[3];
    char name[16];
    md2_triangleVertex_t vertices[1];
};

struct md2_commandElement_t {
    float s, t;
    int index;
};
#pragma pack()

/**
 * Note vertex Z/Y are swapped here (ordered XYZ in the serialized data).
 */
static void loadMd2(de::FileHandle &file, Model &mdl)
{
    // Read the header.
    md2_header_t hdr;
    bool readHeaderOk = readMd2Header(file, hdr);
    DENG2_ASSERT(readHeaderOk);
    DENG2_UNUSED(readHeaderOk); // should this be checked?

    mdl._numVertices = hdr.numVertices;

    mdl.clearAllFrames();

    // Load and convert to DMD.
    uint8_t *frameData = (uint8_t *) allocAndLoad(file, hdr.offsetFrames, hdr.frameSize * hdr.numFrames);
    for(int i = 0; i < hdr.numFrames; ++i)
    {
        md2_packedFrame_t const *pfr = (md2_packedFrame_t const *) (frameData + hdr.frameSize * i);
        Vector3f const scale(FLOAT(pfr->scale[0]), FLOAT(pfr->scale[2]), FLOAT(pfr->scale[1]));
        Vector3f const translation(FLOAT(pfr->translate[0]), FLOAT(pfr->translate[2]), FLOAT(pfr->translate[1]));
        String const frameName = pfr->name;

        ModelFrame *frame = new ModelFrame(mdl, frameName);
        frame->vertices.reserve(hdr.numVertices);

        // Scale and translate each vertex.
        md2_triangleVertex_t const *pVtx = pfr->vertices;
        for(int k = 0; k < hdr.numVertices; ++k, pVtx++)
        {
            frame->vertices.append(ModelFrame::Vertex());
            ModelFrame::Vertex &vtx = frame->vertices.last();

            vtx.pos = Vector3f(pVtx->vertex[0], pVtx->vertex[2], pVtx->vertex[1])
                          * scale + translation;
            vtx.pos.y *= modelAspectMod; // Aspect undoing.

            vtx.norm = Vector3f(avertexnormals[pVtx->normalIndex]);

            if(!k)
            {
                frame->min = frame->max = vtx.pos;
            }
            else
            {
                frame->min = vtx.pos.min(frame->min);
                frame->max = vtx.pos.max(frame->max);
            }
        }

        mdl.addFrame(frame); // takes owernship
    }
    M_Free(frameData);

    mdl._lods.append(new ModelDetailLevel(mdl, 0));
    ModelDetailLevel &lod0 = *mdl._lods.last();

    uint8_t *commandData = (uint8_t *) allocAndLoad(file, hdr.offsetGlCommands, 4 * hdr.numGlCommands);
    for(uint8_t const *pos = commandData; *pos;)
    {
        int count = LONG( *(int *) pos ); pos += 4;

        lod0.primitives.append(Model::Primitive());
        Model::Primitive &prim = lod0.primitives.last();

        // The type of primitive depends on the sign.
        prim.triFan = (count < 0);

        if(count < 0)
        {
            count = -count;
        }

        while(count--)
        {
            md2_commandElement_t const *v = (md2_commandElement_t *) pos; pos += 12;

            prim.elements.append(Model::Primitive::Element());
            Model::Primitive::Element &elem = prim.elements.last();
            elem.texCoord = Vector2f(FLOAT(v->s), FLOAT(v->t));
            elem.index    = LONG(v->index);
        }
    }
    M_Free(commandData);

    mdl.clearAllSkins();

    // Load skins. (Note: numSkins may be zero.)
    file.seek(hdr.offsetSkins, SeekSet);
    for(int i = 0; i < hdr.numSkins; ++i)
    {
        char name[64]; file.read((uint8_t *)name, 64);
        mdl.newSkin(name);
    }
}

//
#define DMD_MAGIC 0x4D444D44 ///< "DMDM" = Doomsday/Detailed MoDel Magic

#pragma pack(1)
struct dmd_header_t
{
    int magic;
    int version;
    int flags;
};
#pragma pack()

static bool readHeaderDmd(de::FileHandle &file, dmd_header_t &hdr)
{
    size_t readBytes = file.read((uint8_t *)&hdr, sizeof(dmd_header_t));
    if(readBytes < sizeof(dmd_header_t)) return false;

    hdr.magic   = littleEndianByteOrder.toNative(hdr.magic);
    hdr.version = littleEndianByteOrder.toNative(hdr.version);
    hdr.flags   = littleEndianByteOrder.toNative(hdr.flags);
    return true;
}

static bool recogniseDmd(de::FileHandle &file)
{
    dmd_header_t hdr;
    size_t initPos = file.tell();
    // Seek to the start of the header.
    file.seek(0, SeekSet);
    bool result = (readHeaderDmd(file, hdr) && LONG(hdr.magic) == DMD_MAGIC);
    // Return the stream to its original position.
    file.seek(initPos, SeekSet);
    return result;
}

// DMD chunk types.
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
    dmd_packedVertex_t vertices[1]; // dmd_info_t::numVertices size
} dmd_packedFrame_t;

typedef struct {
    short vertexIndices[3];
    short textureIndices[3];
} dmd_triangle_t;
#pragma pack()

/**
 * Packed: pppppppy yyyyyyyy. Yaw is on the XY plane.
 */
static Vector3f unpackVector(ushort packed)
{
    float const yaw   = (packed & 511) / 512.0f * 2 * PI;
    float const pitch = ((packed >> 9) / 127.0f - 0.5f) * PI;
    float const cosp  = float(cos(pitch));
    return Vector3f(cos(yaw) * cosp, sin(yaw) * cosp, sin(pitch));
}

/**
 * Note vertex Z/Y are swapped here (ordered XYZ in the serialized data).
 */
static void loadDmd(de::FileHandle &file, Model &mdl)
{
    // Read the header.
    dmd_header_t hdr;
    bool readHeaderOk = readHeaderDmd(file, hdr);
    DENG2_ASSERT(readHeaderOk);
    DENG2_UNUSED(readHeaderOk); // should this be checked?

    // Read the chunks.
    dmd_chunk_t chunk;
    file.read((uint8_t *)&chunk, sizeof(chunk));

    dmd_info_t info; zap(info);
    while(LONG(chunk.type) != DMC_END)
    {
        switch(LONG(chunk.type))
        {
        case DMC_INFO: // Standard DMD information chunk.
            file.read((uint8_t *)&info, LONG(chunk.length));

            info.skinWidth       = LONG(info.skinWidth);
            info.skinHeight      = LONG(info.skinHeight);
            info.frameSize       = LONG(info.frameSize);
            info.numSkins        = LONG(info.numSkins);
            info.numVertices     = LONG(info.numVertices);
            info.numTexCoords    = LONG(info.numTexCoords);
            info.numFrames       = LONG(info.numFrames);
            info.numLODs         = LONG(info.numLODs);
            info.offsetSkins     = LONG(info.offsetSkins);
            info.offsetTexCoords = LONG(info.offsetTexCoords);
            info.offsetFrames    = LONG(info.offsetFrames);
            info.offsetLODs      = LONG(info.offsetLODs);
            info.offsetEnd       = LONG(info.offsetEnd);
            break;

        default:
            // Skip unknown chunks.
            file.seek(LONG(chunk.length), SeekCur);
            break;
        }
        // Read the next chunk header.
        file.read((uint8_t *)&chunk, sizeof(chunk));
    }
    mdl._numVertices = info.numVertices;

    mdl.clearAllSkins();

    // Allocate and load in the data. (Note: numSkins may be zero.)
    file.seek(info.offsetSkins, SeekSet);
    for(int i = 0; i < info.numSkins; ++i)
    {
        char name[64]; file.read((uint8_t *)name, 64);
        mdl.newSkin(name);
    }

    mdl.clearAllFrames();

    uint8_t *frameData = (uint8_t *) allocAndLoad(file, info.offsetFrames, info.frameSize * info.numFrames);
    for(int i = 0; i < info.numFrames; ++i)
    {
        dmd_packedFrame_t const *pfr = (dmd_packedFrame_t *) (frameData + info.frameSize * i);
        Vector3f const scale(FLOAT(pfr->scale[0]), FLOAT(pfr->scale[2]), FLOAT(pfr->scale[1]));
        Vector3f const translation(FLOAT(pfr->translate[0]), FLOAT(pfr->translate[2]), FLOAT(pfr->translate[1]));
        String const frameName = pfr->name;

        ModelFrame *frame = new ModelFrame(mdl, frameName);
        frame->vertices.reserve(info.numVertices);

        // Scale and translate each vertex.
        dmd_packedVertex_t const *pVtx = pfr->vertices;
        for(int k = 0; k < info.numVertices; ++k, ++pVtx)
        {
            frame->vertices.append(ModelFrame::Vertex());
            ModelFrame::Vertex &vtx = frame->vertices.last();

            vtx.pos = Vector3f(pVtx->vertex[0], pVtx->vertex[2], pVtx->vertex[1])
                          * scale + translation;
            vtx.pos.y *= modelAspectMod; // Aspect undo.

            vtx.norm = unpackVector(USHORT(pVtx->normal));

            if(!k)
            {
                frame->min = frame->max = vtx.pos;
            }
            else
            {
                frame->min = vtx.pos.min(frame->min);
                frame->max = vtx.pos.max(frame->max);
            }
        }

        mdl.addFrame(frame);
    }
    M_Free(frameData);

    file.seek(info.offsetLODs, SeekSet);
    dmd_levelOfDetail_t *lodInfo = new dmd_levelOfDetail_t[info.numLODs];

    for(int i = 0; i < info.numLODs; ++i)
    {
        file.read((uint8_t *)&lodInfo[i], sizeof(dmd_levelOfDetail_t));

        lodInfo[i].numTriangles     = LONG(lodInfo[i].numTriangles);
        lodInfo[i].numGlCommands    = LONG(lodInfo[i].numGlCommands);
        lodInfo[i].offsetTriangles  = LONG(lodInfo[i].offsetTriangles);
        lodInfo[i].offsetGlCommands = LONG(lodInfo[i].offsetGlCommands);
    }

    dmd_triangle_t **triangles = new dmd_triangle_t*[info.numLODs];

    for(int i = 0; i < info.numLODs; ++i)
    {
        mdl._lods.append(new ModelDetailLevel(mdl, i));
        ModelDetailLevel &lod = *mdl._lods.last();

        triangles[i] = (dmd_triangle_t *) allocAndLoad(file, lodInfo[i].offsetTriangles,
                                                       sizeof(dmd_triangle_t) * lodInfo[i].numTriangles);

        uint8_t *commandData = (uint8_t *) allocAndLoad(file, lodInfo[i].offsetGlCommands,
                                                        4 * lodInfo[i].numGlCommands);
        for(uint8_t const *pos = commandData; *pos;)
        {
            int count = LONG( *(int *) pos ); pos += 4;

            lod.primitives.append(Model::Primitive());
            Model::Primitive &prim = lod.primitives.last();

            // The type of primitive depends on the sign of the element count.
            prim.triFan = (count < 0);

            if(count < 0)
            {
                count = -count;
            }

            while(count--)
            {
                md2_commandElement_t const *v = (md2_commandElement_t *) pos; pos += 12;

                prim.elements.append(Model::Primitive::Element());
                Model::Primitive::Element &elem = prim.elements.last();

                elem.texCoord = Vector2f(FLOAT(v->s), FLOAT(v->t));
                elem.index    = LONG(v->index);
            }
        }
        M_Free(commandData);
    }

    // Determine vertex usage at each LOD level.
    mdl._vertexUsage.resize(info.numVertices * info.numLODs);
    mdl._vertexUsage.fill(false);

    for(int i = 0; i < info.numLODs; ++i)
    for(int k = 0; k < lodInfo[i].numTriangles; ++k)
    for(int m = 0; m < 3; ++m)
    {
        int vertexIndex = SHORT(triangles[i][k].vertexIndices[m]);
        mdl._vertexUsage.setBit(vertexIndex * info.numLODs + i);
    }

    delete [] lodInfo;
    for(int i = 0; i < info.numLODs; ++i)
    {
        M_Free(triangles[i]);
    }
    delete [] triangles;
}
