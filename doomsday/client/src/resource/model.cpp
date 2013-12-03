/** @file model.cpp  3D model resource.
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
#include "resource/model.h"

#include "filesys/file.h"
#include "Texture"
#include "TextureManifest"
#include <de/Range>
#include <de/memory.h>
#include <QtAlgorithms>

using namespace de;

bool Model::DetailLevel::hasVertex(int number) const
{
    return model._vertexUsage.testBit(number * model.lodCount() + level);
}

void Model::Frame::bounds(Vector3f &retMin, Vector3f &retMax) const
{
    retMin = min;
    retMax = max;
}

float Model::Frame::horizontalRange(float *top, float *bottom) const
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

static void *allocAndLoad(de::FileHandle &file, int offset, int len)
{
    uint8_t *ptr = (uint8_t *) M_Malloc(len);
    file.seek(offset, SeekSet);
    file.read(ptr, len);
    return ptr;
}

// Precalculated normal LUT for use when loading MD2/DMD format models.
#define NUMVERTEXNORMALS 162
static float avertexnormals[NUMVERTEXNORMALS][3] = {
#include "tab_anorms.h"
};

DENG2_PIMPL(Model)
{
    uint modelId; ///< Unique id of the model (in the repository).
    Flags flags;
    Skins skins;
    Frames frames;

    Instance(Public *i)
        : Base(i)
        , modelId(0)
    {}

    ~Instance()
    {
        self.clearAllFrames();
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
    static Model *loadMd2(de::FileHandle &file, float aspectScale)
    {
        // Read the header.
        md2_header_t hdr;
        bool readHeaderOk = readMd2Header(file, hdr);
        DENG2_ASSERT(readHeaderOk);
        DENG2_UNUSED(readHeaderOk); // should this be checked?

        Model *mdl = new Model;

        mdl->_numVertices = hdr.numVertices;

        // Load and convert to DMD.
        uint8_t *frameData = (uint8_t *) allocAndLoad(file, hdr.offsetFrames, hdr.frameSize * hdr.numFrames);
        for(int i = 0; i < hdr.numFrames; ++i)
        {
            md2_packedFrame_t const *pfr = (md2_packedFrame_t const *) (frameData + hdr.frameSize * i);
            Vector3f const scale(FLOAT(pfr->scale[0]), FLOAT(pfr->scale[2]), FLOAT(pfr->scale[1]));
            Vector3f const translation(FLOAT(pfr->translate[0]), FLOAT(pfr->translate[2]), FLOAT(pfr->translate[1]));
            String const frameName = pfr->name;

            ModelFrame *frame = new ModelFrame(*mdl, frameName);
            frame->vertices.reserve(hdr.numVertices);

            // Scale and translate each vertex.
            md2_triangleVertex_t const *pVtx = pfr->vertices;
            for(int k = 0; k < hdr.numVertices; ++k, pVtx++)
            {
                frame->vertices.append(ModelFrame::Vertex());
                ModelFrame::Vertex &vtx = frame->vertices.last();

                vtx.pos = Vector3f(pVtx->vertex[0], pVtx->vertex[2], pVtx->vertex[1])
                              * scale + translation;
                vtx.pos.y *= aspectScale; // Aspect undoing.

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

            mdl->addFrame(frame); // takes owernship
        }
        M_Free(frameData);

        mdl->_lods.append(new ModelDetailLevel(*mdl, 0));
        ModelDetailLevel &lod0 = *mdl->_lods.last();

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
    static Model *loadDmd(de::FileHandle &file, float aspectScale)
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

        Model *mdl = new Model;

        mdl->_numVertices = info.numVertices;

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
            dmd_packedFrame_t const *pfr = (dmd_packedFrame_t *) (frameData + info.frameSize * i);
            Vector3f const scale(FLOAT(pfr->scale[0]), FLOAT(pfr->scale[2]), FLOAT(pfr->scale[1]));
            Vector3f const translation(FLOAT(pfr->translate[0]), FLOAT(pfr->translate[2]), FLOAT(pfr->translate[1]));
            String const frameName = pfr->name;

            ModelFrame *frame = new ModelFrame(*mdl, frameName);
            frame->vertices.reserve(info.numVertices);

            // Scale and translate each vertex.
            dmd_packedVertex_t const *pVtx = pfr->vertices;
            for(int k = 0; k < info.numVertices; ++k, ++pVtx)
            {
                frame->vertices.append(ModelFrame::Vertex());
                ModelFrame::Vertex &vtx = frame->vertices.last();

                vtx.pos = Vector3f(pVtx->vertex[0], pVtx->vertex[2], pVtx->vertex[1])
                              * scale + translation;
                vtx.pos.y *= aspectScale; // Aspect undo.

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

            mdl->addFrame(frame);
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
            mdl->_lods.append(new ModelDetailLevel(*mdl, i));
            ModelDetailLevel &lod = *mdl->_lods.last();

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
        mdl->_vertexUsage.resize(info.numVertices * info.numLODs);
        mdl->_vertexUsage.fill(false);

        for(int i = 0; i < info.numLODs; ++i)
        for(int k = 0; k < lodInfo[i].numTriangles; ++k)
        for(int m = 0; m < 3; ++m)
        {
            int vertexIndex = SHORT(triangles[i][k].vertexIndices[m]);
            mdl->_vertexUsage.setBit(vertexIndex * info.numLODs + i);
        }

        delete [] lodInfo;
        for(int i = 0; i < info.numLODs; ++i)
        {
            M_Free(triangles[i]);
        }
        delete [] triangles;

        return mdl;
    }

    static Model *interpretDmd(de::FileHandle &hndl, float aspectScale)
    {
        if(recogniseDmd(hndl))
        {
            LOG_VERBOSE("Interpreted \"" + NativePath(hndl.file().composePath()).pretty() + "\" as a DMD model.");
            return loadDmd(hndl, aspectScale);
        }
        return 0;
    }

    static Model *interpretMd2(de::FileHandle &hndl, float aspectScale)
    {
        if(recogniseMd2(hndl))
        {
            LOG_VERBOSE("Interpreted \"" + NativePath(hndl.file().composePath()).pretty() + "\" as a MD2 model.");
            return loadMd2(hndl, aspectScale);
        }
        return 0;
    }

#if 0
    /**
     * Calculate vertex normals. Only with -renorm.
     */
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
};

