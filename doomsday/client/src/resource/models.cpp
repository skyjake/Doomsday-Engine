/** @file models.cpp 3D Model Resources.
 *
 * MD2/DMD2 loading and setup.
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include <QDir>
#include <de/App>
#include <de/ByteOrder>
#include <de/NativePath>
#include <de/StringPool>
#include <de/mathutil.h> // for M_CycleIntoRange()
#include <de/memory.h>

#include "de_platform.h"

#include <cmath>
#include <cstring> // memset

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"
#include "de_graphics.h"
#include "de_play.h"
#include "de_resource.h"

#include "def_main.h"

#include "MaterialSnapshot"

#include "render/r_things.h"
#include "render/rend_model.h"
#include "render/sprite.h"

using namespace de;

modeldef_t* modefs;
int numModelDefs;

byte useModels = true;

float rModelAspectMod = 1 / 1.2f; //.833334f;

static StringPool* modelRepository; // Owns model_t instances.

static int maxModelDefs;
static modeldef_t** stateModefs;

static float avertexnormals[NUMVERTEXNORMALS][3] = {
#include "tab_anorms.h"
};

/**
 * Packed: pppppppy yyyyyyyy. Yaw is on the XY plane.
 */
static void unpackVector(ushort packed, float vec[3])
{
    float const yaw   = (packed & 511) / 512.0f * 2 * PI;
    float const pitch = ((packed >> 9) / 127.0f - 0.5f) * PI;
    float const cosp  = float(cos(pitch));

    vec[VX] = float( cos(yaw) * cosp );
    vec[VY] = float( sin(yaw) * cosp );
    vec[VZ] = float( sin(pitch) );
}

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

static void* allocAndLoad(de::FileHandle& file, int offset, int len)
{
    uint8_t* ptr = (uint8_t*) M_Malloc(len);
    if(!ptr) throw Error("allocAndLoad:", String("Failed on allocation of %1 bytes for load buffer").arg(len));
    file.seek(offset, SeekSet);
    file.read(ptr, len);
    return ptr;
}

