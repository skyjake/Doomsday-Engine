/** @file resource/model.cpp  3D model resource (MD2/DMD).
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "resource/framemodel.h"
#include "resource/clienttexture.h"

#include <doomsday/res/texturemanifest.h>
#include <doomsday/filesys/file.h>
#include <de/range.h>
#include <de/bitarray.h>
#include <de/legacy/memory.h>

using namespace de;
using namespace res;

bool FrameModel::DetailLevel::hasVertex(int number) const
{
    return model.lodVertexUsage().testBit(number * model.lodCount() + level);
}

void FrameModel::Frame::bounds(Vec3f &retMin, Vec3f &retMax) const
{
    retMin = min;
    retMax = max;
}

float FrameModel::Frame::horizontalRange(float *top, float *bottom) const
{
    *top    = max.y;
    *bottom = min.y;
    return max.y - min.y;
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

static bool readMd2Header(FileHandle &file, md2_header_t &hdr)
{
    size_t readBytes = file.read((uint8_t *)&hdr, sizeof(md2_header_t));
    if(readBytes < sizeof(md2_header_t)) return false;

    hdr.magic            = littleEndianByteOrder.toHost(hdr.magic);
    hdr.version          = littleEndianByteOrder.toHost(hdr.version);
    hdr.skinWidth        = littleEndianByteOrder.toHost(hdr.skinWidth);
    hdr.skinHeight       = littleEndianByteOrder.toHost(hdr.skinHeight);
    hdr.frameSize        = littleEndianByteOrder.toHost(hdr.frameSize);
    hdr.numSkins         = littleEndianByteOrder.toHost(hdr.numSkins);
    hdr.numVertices      = littleEndianByteOrder.toHost(hdr.numVertices);
    hdr.numTexCoords     = littleEndianByteOrder.toHost(hdr.numTexCoords);
    hdr.numTriangles     = littleEndianByteOrder.toHost(hdr.numTriangles);
    hdr.numGlCommands    = littleEndianByteOrder.toHost(hdr.numGlCommands);
    hdr.numFrames        = littleEndianByteOrder.toHost(hdr.numFrames);
    hdr.offsetSkins      = littleEndianByteOrder.toHost(hdr.offsetSkins);
    hdr.offsetTexCoords  = littleEndianByteOrder.toHost(hdr.offsetTexCoords);
    hdr.offsetTriangles  = littleEndianByteOrder.toHost(hdr.offsetTriangles);
    hdr.offsetFrames     = littleEndianByteOrder.toHost(hdr.offsetFrames);
    hdr.offsetGlCommands = littleEndianByteOrder.toHost(hdr.offsetGlCommands);
    hdr.offsetEnd        = littleEndianByteOrder.toHost(hdr.offsetEnd);
    return true;
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

static bool readHeaderDmd(FileHandle &file, dmd_header_t &hdr)
{
    size_t readBytes = file.read((uint8_t *)&hdr, sizeof(dmd_header_t));
    if(readBytes < sizeof(dmd_header_t)) return false;

    hdr.magic   = littleEndianByteOrder.toHost(hdr.magic);
    hdr.version = littleEndianByteOrder.toHost(hdr.version);
    hdr.flags   = littleEndianByteOrder.toHost(hdr.flags);
    return true;
}

static void *allocAndLoad(FileHandle &file, int offset, int len)
{
    uint8_t *ptr = (uint8_t *) M_Malloc(len);
    file.seek(offset, SeekSet);
    file.read(ptr, len);
    return ptr;
}

// Precalculated normal LUT for use when loading MD2/DMD format models.
#define NUMVERTEXNORMALS 162
static float avertexnormals[NUMVERTEXNORMALS][3] = {
#include "misc/tab_anorms.h"
};

/**
 * @todo reimplement file loading using de::Reader.
 */