Model::Model(Flags flags)
    : _numVertices(0)
    , d(new Instance(this))
{
    setFlags(flags, de::ReplaceFlags);
}

bool Model::recognise(de::FileHandle &hndl) //static
{
    if(recogniseDmd(hndl)) return true;
    if(recogniseMd2(hndl)) return true;
    return false;
}

struct ModelFileType
{
    String name; ///< Symbolic name of the resource type.
    String ext;  ///< Known file extension.

    Model *(*interpretFunc)(de::FileHandle &hndl, float aspectScale);
};

Model *Model::loadFromFile(de::FileHandle &hndl, float aspectScale) //static
{
    // Recognised file types.
    static ModelFileType modelTypes[] = {
        { "DMD",    ".dmd",     Instance::interpretDmd },
        { "MD2",    ".md2",     Instance::interpretMd2 },
        { "",       "",         0 } // Terminate.
    };

    // Firstly, attempt to guess the resource type from the file extension.
    ModelFileType *rtypeGuess = 0;
    // An extension is required for this.
    String filePath = hndl.file().composePath();
    String ext      = filePath.fileNameExtension();
    if(!ext.isEmpty())
    {
        for(int i = 0; !modelTypes[i].name.isEmpty(); ++i)
        {
            ModelFileType &type = modelTypes[i];
            if(!type.ext.compareWithoutCase(ext))
            {
                rtypeGuess = &type;
                if(Model *mdl = type.interpretFunc(hndl, aspectScale))
                {
                    return mdl;
                }
                break;
            }
        }
    }

    // Not yet interpreted - try each recognisable format in order.
    // Try each recognisable format instead.
    for(int i = 0; !modelTypes[i].name.isEmpty(); ++i)
    {
        ModelFileType const &modelType = modelTypes[i];

        // Already tried this?
        if(&modelType == rtypeGuess) continue;

        if(Model *mdl = modelType.interpretFunc(hndl, aspectScale))
        {
            return mdl;
        }
    }

    return 0;
}

uint Model::modelId() const
{
    return d->modelId;
}

void Model::setModelId(uint newModelId)
{
    d->modelId = newModelId;
}

Model::Flags Model::flags() const
{
    return d->flags;
}

void Model::setFlags(Model::Flags flagsToChange, FlagOp operation)
{
    applyFlagOperation(d->flags, flagsToChange, operation);
}

int Model::frameNumber(String name) const
{
    if(!name.isEmpty())
    {
        for(int i = 0; i < d->frames.count(); ++i)
        {
            Frame *frame = d->frames.at(i);
            if(!frame->name.compareWithoutCase(name))
                return i;
        }
    }
    return -1; // Not found.
}

Model::Frame &Model::frame(int number) const
{
    if(hasFrame(number))
    {
        return *d->frames.at(number);
    }
    throw MissingFrameError("Model::frame", "Invalid frame number " + String::number(number) + ", valid range is " + Rangei(0, d->frames.count()).asText());
}

void Model::addFrame(Frame *newFrame)
{
    if(!newFrame) return;
    if(!d->frames.contains(newFrame))
    {
        d->frames.append(newFrame);
    }
}

Model::Frames const &Model::frames() const
{
    return d->frames;
}

void Model::clearAllFrames()
{
    qDeleteAll(d->frames);
    d->frames.clear();
}

int Model::skinNumber(String name) const
{
    if(!name.isEmpty())
    {
        for(int i = 0; i < d->skins.count(); ++i)
        {
            Skin const &skin = d->skins.at(i);
            if(!skin.name.compareWithoutCase(name))
                return i;
        }
    }
    return -1; // Not found.
}

Model::Skin &Model::skin(int number) const
{
    if(hasSkin(number))
    {
        return const_cast<Skin &>(d->skins.at(number));
    }
    throw MissingSkinError("Model::skin", "Invalid skin number " + String::number(number) + ", valid range is " + Rangei(0, d->skins.count()).asText());
}

Model::Skin &Model::newSkin(String name)
{
    if(int index = skinNumber(name) > 0)
    {
        return skin(index);
    }
    d->skins.append(ModelSkin(name));
    return d->skins.last();
}

Model::Skins const &Model::skins() const
{
    return d->skins;
}

void Model::clearAllSkins()
{
    d->skins.clear();
}

Model::DetailLevel &Model::lod(int level) const
{
    if(hasLod(level))
    {
        return *_lods.at(level);
    }
    throw MissingDetailLevelError("Model::lod", "Invalid detail level " + String::number(level) + ", valid range is " + Rangei(0, _lods.count()).asText());
}

Model::DetailLevels const &Model::lods() const
{
    return _lods;
}

Model::Primitives const &Model::primitives() const
{
    return lod(0).primitives;
}

int Model::vertexCount() const
{
    return _numVertices;
}