static bool readMd2Header(de::FileHandle& file, md2_header_t& hdr)
{
    size_t readBytes = file.read((uint8_t*)&hdr, sizeof(md2_header_t));
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
static bool recogniseMd2(de::FileHandle& file)
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

static void loadMd2(de::FileHandle& file, model_t& mdl)
{
    int const axis[3] = { 0, 2, 1 };

    md2_header_t oldHdr;
    dmd_header_t& hdr = mdl.header;
    dmd_info_t&   inf = mdl.info;

    // Read the header.
    bool readHeaderOk = readMd2Header(file, oldHdr);
    DENG_ASSERT(readHeaderOk);
    DENG_UNUSED(readHeaderOk); // should this be checked?

    // Convert it to DMD.
    hdr.magic = MD2_MAGIC;
    hdr.version = 8;
    hdr.flags = 0;
    mdl.vertexUsage = NULL;

    inf.skinWidth       = oldHdr.skinWidth;
    inf.skinHeight      = oldHdr.skinHeight;
    inf.frameSize       = oldHdr.frameSize;
    inf.numSkins        = oldHdr.numSkins;
    inf.numTexCoords    = oldHdr.numTexCoords;
    inf.numVertices     = oldHdr.numVertices;
    inf.numFrames       = oldHdr.numFrames;
    inf.offsetSkins     = oldHdr.offsetSkins;
    inf.offsetTexCoords = oldHdr.offsetTexCoords;
    inf.offsetFrames    = oldHdr.offsetFrames;
    inf.offsetLODs      = oldHdr.offsetEnd;    // Doesn't exist.
    inf.numLODs         = 1;

    mdl.lodInfo[0].numTriangles     = oldHdr.numTriangles;
    mdl.lodInfo[0].numGlCommands    = oldHdr.numGlCommands;
    mdl.lodInfo[0].offsetTriangles  = oldHdr.offsetTriangles;
    mdl.lodInfo[0].offsetGlCommands = oldHdr.offsetGlCommands;
    inf.offsetEnd = oldHdr.offsetEnd;

    // The frames need to be unpacked.
    byte* frames = (byte*) allocAndLoad(file, inf.offsetFrames, inf.frameSize * inf.numFrames);

    mdl.frames = (model_frame_t*) M_Malloc(sizeof(model_frame_t) * inf.numFrames);

    model_frame_t* frame = mdl.frames;
    for(int i = 0; i < inf.numFrames; ++i, ++frame)
    {
        md2_packedFrame_t* pfr = (md2_packedFrame_t*) (frames + inf.frameSize * i);

        memcpy(frame->name, pfr->name, sizeof(pfr->name));
        frame->vertices = (model_vertex_t*) M_Malloc(sizeof(model_vertex_t) * inf.numVertices);
        frame->normals  = (model_vertex_t*) M_Malloc(sizeof(model_vertex_t) * inf.numVertices);

        // Translate each vertex.
        md2_triangleVertex_t* pVtx = pfr->vertices;
        for(int k = 0; k < inf.numVertices; ++k, pVtx++)
        {
            byte const lightNormalIndex = pVtx->lightNormalIndex;

            memcpy(frame->normals[k].xyz, avertexnormals[lightNormalIndex], sizeof(float) * 3);

            for(int c = 0; c < 3; ++c)
            {
                frame->vertices[k].xyz[axis[c]] =
                    pVtx->vertex[c] * FLOAT(pfr->scale[c]) + FLOAT(pfr->translate[c]);
            }

            // Aspect undoing.
            frame->vertices[k].xyz[VY] *= rModelAspectMod;

            for(int c = 0; c < 3; ++c)
            {
                if(!k || frame->vertices[k].xyz[c] < frame->min[c])
                    frame->min[c] = frame->vertices[k].xyz[c];

                if(!k || frame->vertices[k].xyz[c] > frame->max[c])
                    frame->max[c] = frame->vertices[k].xyz[c];
            }
        }
    }
    M_Free(frames);

    mdl.lods[0].glCommands = (int*) allocAndLoad(file, mdl.lodInfo[0].offsetGlCommands,
                                                 sizeof(int) * mdl.lodInfo[0].numGlCommands);

    // Load skins. (Note: numSkins may be zero.)
    mdl.skins = (dmd_skin_t*) M_Calloc(sizeof(*mdl.skins) * inf.numSkins);
    if(!mdl.skins) throw std::bad_alloc();

    file.seek(inf.offsetSkins, SeekSet);
    for(int i = 0; i < inf.numSkins; ++i)
    {
        file.read((uint8_t*)mdl.skins[i].name, 64);
    }
}

static bool readHeaderDmd(de::FileHandle& file, dmd_header_t& hdr)
{
    size_t readBytes = file.read((uint8_t*)&hdr, sizeof(dmd_header_t));
    if(readBytes < sizeof(dmd_header_t)) return false;

    hdr.magic   = littleEndianByteOrder.toNative(hdr.magic);
    hdr.version = littleEndianByteOrder.toNative(hdr.version);
    hdr.flags   = littleEndianByteOrder.toNative(hdr.flags);
    return true;
}

static bool recogniseDmd(de::FileHandle& file)
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

static void loadDmd(de::FileHandle& file, model_t& mdl)
{
    int const axis[3] = { 0, 2, 1 };

    // Read the header.
    bool readHeaderOk = readHeaderDmd(file, mdl.header);
    DENG_ASSERT(readHeaderOk);
    DENG_UNUSED(readHeaderOk); // should this be checked?

    dmd_info_t& inf = mdl.info;

    // Read the chunks.
    dmd_chunk_t chunk;
    file.read((uint8_t*)&chunk, sizeof(chunk));

    while(LONG(chunk.type) != DMC_END)
    {
        switch(LONG(chunk.type))
        {
        case DMC_INFO: // Standard DMD information chunk.
            file.read((uint8_t*)&inf, LONG(chunk.length));
            inf.skinWidth       = LONG(inf.skinWidth);
            inf.skinHeight      = LONG(inf.skinHeight);
            inf.frameSize       = LONG(inf.frameSize);
            inf.numSkins        = LONG(inf.numSkins);
            inf.numVertices     = LONG(inf.numVertices);
            inf.numTexCoords    = LONG(inf.numTexCoords);
            inf.numFrames       = LONG(inf.numFrames);
            inf.numLODs         = LONG(inf.numLODs);
            inf.offsetSkins     = LONG(inf.offsetSkins);
            inf.offsetTexCoords = LONG(inf.offsetTexCoords);
            inf.offsetFrames    = LONG(inf.offsetFrames);
            inf.offsetLODs      = LONG(inf.offsetLODs);
            inf.offsetEnd       = LONG(inf.offsetEnd);
            break;

        default:
            // Just skip all unknown chunks.
            file.seek(LONG(chunk.length), SeekCur);
            break;
        }
        // Read the next chunk header.
        file.read((uint8_t*)&chunk, sizeof(chunk));
    }

    // Allocate and load in the data. (Note: numSkins may be zero.)
    mdl.skins = (dmd_skin_t*) M_Calloc(sizeof(dmd_skin_t) * inf.numSkins);
    file.seek(inf.offsetSkins, SeekSet);
    for(int i = 0; i < inf.numSkins; ++i)
    {
        file.read((uint8_t*)mdl.skins[i].name, 64);
    }

    char* temp = (char*) allocAndLoad(file, inf.offsetFrames, inf.frameSize * inf.numFrames);
    mdl.frames = (model_frame_t*) M_Malloc(sizeof(model_frame_t) * inf.numFrames);

    model_frame_t* frame = mdl.frames;
    for(int i = 0; i < inf.numFrames; ++i, ++frame)
    {
        dmd_packedFrame_t* pfr = (dmd_packedFrame_t*) (temp + inf.frameSize * i);
        dmd_packedVertex_t* pVtx;

        memcpy(frame->name, pfr->name, sizeof(pfr->name));
        frame->vertices = (model_vertex_t*) M_Malloc(sizeof(model_vertex_t) * inf.numVertices);
        frame->normals  = (model_vertex_t*) M_Malloc(sizeof(model_vertex_t) * inf.numVertices);

        // Translate each vertex.
        pVtx = pfr->vertices;
        for(int k = 0; k < inf.numVertices; ++k, ++pVtx)
        {
            unpackVector(USHORT(pVtx->normal), frame->normals[k].xyz);
            for(int c = 0; c < 3; ++c)
            {
                frame->vertices[k].xyz[axis[c]] =
                    pVtx->vertex[c] * FLOAT(pfr->scale[c]) + FLOAT(pfr->translate[c]);
            }

            // Aspect undo.
            frame->vertices[k].xyz[1] *= rModelAspectMod;

            for(int c = 0; c < 3; ++c)
            {
                if(!k || frame->vertices[k].xyz[c] < frame->min[c])
                    frame->min[c] = frame->vertices[k].xyz[c];

                if(!k || frame->vertices[k].xyz[c] > frame->max[c])
                    frame->max[c] = frame->vertices[k].xyz[c];
            }
        }
    }
    M_Free(temp);

    file.seek(inf.offsetLODs, SeekSet);
    file.read((uint8_t*)mdl.lodInfo, sizeof(dmd_levelOfDetail_t) * inf.numLODs);

    dmd_triangle_t* triangles[MAX_LODS];
    for(int i = 0; i < inf.numLODs; ++i)
    {
        mdl.lodInfo[i].numTriangles     = LONG(mdl.lodInfo[i].numTriangles);
        mdl.lodInfo[i].numGlCommands    = LONG(mdl.lodInfo[i].numGlCommands);
        mdl.lodInfo[i].offsetTriangles  = LONG(mdl.lodInfo[i].offsetTriangles);
        mdl.lodInfo[i].offsetGlCommands = LONG(mdl.lodInfo[i].offsetGlCommands);

        triangles[i] = (dmd_triangle_t*) allocAndLoad(file, mdl.lodInfo[i].offsetTriangles,
                                                      sizeof(dmd_triangle_t) * mdl.lodInfo[i].numTriangles);

        mdl.lods[i].glCommands = (int*) allocAndLoad(file, mdl.lodInfo[i].offsetGlCommands,
                                                     sizeof(int) * mdl.lodInfo[i].numGlCommands);
    }

    // Determine vertex usage at each LOD level.
    /// @attention Assumes there will never be more than 8 LOD levels.
    mdl.vertexUsage = (char*) M_Calloc(inf.numVertices);
    for(int i = 0; i < inf.numLODs; ++i)
    {
        for(int k = 0; k < mdl.lodInfo[i].numTriangles; ++k)
        {
            for(int c = 0; c < 3; ++c)
                mdl.vertexUsage[SHORT(triangles[i][k].vertexIndices[c])] |= 1 << i;
        }
    }

    // We don't need the triangles any more.
    for(int i = 0; i < inf.numLODs; ++i)
    {
        M_Free(triangles[i]);
    }
}

static model_t* modelForId(modelid_t modelId, bool canCreate = false)
{
    DENG_ASSERT(modelRepository);
    model_t* mdl = reinterpret_cast<model_t*>(modelRepository->userPointer(modelId));
    if(!mdl && canCreate)
    {
        // Allocate a new model_t.
        mdl = (model_t*) M_Calloc(sizeof(model_t));
        mdl->modelId = modelId;
        mdl->allowTexComp = true;
        modelRepository->setUserPointer(modelId, mdl);
    }
    return mdl;
}

model_t* Models_ToModel(modelid_t id)
{
    return modelForId(id);
}

static inline String const &findModelPath(modelid_t id)
{
    return modelRepository->stringRef(id);
}

static String findSkinPath(Path const &skinPath, Path const &modelFilePath)
{
    DENG_ASSERT(!skinPath.isEmpty());

    // Try the "first choice" directory first.
    if(!modelFilePath.isEmpty())
    {
        // The "first choice" directory is that in which the model file resides.
        try
        {
            de::Uri searchPath("Models", modelFilePath.toString().fileNamePath() / skinPath.fileName());
            return App_FileSystem()->findPath(searchPath, RLF_DEFAULT, DD_ResourceClassById(RC_GRAPHIC));
        }
        catch(FS1::NotFoundError const &)
        {} // Ignore this error.
    }

    /// @throws FS1::NotFoundError if no resource was found.
    de::Uri searchPath("Models", skinPath);
    return App_FileSystem()->findPath(searchPath, RLF_DEFAULT, DD_ResourceClassById(RC_GRAPHIC));
}

/**
 * Allocate room for a new skin file name.
 */
static short defineSkinAndAddToModelIndex(model_t &mdl, Path const &skinPath)
{
    int const newSkin = mdl.info.numSkins;

    if(Texture *tex = R_DefineTexture("ModelSkins", de::Uri(skinPath)))
    {
        // A duplicate? (return existing skin number)
        for(int i = 0; i < mdl.info.numSkins; ++i)
        {
            if(mdl.skins[i].texture == reinterpret_cast<texture_s *>(tex)) return i;
        }

        // Add this new skin.
        mdl.skins = (dmd_skin_t *) M_Realloc(mdl.skins, sizeof(*mdl.skins) * ++mdl.info.numSkins);
        if(!mdl.skins) throw Error("newModelSkin", String("Failed on (re)allocation of %1 bytes enlarging DmdSkin list").arg(sizeof(*mdl.skins) * mdl.info.numSkins));
        std::memset(mdl.skins + newSkin, 0, sizeof(dmd_skin_t));

        QByteArray pathUtf8 = skinPath.toString().toUtf8();
        qstrncpy(mdl.skins[newSkin].name, pathUtf8.constData(), 256);
        mdl.skins[newSkin].texture = reinterpret_cast<texture_s *>(tex);
    }

    return newSkin;
}

static void defineAllSkins(model_t &mdl)
{
    String const &modelFilePath = findModelPath(mdl.modelId);

    int numFoundSkins = 0;
    for(int i = 0; i < mdl.info.numSkins; ++i)
    {
        if(!mdl.skins[i].name[0]) continue;

        try
        {
            de::Uri foundResourceUri(Path(findSkinPath(mdl.skins[i].name, modelFilePath)));

            mdl.skins[i].texture = reinterpret_cast<texture_s *>(R_DefineTexture("ModelSkins", foundResourceUri));

            // We have found one more skin for this model.
            numFoundSkins += 1;
        }
        catch(FS1::NotFoundError const&)
        {
            LOG_WARNING("Failed to locate \"%s\" (#%i) for model \"%s\", ignoring.")
                << mdl.skins[i].name << i << NativePath(modelFilePath).pretty();
        }
    }

    if(!numFoundSkins)
    {
        // Lastly try a skin named similarly to the model in the same directory.
        de::Uri skinSearchPath = de::Uri(modelFilePath.fileNamePath() / modelFilePath.fileNameWithoutExtension(), RC_GRAPHIC);

        AutoStr *foundSkinPath = AutoStr_NewStd();
        if(F_FindPath(RC_GRAPHIC, reinterpret_cast<uri_s *>(&skinSearchPath), foundSkinPath))
        {
            // Huzzah! we found a skin.
            defineSkinAndAddToModelIndex(mdl, Path(Str_Text(foundSkinPath)));

            // We have found one more skin for this model.
            numFoundSkins = 1;

            LOG_INFO("Assigned fallback skin \"%s\" to index #0 for model \"%s\".")
                << NativePath(Str_Text(foundSkinPath)).pretty()
                << NativePath(modelFilePath).pretty();
        }
    }

    if(!numFoundSkins)
    {
        LOG_WARNING("Failed to locate a skin for model \"%s\". This model will be rendered without a skin.")
            << NativePath(modelFilePath).pretty();
    }
}

static model_t *interpretDmd(de::FileHandle &hndl, String path, modelid_t modelId)
{
    if(recogniseDmd(hndl))
    {
        LOG_AS("DmdFileType");
        LOG_VERBOSE("Interpreted \"" + NativePath(path).pretty() + "\".");
        model_t *mdl = modelForId(modelId, true/*create*/);
        loadDmd(hndl, *mdl);
        return mdl;
    }
    return 0;
}

static model_t *interpretMd2(de::FileHandle &hndl, String path, modelid_t modelId)
{
    if(recogniseMd2(hndl))
    {
        LOG_AS("Md2FileType");
        LOG_VERBOSE("Interpreted \"" + NativePath(path).pretty() + "\".");
        model_t *mdl = modelForId(modelId, true/*create*/);
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

    model_t *(*interpretFunc)(de::FileHandle &hndl, String path, modelid_t modelId);
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

static model_t *interpretModel(de::FileHandle &hndl, String path, modelid_t modelId)
{
    model_t *mdl = 0;

    // Firstly try the interpreter for the guessed resource types.
    ModelFileType const *rtypeGuess = guessModelFileTypeFromFileName(path);
    if(rtypeGuess)
    {
        mdl = rtypeGuess->interpretFunc(hndl, path, modelId);
    }

    // If not yet interpreted - try each recognisable format in order.
    if(!mdl)
    {
        // Try each recognisable format instead.
        for(int i = 0; !modelTypes[i].name.isEmpty(); ++i)
        {
            ModelFileType const &modelType = modelTypes[i];

            // Already tried this?
            if(&modelType == rtypeGuess) continue;

            mdl = modelType.interpretFunc(hndl, path, modelId);
            if(mdl) break;
        }
    }

    return mdl;
}

/**
 * Finds the existing model or loads in a new one.
 */
static model_t *loadModel(String path)
{
    // Have we already loaded this?
    modelid_t modelId = modelRepository->intern(path);
    model_t *mdl = Models_ToModel(modelId);
    if(mdl) return mdl; // Yes.

    try
    {
        // Attempt to interpret and load this model file.
        de::FileHandle &hndl = App_FileSystem()->openFile(path, "rb");

        mdl = interpretModel(hndl, path, modelId);

        // We're done with the file.
        App_FileSystem()->releaseFile(hndl.file());
        delete &hndl;

        // Loaded?
        if(mdl)
        {
            defineAllSkins(*mdl);

            // Enlarge the vertex buffers in preparation for drawing of this model.
            if(!Rend_ModelExpandVertexBuffers(mdl->info.numVertices))
            {
                LOG_WARNING("Model \"%s\" contains more than %u max vertices (%u), it will not be rendered.")
                    << NativePath(path).pretty()
                    << uint(RENDER_MAX_MODEL_VERTS) << uint(mdl->info.numVertices);
            }

            return mdl;
        }
    }
    catch(FS1::NotFoundError const &er)
    {
        // Huh?? Should never happen.
        LOG_WARNING(er.asText() + ", ignoring.");
    }

    return 0;
}

/**
 * Returns the appropriate modeldef for the given state.
 */
static modeldef_t *getStateModel(state_t &st, int select)
{
    modeldef_t *modef = stateModefs[&st - states];
    if(!modef) return 0;

    if(select)
    {
        // Choose the correct selector, or selector zero if the given one not available.
        int const mosel = select & DDMOBJ_SELECTOR_MASK;
        for(modeldef_t *it = modef; it; it = it->selectNext)
        {
            if(it->select == mosel)
            {
                return it;
            }
        }
    }

    return modef;
}

modeldef_t* Models_Definition(char const* id)
{
    if(!id || !id[0]) return 0;

    for(int i = 0; i < numModelDefs; ++i)
    {
        if(!strcmp(modefs[i].id, id))
            return modefs + i;
    }
    return 0;
}

float Models_ModelForMobj(mobj_t* mo, modeldef_t** modef, modeldef_t** nextmodef)
{
    // On the client it is possible that we don't know the mobj's state.
    if(!mo->state) return -1;

    state_t& st = *mo->state;

    // By default there are no models.
    *nextmodef = NULL;
    *modef = getStateModel(st, mo->selector);
    if(!*modef) return -1; // No model available.

    float interp = -1;

    // World time animation?
    bool worldTime = false;
    if((*modef)->flags & MFF_WORLD_TIME_ANIM)
    {
        float duration = (*modef)->interRange[0];
        float offset   = (*modef)->interRange[1];

        // Validate/modify the values.
        if(duration == 0) duration = 1;

        if(offset == -1)
        {
            offset = M_CycleIntoRange(MOBJ_TO_ID(mo), duration);
        }

        interp = M_CycleIntoRange(ddMapTime / duration + offset, 1);
        worldTime = true;
    }
    else
    {
        // Calculate the currently applicable intermark.
        interp = 1.0f - (mo->tics - frameTimePos) / float( st.tics );
    }

/*#if _DEBUG
    if(mo->dPlayer)
    {
        qDebug() << "itp:" << interp << " mot:" << mo->tics << " stt:" << st.tics;
    }
#endif*/

    // First find the modef for the interpoint. Intermark is 'stronger' than interrange.

    // Scan interlinks.
    while((*modef)->interNext && (*modef)->interNext->interMark <= interp)
    {
        *modef = (*modef)->interNext;
    }

    if(!worldTime)
    {
        // Scale to the modeldef's interpolation range.
        interp = (*modef)->interRange[0] + interp * ((*modef)->interRange[1] -
                                                     (*modef)->interRange[0]);
    }

    // What would be the next model? Check interlinks first.
    if((*modef)->interNext)
    {
        *nextmodef = (*modef)->interNext;
    }
    else if(worldTime)
    {
        *nextmodef = getStateModel(st, mo->selector);
    }
    else if(st.nextState > 0) // Check next state.
    {
        // Find the appropriate state based on interrange.
        state_t* it = states + st.nextState;
        bool foundNext = false;
        if((*modef)->interRange[1] < 1)
        {
            // Current modef doesn't interpolate to the end, find the proper destination
            // modef (it isn't just the next one). Scan the states that follow (and
            // interlinks of each).
            bool stopScan = false;
            int max = 20; // Let's not be here forever...
            while(!stopScan)
            {
                if(!((!stateModefs[it - states] ||
                      getStateModel(*it, mo->selector)->interRange[0] > 0) &&
                     it->nextState > 0))
                {
                    stopScan = true;
                }
                else
                {
                    // Scan interlinks, then go to the next state.
                    modeldef_t* mdit;
                    if((mdit = getStateModel(*it, mo->selector)) && mdit->interNext)
                    {
                        forever
                        {
                            mdit = mdit->interNext;
                            if(mdit)
                            {
                                if(mdit->interRange[0] <= 0) // A new beginning?
                                {
                                    *nextmodef = mdit;
                                    foundNext = true;
                                }
                            }

                            if(!mdit || foundNext)
                            {
                                break;
                            }
                        }
                    }

                    if(foundNext)
                    {
                        stopScan = true;
                    }
                    else
                    {
                        it = states + it->nextState;
                    }
                }

                if(max-- <= 0)
                    stopScan = true;
            }
            // @todo What about max == -1? What should 'it' be then?
        }

        if(!foundNext)
            *nextmodef = getStateModel(*it, mo->selector);
    }

    // Is this group disabled?
    if(useModels >= 2 && (*modef)->group & useModels)
    {
        *modef = *nextmodef = NULL;
        return -1;
    }

    return interp;
}

/**
 * Scales the given model so that it'll be 'destHeight' units tall. Measurements
 * are based on submodel zero. Scale is applied uniformly.
 */
static void scaleModel(modeldef_t &mf, float destHeight, float offset)
{
    submodeldef_t &smf = mf.sub[0];

    // No model to scale?
    if(!smf.modelId) return;

    // Find the top and bottom heights.
    float top, bottom;
    float height = Models_ToModel(smf.modelId)->frame(smf.frame).horizontalRange(&top, &bottom);
    if(!height) height = 1;

    float scale = destHeight / height;

    for(int i = 0; i < 3; ++i)
    {
        mf.scale[i] = scale;
    }
    mf.offset[VY] = -bottom * scale + offset;
}

static void scaleModelToSprite(modeldef_t &mf, int sprite, int frame)
{
    spritedef_t &spr = sprites[sprite];

    if(!spr.numFrames || spr.spriteFrames == NULL) return;

    MaterialSnapshot const &ms = spr.spriteFrames[frame].mats[0]->prepare(Rend_SpriteMaterialSpec());
    Texture const &tex = ms.texture(MTU_PRIMARY).generalCase();
    int off = MAX_OF(0, -tex.origin().y() - ms.dimensions().height());
    scaleModel(mf, ms.dimensions().height(), off);
}

static float calcModelVisualRadius(modeldef_t *def)
{
    if(!def || !def->sub[0].modelId) return 0;

    // Use the first frame bounds.
    float min[3], max[3];
    float maxRadius = 0;
    for(int i = 0; i < MAX_FRAME_MODELS; ++i)
    {
        if(!def->sub[i].modelId) break;

        Models_ToModel(def->sub[i].modelId)->frame(def->sub[i].frame).getBounds(min, max);

        // Half the distance from bottom left to top right.
        float radius = (def->scale[VX] * (max[VX] - min[VX]) +
                        def->scale[VZ] * (max[VZ] - min[VZ])) / 3.5f;
        if(radius > maxRadius)
            maxRadius = radius;
    }

    return maxRadius;
}

/**
 * Create a new modeldef or find an existing one. This is for ID'd models.
 */
static modeldef_t* getModelDefWithId(char const* id)
{
    // ID defined?
    if(!id || !id[0]) return NULL;

    // First try to find an existing modef.
    modeldef_t* md = Models_Definition(id);
    if(md) return md;

    // Get a new entry.
    md = modefs + numModelDefs++;
    memset(md, 0, sizeof(*md));
    strncpy(md->id, id, MODELDEF_ID_MAXLEN);
    return md;
}

/**
 * Create a new modeldef or find an existing one. There can be only one model
 * definition associated with a state/intermark pair.
 */
static modeldef_t* getModelDef(int state, float interMark, int select)
{
    // Is this a valid state?
    if(state < 0 || state >= countStates.num) return NULL;

    // First try to find an existing modef.
    for(int i = 0; i < numModelDefs; ++i)
    {
        if(modefs[i].state == &states[state] &&
           modefs[i].interMark == interMark && modefs[i].select == select)
        {
            // Models are loaded in reverse order; this one already has
            // a model.
            return NULL;
        }
    }

    // This is impossible, but checking won't hurt...
    if(numModelDefs >= maxModelDefs) return NULL;

    modeldef_t* md = modefs + numModelDefs++;
    memset(md, 0, sizeof(*md));

    // Set initial data.
    md->state = &states[state];
    md->interMark = interMark;
    md->select = select;
    return md;
}

/**
 * Creates a modeldef based on the given DED info. A pretty straightforward
 * operation. No interlinks are set yet. Autoscaling is done and the scale
 * factors set appropriately. After this has been called for all available
 * Model DEDs, each State that has a model will have a pointer to the one
 * with the smallest intermark (start of a chain).
 */
static void setupModel(ded_model_t& def)
{
    LOG_AS("setupModel");

    int const modelScopeFlags = def.flags | defs.modelFlags;
    int const statenum = Def_GetStateNum(def.state);

    // Is this an ID'd model?
    modeldef_t* modef = getModelDefWithId(def.id);
    if(!modef)
    {
        // No, normal State-model.
        if(statenum < 0) return;

        modef = getModelDef(statenum + def.off, def.interMark, def.selector);
        if(!modef) return; // Can't get a modef, quit!
    }

    // Init modef info (state & intermark already set).
    modef->def = &def;
    modef->group = def.group;
    modef->flags = modelScopeFlags;
    for(int i = 0; i < 3; ++i)
    {
        modef->offset[i] = def.offset[i];
        modef->scale[i]  = def.scale[i];
    }
    modef->offset[VY] += defs.modelOffset; // Common Y axis offset.
    modef->scale[VY]  *= defs.modelScale;  // Common Y axis scaling.
    modef->resize      = def.resize;
    modef->skinTics    = def.skinTics;
    for(int i = 0; i < 2; ++i)
    {
        modef->interRange[i] = def.interRange[i];
    }
    if(modef->skinTics < 1)
        modef->skinTics = 1;

    // Submodels.
    ded_submodel_t* subdef = def.sub;
    submodeldef_t*  sub    = modef->sub;
    for(int i = 0; i < MAX_FRAME_MODELS; ++i, ++subdef, ++sub)
    {
        sub->modelId = 0;

        if(!subdef->filename || Uri_IsEmpty(subdef->filename)) continue;

        AutoStr* foundPath = AutoStr_NewStd();
        if(!F_FindPath(RC_MODEL, subdef->filename, foundPath))
        {
            de::Uri const &searchPath = reinterpret_cast<de::Uri &>(*subdef->filename);
            LOG_WARNING("Failed to locate \"%s\", ignoring.") << searchPath;
            continue;
        }

        model_t* mdl = loadModel(Str_Text(foundPath));
        if(!mdl) continue;

        sub->modelId = mdl->modelId;
        sub->frame = mdl->frameNumForName(subdef->frame);
        sub->frameRange = MAX_OF(1, subdef->frameRange); // Frame range must always be greater than zero.

        sub->alpha = byte(255 - subdef->alpha * 255);
        sub->blendMode = subdef->blendMode;

        // Submodel-specific flags cancel out model-scope flags!
        sub->flags = modelScopeFlags ^ subdef->flags;

        // Flags may override alpha and/or blendmode.
        if(sub->flags & MFF_BRIGHTSHADOW)
        {
            sub->alpha = byte(256 * .80f);
            sub->blendMode = BM_ADD;
        }
        else if(sub->flags & MFF_BRIGHTSHADOW2)
        {
            sub->blendMode = BM_ADD;
        }
        else if(sub->flags & MFF_DARKSHADOW)
        {
            sub->blendMode = BM_DARK;
        }
        else if(sub->flags & MFF_SHADOW2)
        {
            sub->alpha = byte(256 * .2f);
        }
        else if(sub->flags & MFF_SHADOW1)
        {
            sub->alpha = byte(256 * .62f);
        }

        // Extra blendmodes:
        if(sub->flags & MFF_REVERSE_SUBTRACT)
        {
            sub->blendMode = BM_REVERSE_SUBTRACT;
        }
        else if(sub->flags & MFF_SUBTRACT)
        {
            sub->blendMode = BM_SUBTRACT;
        }

        if(subdef->skinFilename && !Uri_IsEmpty(subdef->skinFilename))
        {
            // A specific file name has been given for the skin.
            String const &skinFilePath  = reinterpret_cast<de::Uri &>(*subdef->skinFilename).path();
            String const &modelFilePath = findModelPath(sub->modelId);

            try
            {
                Path foundResourcePath(findSkinPath(skinFilePath, modelFilePath));

                sub->skin = defineSkinAndAddToModelIndex(*mdl, foundResourcePath);
            }
            catch(FS1::NotFoundError const&)
            {
                LOG_WARNING("Failed to locate skin \"%s\" for model \"%s\", ignoring.")
                    << reinterpret_cast<de::Uri &>(*subdef->skinFilename) << NativePath(modelFilePath).pretty();
            }
        }
        else
        {
            sub->skin = subdef->skin;
        }

        sub->skinRange = subdef->skinRange;
        // Skin range must always be greater than zero.
        if(sub->skinRange < 1)
            sub->skinRange = 1;

        // Offset within the model.
        for(int k = 0; k < 3; ++k)
        {
            sub->offset[k] = subdef->offset[k];
        }

        if(subdef->shinySkin && !Uri_IsEmpty(subdef->shinySkin))
        {
            String const &skinFilePath  = reinterpret_cast<de::Uri &>(*subdef->shinySkin).path();
            String const &modelFilePath = findModelPath(sub->modelId);

            try
            {
                de::Uri foundResourceUri(Path(findSkinPath(skinFilePath, modelFilePath)));

                sub->shinySkin = reinterpret_cast<texture_s *>(R_DefineTexture("ModelReflectionSkins", foundResourceUri));
            }
            catch(FS1::NotFoundError const &)
            {
                LOG_WARNING("Failed to locate skin \"%s\" for model \"%s\", ignoring.")
                    << skinFilePath << NativePath(modelFilePath).pretty();
            }
        }
        else
        {
            sub->shinySkin = 0;
        }

        // Should we allow texture compression with this model?
        if(sub->flags & MFF_NO_TEXCOMP)
        {
            // All skins of this model will no longer use compression.
            mdl->allowTexComp = false;
        }
    }

    // Do scaling, if necessary.
    if(modef->resize)
    {
        scaleModel(*modef, modef->resize, modef->offset[VY]);
    }
    else if(modef->state && modef->sub[0].flags & MFF_AUTOSCALE)
    {
        int sprNum   = Def_GetSpriteNum(def.sprite.id);
        int sprFrame = def.spriteFrame;

        if(sprNum < 0)
        {
            // No sprite ID given.
            sprNum   = modef->state->sprite;
            sprFrame = modef->state->frame;
        }

        scaleModelToSprite(*modef, sprNum, sprFrame);
    }

    if(modef->state)
    {
        int stateNum = modef->state - states;

        // Associate this modeldef with its state.
        if(!stateModefs[stateNum])
        {
            // No modef; use this.
            stateModefs[stateNum] = modef;
        }
        else
        {
            // Must check intermark; smallest wins!
            modeldef_t* other = stateModefs[stateNum];

            if((modef->interMark <= other->interMark && // Should never be ==
                modef->select == other->select) || modef->select < other->select) // Smallest selector?
                stateModefs[stateNum] = modef;
        }
    }

    // Calculate the particle offset for each submodel.
    sub = modef->sub;
    float min[3], max[3];
    for(int i = 0; i < MAX_FRAME_MODELS; ++i, ++sub)
    {
        if(sub->modelId)
        {
            Models_ToModel(sub->modelId)->frame(sub->frame).getBounds(min, max);

            // Apply the various scalings and offsets.
            for(int k = 0; k < 3; ++k)
            {
                modef->ptcOffset[i][k] =
                    ((max[k] + min[k]) / 2 + sub->offset[k]) * modef->scale[k] + modef->offset[k];
            }
        }
        else
        {
            memset(modef->ptcOffset[i], 0, sizeof(modef->ptcOffset[i]));
        }
    }

    // Calculate visual radius for shadows.
    if(def.shadowRadius)
    {
        modef->visualRadius = def.shadowRadius;
    }
    else
    {
        modef->visualRadius = calcModelVisualRadius(modef);
    }
}

static int destroyModelInRepository(StringPool::Id id, void* /*parm*/)
{
    model_t* mdl = reinterpret_cast<model_t*>(modelRepository->userPointer(id));
    if(!mdl) return 0;

    M_Free(mdl->skins);
    for(int i = 0; i < mdl->info.numFrames; ++i)
    {
        M_Free(mdl->frames[i].vertices);
        M_Free(mdl->frames[i].normals);
    }
    M_Free(mdl->frames);

    for(int i = 0; i < mdl->info.numLODs; ++i)
    {
        //M_Free(modellist[i]->lods[k].triangles);
        M_Free(mdl->lods[i].glCommands);
    }
    M_Free(mdl->vertexUsage);
    M_Free(mdl);

    return 0;
}

static void clearModelList(void)
{
    if(!modelRepository) return;

    modelRepository->iterate(destroyModelInRepository, 0);
}

void Models_Init(void)
{
    // Dedicated servers do nothing with models.
    if(isDedicated || CommandLine_Check("-nomd2")) return;

    LOG_VERBOSE("Initializing Models...");
    uint usedTime = Timer_RealMilliseconds();

    modelRepository = new StringPool();

    clearModelList();
    if(modefs)
    {
        M_Free(modefs);
    }

    // There can't be more modeldefs than there are DED Models.
    // There can be fewer, though.
    maxModelDefs = defs.count.models.num;
    modefs = (modeldef_t*) M_Malloc(sizeof(modeldef_t) * maxModelDefs);
    numModelDefs = 0;

    // Clear the modef pointers of all States.
    stateModefs = (modeldef_t**) M_Realloc(stateModefs, countStates.num * sizeof(*stateModefs));
    memset(stateModefs, 0, countStates.num * sizeof(*stateModefs));

    // Read in the model files and their data.
    // Use the latest definition available for each sprite ID.
    for(int i = defs.count.models.num - 1; i >= 0; --i)
    {
        if(!(i % 100))
        {
            // This may take a while, so keep updating the progress.
            Con_SetProgress(130 + 70*(defs.count.models.num - i)/defs.count.models.num);
        }

        setupModel(defs.models[i]);
    }

    // Create interlinks. Note that the order in which the defs were loaded
    // is important. We want to allow "patch" definitions, right?

    // For each modeldef we will find the "next" def.
    modeldef_t* me = modefs + (numModelDefs - 1);
    for(int i = numModelDefs - 1; i >= 0; --i, --me)
    {
        float minmark = 2; // max = 1, so this is "out of bounds".

        modeldef_t* closest = 0;

        modeldef_t* other = modefs + (numModelDefs - 1);
        for(int k = numModelDefs - 1; k >= 0; --k, --other)
        {
            // Same state and a bigger order are the requirements.
            if(other->state == me->state && other->def > me->def && // Defined after me.
               other->interMark > me->interMark &&
               other->interMark < minmark)
            {
                minmark = other->interMark;
                closest = other;
            }
        }

        me->interNext = closest;
    }

    // Create selectlinks.
    me = modefs + (numModelDefs - 1);
    for(int i = numModelDefs - 1; i >= 0; --i, --me)
    {
        int minsel = DDMAXINT;

        modeldef_t* closest = 0;

        // Start scanning from the next definition.
        modeldef_t* other = modefs + (numModelDefs - 1);
        for(int k = numModelDefs - 1; k >= 0; --k, --other)
        {
            // Same state and a bigger order are the requirements.
            if(other->state == me->state && other->def > me->def && // Defined after me.
               other->select > me->select && other->select < minsel &&
               other->interMark >= me->interMark)
            {
                minsel = other->select;
                closest = other;
            }
        }

        me->selectNext = closest;
    }

    LOG_INFO(String("Models_Init: Done in %1 seconds.").arg(double((Timer_RealMilliseconds() - usedTime) / 1000.0f), 0, 'g', 2));
}

void Models_Shutdown(void)
{
    /// @todo Why only centralized memory deallocation? Bad design...
    if(modefs)
    {
        M_Free(modefs); modefs = 0;
    }
    if(stateModefs)
    {
        M_Free(stateModefs); stateModefs = 0;
    }

    clearModelList();

    if(modelRepository)
    {
        delete modelRepository; modelRepository = 0;
    }
}

void Models_Cache(modeldef_t* modef)
{
    if(!modef) return;

    for(int sub = 0; sub < MAX_FRAME_MODELS; ++sub)
    {
        submodeldef_t& subdef = modef->sub[sub];
        model_t* mdl = Models_ToModel(subdef.modelId);
        if(!mdl) continue;

        // Load all skins.
        for(int k = 0; k < mdl->info.numSkins; ++k)
        {
            if(Texture *tex = reinterpret_cast<Texture *>(mdl->skins[k].texture))
            {
                GL_PrepareTexture(*tex, *Rend_ModelDiffuseTextureSpec(!mdl->allowTexComp));
            }
        }

        // Load the shiny skin too.
        if(Texture *tex = reinterpret_cast<Texture *>(subdef.shinySkin))
        {
            GL_PrepareTexture(*tex, *Rend_ModelShinyTextureSpec());
        }
    }
}

#undef Models_CacheForState
DENG_EXTERN_C void Models_CacheForState(int stateIndex)
{
    if(!useModels) return;
    if(stateIndex <= 0 || stateIndex >= defs.count.states.num) return;
    if(!stateModefs[stateIndex]) return;

    Models_Cache(stateModefs[stateIndex]);
}

int Models_CacheForMobj(thinker_t* th, void* /*context*/)
{
    if(!(useModels && precacheSkins)) return true;

    mobj_t* mo = (mobj_t*) th;

    // Check through all the model definitions.
    modeldef_t* modef = modefs;
    for(int i = 0; i < numModelDefs; ++i, ++modef)
    {
        if(!modef->state) continue;
        if(mo->type < 0 || mo->type >= defs.count.mobjs.num) continue; // Hmm?
        if(stateOwners[modef->state - states] != &mobjInfo[mo->type]) continue;

        Models_Cache(modef);
    }

    return false; // Used as iterator.
}