DE_PIMPL(FrameModel)
{
    Flags  flags;
    Skins  skins;
    Frames frames;
    int    numVertices;

    DetailLevels lods;
    BitArray     lodVertexUsage;

    uint modelId; ///< In the repository.

    Impl(Public *i)
        : Base(i)
        , numVertices(0)
        , modelId(0)
    {}

    ~Impl()
    {
        self().clearAllFrames();
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
    static FrameModel *loadMd2(FileHandle &file, float aspectScale)
    {
        // Determine whether this appears to be a MD2 model.
        md2_header_t hdr;
        if(!readMd2Header(file, hdr)) return 0;
        if(hdr.magic != MD2_MAGIC) return 0;

        FrameModel *mdl = new FrameModel;

        mdl->d->numVertices = hdr.numVertices;

        // Load and convert to DMD.
        uint8_t *frameData = (uint8_t *) allocAndLoad(file, hdr.offsetFrames, hdr.frameSize * hdr.numFrames);
        for(int i = 0; i < hdr.numFrames; ++i)
        {
            const md2_packedFrame_t *pfr = (const md2_packedFrame_t *) (frameData + hdr.frameSize * i);
            Vec3f const scale(DD_FLOAT(pfr->scale[0]), DD_FLOAT(pfr->scale[2]), DD_FLOAT(pfr->scale[1]));
            Vec3f const translation(DD_FLOAT(pfr->translate[0]), DD_FLOAT(pfr->translate[2]), DD_FLOAT(pfr->translate[1]));
            const String frameName = pfr->name;

            FrameModelFrame *frame = new FrameModelFrame(*mdl, frameName);
            frame->vertices.reserve(hdr.numVertices);

            // Scale and translate each vertex.
            const md2_triangleVertex_t *pVtx = pfr->vertices;
            for(int k = 0; k < hdr.numVertices; ++k, pVtx++)
            {
                frame->vertices.append(FrameModelFrame::Vertex());
                FrameModelFrame::Vertex &vtx = frame->vertices.last();

                vtx.pos = Vec3f(pVtx->vertex[0], pVtx->vertex[2], pVtx->vertex[1])
                              * scale + translation;
                vtx.pos.y *= aspectScale; // Aspect undoing.

                vtx.norm = Vec3f(avertexnormals[pVtx->normalIndex]);

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

            mdl->d->frames.append(frame);
        }
        M_Free(frameData);

        mdl->d->lods.append(new FrameModelLOD(*mdl, 0));
        FrameModelLOD &lod0 = *mdl->d->lods.last();

        uint8_t *commandData = (uint8_t *) allocAndLoad(file, hdr.offsetGlCommands, 4 * hdr.numGlCommands);
        for(const uint8_t *pos = commandData; *pos;)
        {
            int count = DD_LONG( *(int *) pos ); pos += 4;

            lod0.primitives.append(FrameModel::Primitive());
            FrameModel::Primitive &prim = lod0.primitives.last();

            // The type of primitive depends on the sign.
            prim.triFan = (count < 0);

            if(count < 0)
            {
                count = -count;
            }

            while(count--)
            {
                const md2_commandElement_t *v = (md2_commandElement_t *) pos; pos += 12;

                prim.elements.append(FrameModel::Primitive::Element());
                FrameModel::Primitive::Element &elem = prim.elements.last();
                elem.texCoord = Vec2f(DD_FLOAT(v->s), DD_FLOAT(v->t));
                elem.index    = DD_LONG(v->index);
            }
        }
        M_Free(commandData);

        // Load skins. (Note: numSkins may be zero.)
        file.seek(hdr.offsetSkins, SeekSet);
        for(int i = 0; i < hdr.numSkins; ++i)
        {
            char name[64]; file.read((uint8_t *)name, 64);
            mdl->newSkin(name);
        }

        return mdl;
    }

    // DMD chunk types.
    enum {
        DMC_END, /// Must be the last chunk.
        DMC_INFO /// Required; will be expected to exist.
    };

#pragma pack(1)
    struct dmd_chunk_t
    {
        int type;
        int length; /// Next chunk follows...
    };

    struct dmd_info_t
    {
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
    };

    struct dmd_levelOfDetail_t
    {
        int numTriangles;
        int numGlCommands;
        int offsetTriangles;
        int offsetGlCommands;
    };

    struct dmd_packedVertex_t
    {
        byte vertex[3];
        unsigned short normal; /// Yaw and pitch.
    };

    struct dmd_packedFrame_t
    {
        float scale[3];
        float translate[3];
        char name[16];
        dmd_packedVertex_t vertices[1]; // dmd_info_t::numVertices size
    };

    struct dmd_triangle_t
    {
        short vertexIndices[3];
        short textureIndices[3];
    };
#pragma pack()

    /**
     * Packed: pppppppy yyyyyyyy. Yaw is on the XY plane.
     */
    static Vec3f unpackVector(ushort packed)
    {
        const float yaw   = (packed & 511) / 512.0f * 2 * PI;
        const float pitch = ((packed >> 9) / 127.0f - 0.5f) * PI;
        const float cosp  = float(cos(pitch));
        return Vec3f(cos(yaw) * cosp, sin(yaw) * cosp, sin(pitch));
    }

    /**
     * Note vertex Z/Y are swapped here (ordered XYZ in the serialized data).
     */
    static FrameModel *loadDmd(FileHandle &file, float aspectScale)
    {
        // Determine whether this appears to be a DMD model.
        dmd_header_t hdr;
        if(!readHeaderDmd(file, hdr)) return 0;
        if(hdr.magic != DMD_MAGIC) return 0;

        // Read the chunks.
        dmd_chunk_t chunk;
        file.read((uint8_t *)&chunk, sizeof(chunk));

        dmd_info_t info; zap(info);
        while(DD_LONG(chunk.type) != DMC_END)
        {
            switch(DD_LONG(chunk.type))
            {
            case DMC_INFO: // Standard DMD information chunk.
                file.read((uint8_t *)&info, DD_LONG(chunk.length));

                info.skinWidth       = DD_LONG(info.skinWidth);
                info.skinHeight      = DD_LONG(info.skinHeight);
                info.frameSize       = DD_LONG(info.frameSize);
                info.numSkins        = DD_LONG(info.numSkins);
                info.numVertices     = DD_LONG(info.numVertices);
                info.numTexCoords    = DD_LONG(info.numTexCoords);
                info.numFrames       = DD_LONG(info.numFrames);
                info.numLODs         = DD_LONG(info.numLODs);
                info.offsetSkins     = DD_LONG(info.offsetSkins);
                info.offsetTexCoords = DD_LONG(info.offsetTexCoords);
                info.offsetFrames    = DD_LONG(info.offsetFrames);
                info.offsetLODs      = DD_LONG(info.offsetLODs);
                info.offsetEnd       = DD_LONG(info.offsetEnd);
                break;

            default:
                // Skip unknown chunks.
                file.seek(DD_LONG(chunk.length), SeekCur);
                break;
            }
            // Read the next chunk header.
            file.read((uint8_t *)&chunk, sizeof(chunk));
        }

        FrameModel *mdl = new FrameModel;

        mdl->d->numVertices = info.numVertices;

        // Allocate and load in the data. (Note: numSkins may be zero.)
        file.seek(info.offsetSkins, SeekSet);
        for(int i = 0; i < info.numSkins; ++i)
        {
            char name[64]; file.read((uint8_t *)name, 64);
            mdl->newSkin(name);
        }

        uint8_t *frameData = (uint8_t *) allocAndLoad(file, info.offsetFrames, info.frameSize * info.numFrames);
        for(int i = 0; i < info.numFrames; ++i)
        {
            const dmd_packedFrame_t *pfr = (dmd_packedFrame_t *) (frameData + info.frameSize * i);
            Vec3f const scale(DD_FLOAT(pfr->scale[0]), DD_FLOAT(pfr->scale[2]), DD_FLOAT(pfr->scale[1]));
            Vec3f const translation(DD_FLOAT(pfr->translate[0]), DD_FLOAT(pfr->translate[2]), DD_FLOAT(pfr->translate[1]));
            const String frameName = pfr->name;

            Frame *frame = new Frame(*mdl, frameName);
            frame->vertices.reserve(info.numVertices);

            // Scale and translate each vertex.
            const dmd_packedVertex_t *pVtx = pfr->vertices;
            for(int k = 0; k < info.numVertices; ++k, ++pVtx)
            {
                frame->vertices.append(Frame::Vertex());
                Frame::Vertex &vtx = frame->vertices.last();

                vtx.pos = Vec3f(pVtx->vertex[0], pVtx->vertex[2], pVtx->vertex[1])
                              * scale + translation;
                vtx.pos.y *= aspectScale; // Aspect undo.

                vtx.norm = unpackVector(DD_USHORT(pVtx->normal));

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

            mdl->d->frames.append(frame);
        }
        M_Free(frameData);

        file.seek(info.offsetLODs, SeekSet);
        dmd_levelOfDetail_t *lodInfo = new dmd_levelOfDetail_t[info.numLODs];

        for(int i = 0; i < info.numLODs; ++i)
        {
            file.read((uint8_t *)&lodInfo[i], sizeof(dmd_levelOfDetail_t));

            lodInfo[i].numTriangles     = DD_LONG(lodInfo[i].numTriangles);
            lodInfo[i].numGlCommands    = DD_LONG(lodInfo[i].numGlCommands);
            lodInfo[i].offsetTriangles  = DD_LONG(lodInfo[i].offsetTriangles);
            lodInfo[i].offsetGlCommands = DD_LONG(lodInfo[i].offsetGlCommands);
        }

        dmd_triangle_t **triangles = new dmd_triangle_t*[info.numLODs];

        for(int i = 0; i < info.numLODs; ++i)
        {
            mdl->d->lods.append(new DetailLevel(*mdl, i));
            DetailLevel &lod = *mdl->d->lods.last();

            triangles[i] = (dmd_triangle_t *) allocAndLoad(file, lodInfo[i].offsetTriangles,
                                                           sizeof(dmd_triangle_t) * lodInfo[i].numTriangles);

            uint8_t *commandData = (uint8_t *) allocAndLoad(file, lodInfo[i].offsetGlCommands,
                                                            4 * lodInfo[i].numGlCommands);
            for(const uint8_t *pos = commandData; *pos;)
            {
                int count = DD_LONG( *(int *) pos ); pos += 4;

                lod.primitives.append(Primitive());
                Primitive &prim = lod.primitives.last();

                // The type of primitive depends on the sign of the element count.
                prim.triFan = (count < 0);

                if(count < 0)
                {
                    count = -count;
                }

                while(count--)
                {
                    const md2_commandElement_t *v = (md2_commandElement_t *) pos; pos += 12;

                    prim.elements.append(Primitive::Element());
                    Primitive::Element &elem = prim.elements.last();

                    elem.texCoord = Vec2f(DD_FLOAT(v->s), DD_FLOAT(v->t));
                    elem.index    = DD_LONG(v->index);
                }
            }
            M_Free(commandData);
        }

        // Determine vertex usage at each LOD level.
        mdl->d->lodVertexUsage.resize(info.numVertices * info.numLODs);
        mdl->d->lodVertexUsage.fill(false);

        for(int i = 0; i < info.numLODs; ++i)
        for(int k = 0; k < lodInfo[i].numTriangles; ++k)
        for(int m = 0; m < 3; ++m)
        {
            int vertexIndex = DD_SHORT(triangles[i][k].vertexIndices[m]);
            mdl->d->lodVertexUsage.setBit(vertexIndex * info.numLODs + i);
        }

        delete [] lodInfo;
        for(int i = 0; i < info.numLODs; ++i)
        {
            M_Free(triangles[i]);
        }
        delete [] triangles;

        return mdl;
    }

#if 0
    /**
     * Calculate vertex normals. Only with -renorm.
     */
    static void rebuildNormals(model_t &mdl)
    {
        // Renormalizing?
        if(!CommandLine_Check("-renorm")) return;

        const int tris  = mdl.lodInfo[0].numTriangles;
        const int verts = mdl.info.numVertices;

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
};

FrameModel::FrameModel(Flags flags) : d(new Impl(this))
{
    setFlags(flags, de::ReplaceFlags);
}

static bool recogniseDmd(FileHandle &file)
{
    dmd_header_t hdr;
    size_t initPos = file.tell();
    // Seek to the start of the header.
    file.seek(0, SeekSet);
    bool result = (readHeaderDmd(file, hdr) && DD_LONG(hdr.magic) == DMD_MAGIC);
    // Return the stream to its original position.
    file.seek(initPos, SeekSet);
    return result;
}

static bool recogniseMd2(FileHandle &file)
{
    md2_header_t hdr;
    size_t initPos = file.tell();
    // Seek to the start of the header.
    file.seek(0, SeekSet);
    bool result = (readMd2Header(file, hdr) && DD_LONG(hdr.magic) == MD2_MAGIC);
    // Return the stream to its original position.
    file.seek(initPos, SeekSet);
    return result;
}

bool FrameModel::recognise(FileHandle &hndl) //static
{
    LOG_AS("FrameModel");

    if(recogniseDmd(hndl)) return true;
    if(recogniseMd2(hndl)) return true;
    return false;
}

struct ModelFileType
{
    String name; ///< Symbolic name of the resource type.
    String ext;  ///< Known file extension.

    FrameModel *(*loadFunc)(FileHandle &hndl, float aspectScale);
};

FrameModel *FrameModel::loadFromFile(FileHandle &hndl, float aspectScale) //static
{
    LOG_AS("FrameModel");

    // Recognized file types.
    static ModelFileType const modelTypes[] = {
        { "DMD",    ".dmd",     Impl::loadDmd },
        { "MD2",    ".md2",     Impl::loadMd2 },
    };

    // Firstly, attempt to guess the resource type from the file extension.
    const ModelFileType *rtypeGuess = 0;
    String filePath = hndl.file().composePath();
    String ext      = filePath.fileNameExtension();
    if (!ext.isEmpty())
    {
        for (const auto &rtype : modelTypes)
        {
            if (!rtype.ext.compareWithoutCase(ext))
            {
                rtypeGuess = &rtype;
                if (FrameModel *mdl = rtype.loadFunc(hndl, aspectScale))
                {
                    LOG_RES_VERBOSE("Interpreted \"" + NativePath(filePath).pretty() + "\" as a " + rtype.name + " model");
                    return mdl;
                }
                break;
            }
        }
    }

    // Not yet interpreted - try each known format in order.
    for (const auto &rtype : modelTypes)
    {
        // Already tried this?
        if (&rtype == rtypeGuess) continue;

        if (FrameModel *mdl = rtype.loadFunc(hndl, aspectScale))
        {
            LOG_RES_VERBOSE("Interpreted \"" + NativePath(filePath).pretty() + "\" as a " + rtype.name + " model");
            return mdl;
        }
    }

    return 0;
}

uint FrameModel::modelId() const
{
    return d->modelId;
}

void FrameModel::setModelId(uint newModelId)
{
    d->modelId = newModelId;
}

Flags FrameModel::flags() const
{
    return d->flags;
}

void FrameModel::setFlags(Flags flagsToChange, FlagOp operation)
{
    LOG_AS("FrameModel");
    applyFlagOperation(d->flags, flagsToChange, operation);
}

int FrameModel::frameNumber(const String& name) const
{
    if (!name.isEmpty())
    {
        for (int i = 0; i < d->frames.count(); ++i)
        {
            Frame *frame = d->frames.at(i);
            if (!frame->name.compareWithoutCase(name)) return i;
        }
    }
    return -1; // Not found.
}

FrameModel::Frame &FrameModel::frame(int number) const
{
    LOG_AS("FrameModel");
    if(hasFrame(number))
    {
        return *d->frames.at(number);
    }
    throw MissingFrameError("FrameModel::frame", "Invalid frame number " + String::asText(number) + ", valid range is " + Rangei(0, d->frames.count()).asText());
}

const FrameModel::Frames &FrameModel::frames() const
{
    return d->frames;
}

void FrameModel::clearAllFrames()
{
    LOG_AS("FrameModel");
    deleteAll(d->frames);
    d->frames.clear();
}

int FrameModel::skinNumber(const String& name) const
{
    if(!name.isEmpty())
    {
        // Reverse iteration so that later skins override earlier ones.
        for(int i = d->skins.count(); i--> 0; )
        {
            const Skin &skin = d->skins.at(i);
            if(!skin.name.compareWithoutCase(name))
                return i;
        }
    }
    return -1; // Not found.
}

FrameModel::Skin &FrameModel::skin(int number) const
{
    LOG_AS("FrameModel");
    if(hasSkin(number))
    {
        return const_cast<Skin &>(d->skins.at(number));
    }
    throw MissingSkinError("FrameModel::skin", "Invalid skin number " + String::asText(number) + ", valid range is " + Rangei(0, d->skins.count()).asText());
}

FrameModel::Skin &FrameModel::newSkin(String name)
{
    LOG_AS("FrameModel");
    // Allow duplicates so that skin indices remain unchanged for selection by index.
    d->skins.append(FrameModelSkin(name));
    return d->skins.last();
}

const FrameModel::Skins &FrameModel::skins() const
{
    return d->skins;
}

void FrameModel::clearAllSkins()
{
    LOG_AS("FrameModel");
    d->skins.clear();
}

FrameModel::DetailLevel &FrameModel::lod(int level) const
{
    LOG_AS("FrameModel");
    if(hasLod(level))
    {
        return *d->lods.at(level);
    }
    throw MissingDetailLevelError("FrameModel::lod", "Invalid detail level " + String::asText(level) + ", valid range is " + Rangei(0, d->lods.count()).asText());
}

const FrameModel::DetailLevels &FrameModel::lods() const
{
    return d->lods;
}

const FrameModel::Primitives &FrameModel::primitives() const
{
    LOG_AS("FrameModel");
    return lod(0).primitives;
}

int FrameModel::vertexCount() const
{
    return d->numVertices;
}

const BitArray &FrameModel::lodVertexUsage() const
{
    return d->lodVertexUsage;
}
