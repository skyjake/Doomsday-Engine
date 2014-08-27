/** @file modeldrawable.cpp  Drawable specialized for 3D models.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/ModelDrawable"
#include "de/HeightMap"

#include <de/App>
#include <de/ByteArrayFile>
#include <de/Matrix>
#include <de/GLBuffer>
#include <de/GLProgram>
#include <de/GLState>
#include <de/GLUniform>

#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#include <assimp/Importer.hpp>
#include <assimp/LogStream.hpp>
#include <assimp/DefaultLogger.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <array>

namespace de {
namespace internal {

/// Adapter between de::File and Assimp.
class ImpIOStream : public Assimp::IOStream
{
public:
    ImpIOStream(ByteArrayFile const &file) : _file(file), _pos(0)
    {}

    size_t Read(void *pvBuffer, size_t pSize, size_t pCount)
    {
        size_t const num = pSize * pCount;
        _file.get(_pos, reinterpret_cast<IByteArray::Byte *>(pvBuffer), num);
        _pos += num;
        return pCount;
    }

    size_t Write(void const *, size_t, size_t)
    {
        throw Error("ImpIOStream::Write", "Writing not allowed");
    }

    aiReturn Seek(size_t pOffset, aiOrigin pOrigin)
    {
        switch(pOrigin)
        {
        case aiOrigin_SET:
            _pos = pOffset;
            break;

        case aiOrigin_CUR:
            _pos += pOffset;
            break;

        case aiOrigin_END:
            _pos = _file.size() - pOffset;
            break;

        default:
            break;
        }
        return aiReturn_SUCCESS;
    }

    size_t Tell() const
    {
        return _pos;
    }

    size_t FileSize() const
    {
        return _file.size();
    }

    void Flush() {}

private:
    ByteArrayFile const &_file;
    size_t _pos;
};

/// Adapter between FS2 and Assimp.
struct ImpIOSystem : public Assimp::IOSystem
{
    ImpIOSystem() {}
    ~ImpIOSystem() {}

    char getOsSeparator() const { return '/'; }

    bool Exists(char const *pFile) const
    {
        return App::rootFolder().has(pFile);
    }

    Assimp::IOStream *Open(char const *pFile, char const *)
    {
        String const path = pFile;
        DENG2_ASSERT(!path.contains("\\"));
        return new ImpIOStream(App::rootFolder().locate<ByteArrayFile const>(path));
    }

    void Close(Assimp::IOStream *pFile)
    {
        delete pFile;
    }
};

struct ImpLogger : public Assimp::LogStream
{
    void write(char const *message)
    {
        LOG_RES_VERBOSE("[ai] %s") << message;
    }

    static void registerLogger()
    {
        if(registered) return;

        registered = true;
        Assimp::DefaultLogger::get()->attachStream(
                    new ImpLogger,
                    Assimp::Logger::Info |
                    Assimp::Logger::Warn |
                    Assimp::Logger::Err);
    }

    static bool registered;
};

bool ImpLogger::registered = false;

struct DefaultImageLoader : public ModelDrawable::IImageLoader
{
    Image loadImage(String const &path)
    {
        File const &texFile = App::rootFolder().locate<File>(path);
        qDebug() << "loading image from" << texFile.description().toLatin1();
        return Image::fromData(texFile, texFile.name().fileNameExtension());
    }
};

static DefaultImageLoader defaultImageLoader;

} // namespace internal
using namespace internal;

#define MAX_BONES               64
#define MAX_BONES_PER_VERTEX    4

struct ModelVertex
{
    Vector3f pos;
    Vector4f boneIds;
    Vector4f boneWeights;
    Vector3f normal;
    Vector3f tangent;
    Vector3f bitangent;
    Vector2f texCoord;
    Vector4f texBounds;
    Vector4f texBounds2;
    Vector4f texBounds3;
    Vector4f texBounds4;

    LIBGUI_DECLARE_VERTEX_FORMAT(11)
};

AttribSpec const ModelVertex::_spec[11] = {
    { AttribSpec::Position,    3, GL_FLOAT, false, sizeof(ModelVertex), 0 },
    { AttribSpec::BoneIDs,     4, GL_FLOAT, false, sizeof(ModelVertex), 3 * sizeof(float) },
    { AttribSpec::BoneWeights, 4, GL_FLOAT, false, sizeof(ModelVertex), 7 * sizeof(float) },
    { AttribSpec::Normal,      3, GL_FLOAT, false, sizeof(ModelVertex), 11 * sizeof(float) },
    { AttribSpec::Tangent,     3, GL_FLOAT, false, sizeof(ModelVertex), 14 * sizeof(float) },
    { AttribSpec::Bitangent,   3, GL_FLOAT, false, sizeof(ModelVertex), 17 * sizeof(float) },
    { AttribSpec::TexCoord0,   2, GL_FLOAT, false, sizeof(ModelVertex), 20 * sizeof(float) },
    { AttribSpec::TexBounds0,  4, GL_FLOAT, false, sizeof(ModelVertex), 22 * sizeof(float) },
    { AttribSpec::TexBounds1,  4, GL_FLOAT, false, sizeof(ModelVertex), 26 * sizeof(float) },
    { AttribSpec::TexBounds2,  4, GL_FLOAT, false, sizeof(ModelVertex), 30 * sizeof(float) },
    { AttribSpec::TexBounds3,  4, GL_FLOAT, false, sizeof(ModelVertex), 34 * sizeof(float) }
};
LIBGUI_VERTEX_FORMAT_SPEC(ModelVertex, 38 * sizeof(float))

static Matrix4f convertMatrix(aiMatrix4x4 const &aiMat)
{
    return Matrix4f(&aiMat.a1).transpose();
}

/// Bone used for vertices that have no bones.
static String const DUMMY_BONE_NAME = "__deng_dummy-bone__";

static int const MAX_TEXTURES = 4;

DENG2_PIMPL(ModelDrawable)
{
    typedef GLBufferT<ModelVertex> VBuf;
    typedef QHash<String, int> AnimLookup;

    struct VertexBone
    {
        duint16 ids[MAX_BONES_PER_VERTEX];
        dfloat weights[MAX_BONES_PER_VERTEX];

        VertexBone()
        {
            zap(ids);
            zap(weights);
        }
    };

    struct BoneData
    {
        Matrix4f offset;
    };

    struct MaterialData
    {
        std::array<Id, MAX_TEXTURES> texIds {{ Id::None, Id::None, Id::None, Id::None }};
        QHash<TextureMap, String> custom;
    };

    Asset modelAsset;
    String sourcePath;
    Assimp::Importer importer;
    IImageLoader *imageLoader { &defaultImageLoader };
    aiScene const *scene { nullptr };

    Vector3f minPoint; ///< Bounds in default pose.
    Vector3f maxPoint;
    Matrix4f globalInverse;

    QVector<VertexBone> vertexBones; // indexed by vertex
    QHash<String, duint16> boneNameToIndex;
    QHash<String, aiNode const *> nodeNameToPtr;
    QVector<BoneData> bones; // indexed by bone index
    AnimLookup animNameToIndex;

    std::array<TextureMap, MAX_TEXTURES> textureOrder {{ Diffuse, Unknown, Unknown, Unknown }};
    std::array<Id, MAX_TEXTURES> defaultTexIds {{ Id::None, Id::None, Id::None, Id::None }};
    QVector<MaterialData> materials; // indexed by material index

    bool needMakeBuffer { false };
    AtlasTexture *atlas { nullptr };
    VBuf *buffer        { nullptr };
    GLProgram *program  { nullptr };
    mutable GLUniform uBoneMatrices { "uBoneMatrices", GLUniform::Mat4Array, MAX_BONES };

    Instance(Public *i) : Base(i)
    {
        // Use FS2 for file access.
        importer.SetIOHandler(new ImpIOSystem);

        // Get most kinds of log output.
        ImpLogger::registerLogger();
    }

    ~Instance()
    {
        glDeinit();
    }

    void import(File const &file)
    {
        LOG_RES_MSG("Loading model from %s") << file.description();

        scene = 0;
        sourcePath = file.path();

        // Read the model file and apply suitable postprocessing to clean up the data.
        if(!importer.ReadFile(sourcePath.toLatin1(),
                              aiProcess_CalcTangentSpace |
                              aiProcess_GenSmoothNormals |
                              aiProcess_JoinIdenticalVertices |
                              aiProcess_Triangulate |
                              aiProcess_GenUVCoords |
                              aiProcess_FlipUVs |
                              aiProcess_SortByPType))
        {
            throw LoadError("ModelDrawable::import", String("Failed to load model from %s: %s")
                            .arg(file.description()).arg(importer.GetErrorString()));
        }

        scene = importer.GetScene();

        initBones();

        globalInverse = convertMatrix(scene->mRootNode->mTransformation).inverse();
        maxPoint      = Vector3f(1.0e-9f, 1.0e-9f, 1.0e-9f);
        minPoint      = Vector3f(1.0e9f,  1.0e9f,  1.0e9f);

        // Determine the total bounding box.
        for(duint i = 0; i < scene->mNumMeshes; ++i)
        {
            aiMesh const &mesh = *scene->mMeshes[i];
            for(duint i = 0; i < mesh.mNumVertices; ++i)
            {
                addToBounds(Vector3f(&mesh.mVertices[i].x));
            }
        }

        // Print some information.
        qDebug() << "total bones:" << boneCount();

        // Animations.
        animNameToIndex.clear();
        //qDebug() << "animations:" << scene->mNumAnimations;
        for(duint i = 0; i < scene->mNumAnimations; ++i)
        {
            //qDebug() << "  anim #" << i << "name:" << scene->mAnimations[i]->mName.C_Str();
            String const name = scene->mAnimations[i]->mName.C_Str();
            if(!name.isEmpty())
            {
                animNameToIndex.insert(name, i);
            }
        }

        // Create a lookup for node names.
        nodeNameToPtr.clear();
        nodeNameToPtr.insert("", scene->mRootNode);
        buildNodeLookup(*scene->mRootNode);

        // Prepare material information.
        qDebug() << "materials:" << scene->mNumMaterials;
        for(duint i = 0; i < scene->mNumMaterials; ++i)
        {
            materials << MaterialData();
        }
    }

    void buildNodeLookup(aiNode const &node)
    {
        String const name = node.mName.C_Str();
        if(!name.isEmpty())
        {
            nodeNameToPtr.insert(name, &node);
        }

        for(duint i = 0; i < node.mNumChildren; ++i)
        {
            buildNodeLookup(*node.mChildren[i]);
        }
    }

    /// Release all loaded model data.
    void clear()
    {
        glDeinit();

        sourcePath.clear();
        materials.clear();
        importer.FreeScene();
        scene = 0;
    }

    void glInit()
    {
        DENG2_ASSERT_IN_MAIN_THREAD();

        // Has a scene been imported successfully?
        if(!scene) return;

        if(modelAsset.isReady())
        {
            // Already good to go.
            return;
        }

        // Last minute notification in case some additional setup is needed.
        DENG2_FOR_PUBLIC_AUDIENCE2(AboutToGLInit, i)
        {
            i->modelAboutToGLInit(self);
        }

        // Materials.
        initTextures();

        // Initialize all meshes in the scene into a single GL buffer.
        makeBuffer();

        // Ready to go!
        modelAsset.setState(Ready);
    }

    void glDeinit()
    {
        releaseTexturesFromAtlas();

        /// @todo Free all GL resources.
        delete buffer;
        buffer = 0;

        clearBones();

        modelAsset.setState(NotReady);
    }

    void addToBounds(Vector3f const &point)
    {
        minPoint = minPoint.min(point);
        maxPoint = maxPoint.max(point);
    }

    bool isDefaultTexture(Id const &id) const
    {
        for(Id const &defTex : defaultTexIds)
        {
            if(id == defTex) return true;
        }
        return false;
    }

    void releaseTexture(Id const &id)
    {
        if(!id || isDefaultTexture(id)) return; // We don't own this, don't release.

        qDebug() << "Releasing model texture" << id.asText();
        atlas->release(id);
    }

    void releaseTexturesFromAtlas()
    {
        if(!atlas) return;

        foreach(MaterialData const &tex, materials)
        {
            for(Id const &id : tex.texIds) releaseTexture(id);
        }
        materials.clear();
    }

    int findMaterial(String const &name) const
    {
        if(!scene) return -1;
        for(duint i = 0; i < scene->mNumMaterials; ++i)
        {
            aiMaterial const &material = *scene->mMaterials[i];
            aiString matName;
            if(material.Get(AI_MATKEY_NAME, matName) == AI_SUCCESS)
            {
                if(name == matName.C_Str()) return i;
            }
        }
        return -1;
    }

    static TextureMap textureMapType(aiTextureType type)
    {
        switch(type)
        {
        case aiTextureType_DIFFUSE:  return Diffuse;
        case aiTextureType_NORMALS:  return Normals;
        case aiTextureType_HEIGHT:   return Height;
        case aiTextureType_SPECULAR: return Specular;
        case aiTextureType_EMISSIVE: return Emissive;
        default:
            DENG2_ASSERT(!"Unsupported texture type");
            return Diffuse;
        }
    }

    static aiTextureType impTextureType(TextureMap map)
    {
        switch(map)
        {
        case Diffuse:  return aiTextureType_DIFFUSE;
        case Normals:  return aiTextureType_NORMALS;
        case Height:   return aiTextureType_HEIGHT;
        case Specular: return aiTextureType_SPECULAR;
        case Emissive: return aiTextureType_EMISSIVE;
        default:
            break;
        }
        return aiTextureType_UNKNOWN;
    }

    void fallBackToDefaultTexture(MaterialData &mat, TextureMap map)
    {
        if(!mat.texIds[map]) mat.texIds[map] = defaultTexIds[map];
    }

    /**
     * Load all the textures of the model. The textures are allocated into the
     * atlas provided to the model.
     */
    void initTextures()
    {
        DENG2_ASSERT(atlas != 0);

        for(duint i = 0; i < scene->mNumMaterials; ++i)
        {            
            qDebug() << "  material #" << i;

            auto &mat = materials[i];

            // Load all known types of textures, falling back to defaults.
            loadTextureImage(i, aiTextureType_DIFFUSE);
            fallBackToDefaultTexture(mat, Diffuse);

            loadTextureImage(i, aiTextureType_NORMALS);
            if(!mat.texIds[Normals])
            {
                // Try a height field instead. This will be converted to a normal map.
                loadTextureImage(i, aiTextureType_HEIGHT);
            }
            fallBackToDefaultTexture(mat, Normals);

            loadTextureImage(i, aiTextureType_SPECULAR);
            fallBackToDefaultTexture(mat, Specular);

            loadTextureImage(i, aiTextureType_EMISSIVE);
            fallBackToDefaultTexture(mat, Emissive);
        }
    }

    /**
     * Attempts to load a texture image specified in the material. Also checks if
     * an overridden custom path is provided, though.
     *
     * @param materialId  Material index.
     * @param type        AssImp texture type.
     */
    void loadTextureImage(duint materialId, aiTextureType type)
    {
        DENG2_ASSERT(imageLoader != 0);

        TextureMap map = textureMapType(type);
        aiMaterial const &material = *scene->mMaterials[materialId];
        MaterialData const &data = materials[materialId];

        try
        {
            // Custom override path?
            if(data.custom.contains(map))
            {
                qDebug() << "loading custom path" << data.custom[map];
                return setTexture(materialId, map, imageLoader->loadImage(data.custom[map]));
            }
        }
        catch(Error const &er)
        {
            LOG_RES_WARNING("Failed to load custom model texture (type %i): %s")
                    << type << er.asText();
        }

        qDebug() << "    type:" << type
                 << "count:" << material.GetTextureCount(type);

        // Load the texture based on the information specified in the model's materials.
        aiString texPath;
        for(duint s = 0; s < material.GetTextureCount(type); ++s)
        {
            if(material.GetTexture(type, s, &texPath) == AI_SUCCESS)
            {
                qDebug() << "    texture #" << s << texPath.C_Str();

                try
                {
                    setTexture(materialId, map, imageLoader->loadImage(sourcePath.fileNamePath() /
                                                                       String(texPath.C_Str())));
                    break;
                }
                catch(Error const &er)
                {
                    LOG_RES_WARNING("Failed to load model texture (type %i): %s")
                            << type << er.asText();
                }
            }
        }
    }

    /**
     * Sets a custom texture that will be loaded when the model is glInited.
     *
     * @param materialId  Material id.
     * @param tex         Texture map.
     * @param path        Image file path.
     */
    void setCustomTexture(duint materialId, TextureMap map, String const &path)
    {
        DENG2_ASSERT(!atlas);
        DENG2_ASSERT(materialId < duint(materials.size()));

        materials[materialId].custom.insert(map, path);
    }

    void setTexture(duint materialId, TextureMap map, Image const &content)
    {
        if(!scene) return;
        if(materialId >= scene->mNumMaterials) return;
        if(map == Unknown) return;

        DENG2_ASSERT(atlas != 0);

        MaterialData &tex = materials[materialId];
        Id &id = (map == Height? tex.texIds[Normals] : tex.texIds[map]);

        // Release a previously loaded texture.
        if(id)
        {
            releaseTexture(id);
            id = Id::None;
        }

        if(map == Height)
        {
            // Convert the height map into a normal map.
            HeightMap heightMap;
            heightMap.loadGrayscale(content);
            id = atlas->alloc(heightMap.makeNormalMap());
        }
        else
        {
            id = atlas->alloc(content);
        }

        // The buffer needs to be updated with the new texture bounds.
        needMakeBuffer = true;
    }

    // Bone & Mesh Setup ----------------------------------------------------------------

    void clearBones()
    {
        vertexBones.clear();
        bones.clear();
        boneNameToIndex.clear();
    }

    int boneCount() const
    {
        return bones.size();
    }

    int addBone(String const &name)
    {
        int idx = boneCount();
        bones << BoneData();
        boneNameToIndex[name] = idx;
        return idx;
    }

    int findBone(String const &name) const
    {
        if(boneNameToIndex.contains(name))
        {
            return boneNameToIndex[name];
        }
        return -1;
    }

    int addOrFindBone(String const &name)
    {
        int i = findBone(name);
        if(i >= 0)
        {
            return i;
        }
        return addBone(name);
    }

    void addVertexWeight(duint vertexIndex, duint16 boneIndex, dfloat weight)
    {
        VertexBone &vb = vertexBones[vertexIndex];
        for(int i = 0; i < MAX_BONES_PER_VERTEX; ++i)
        {
            if(vb.weights[i] == 0.f)
            {
                // Here's a free one.
                vb.ids[i] = boneIndex;
                vb.weights[i] = weight;
                return;
            }
        }
        DENG2_ASSERT(!"Too many bone weights for a vertex");
    }

    /**
     * Initializes the per-vertex bone weight information, and indexes the bones
     * of the mesh in a sequential order.
     *
     * @param mesh        Source mesh.
     * @param vertexBase  Index of the first vertex of the mesh.
     */
    void initMeshBones(aiMesh const &mesh, duint vertexBase)
    {
        vertexBones.resize(vertexBase + mesh.mNumVertices);

        if(mesh.HasBones())
        {
            // Mark the per-vertex bone weights.
            for(duint i = 0; i < mesh.mNumBones; ++i)
            {
                aiBone const &bone = *mesh.mBones[i];

                duint const boneIndex = addOrFindBone(bone.mName.C_Str());
                bones[boneIndex].offset = convertMatrix(bone.mOffsetMatrix);

                for(duint w = 0; w < bone.mNumWeights; ++w)
                {
                    addVertexWeight(vertexBase + bone.mWeights[w].mVertexId,
                                    boneIndex, bone.mWeights[w].mWeight);
                }
            }
        }
        else
        {
            // No bones; make one dummy bone so we can render it the same way.
            duint const boneIndex = addOrFindBone(DUMMY_BONE_NAME);
            bones[boneIndex].offset = Matrix4f();

            // All vertices fully affected by this bone.
            for(duint i = 0; i < mesh.mNumVertices; ++i)
            {
                addVertexWeight(vertexBase + i, boneIndex, 1.f);
            }
        }
    }

    /**
     * Initializes all bones in the scene.
     */
    void initBones()
    {
        clearBones();

        int base = 0;
        for(duint i = 0; i < scene->mNumMeshes; ++i)
        {
            aiMesh const &mesh = *scene->mMeshes[i];

            qDebug() << "initializing bones for mesh:" << mesh.mName.C_Str();
            qDebug() << "  bones:" << mesh.mNumBones;

            initMeshBones(mesh, base);
            base += mesh.mNumVertices;
        }
    }

    /**
     * Allocates and fills in the GL buffer containing vertex information.
     */
    void makeBuffer()
    {
        needMakeBuffer = false;

        VBuf::Vertices verts;
        VBuf::Indices indx;

        aiVector3D const zero(0, 0, 0);

        int base = 0;

        // All of the scene's meshes are combined into one GL buffer.
        for(duint m = 0; m < scene->mNumMeshes; ++m)
        {
            aiMesh const &mesh = *scene->mMeshes[m];

            // Load vertices into the buffer.
            for(duint i = 0; i < mesh.mNumVertices; ++i)
            {
                aiVector3D const *pos      = &mesh.mVertices[i];
                aiVector3D const *normal   = (mesh.HasNormals()? &mesh.mNormals[i] : &zero);
                aiVector3D const *texCoord = (mesh.HasTextureCoords(0)? &mesh.mTextureCoords[0][i] : &zero);
                aiVector3D const *tangent  = (mesh.HasTangentsAndBitangents()? &mesh.mTangents[i] : &zero);
                aiVector3D const *bitang   = (mesh.HasTangentsAndBitangents()? &mesh.mBitangents[i] : &zero);

                VBuf::Type v;

                v.pos        = Vector3f(pos->x, pos->y, pos->z);

                v.normal     = Vector3f(normal ->x, normal ->y, normal ->z);
                v.tangent    = Vector3f(tangent->x, tangent->y, tangent->z);
                v.bitangent  = Vector3f(bitang ->x, bitang ->y, bitang ->z);

                v.texCoord   = Vector2f(texCoord->x, texCoord->y);
                v.texBounds  = Vector4f(0, 0, 1, 1);
                v.texBounds2 = Vector4f(0, 0, 1, 1);
                v.texBounds3 = Vector4f(0, 0, 1, 1);
                v.texBounds4 = Vector4f(0, 0, 1, 1);

                /// @todo Add support for multiple UVs, not just mapping the same ones to
                /// different bounds. -jk

                if(scene->HasMaterials())
                {
                    Vector4f *texBounds[MAX_TEXTURES] {
                        &v.texBounds, &v.texBounds2, &v.texBounds3, &v.texBounds4
                    };

                    MaterialData const &material = materials[mesh.mMaterialIndex];
                    for(int t = 0; t < MAX_TEXTURES; ++t)
                    {
                        // Apply the specified order for the textures.
                        TextureMap map = textureOrder[t];
                        if(map < 0 || map >= MAX_TEXTURES) continue;

                        if(material.texIds[map])
                        {
                            *texBounds[t] = atlas->imageRectf(material.texIds[map]).xywh();
                        }
                    }
                }

                for(int b = 0; b < MAX_BONES_PER_VERTEX; ++b)
                {
                    v.boneIds[b]     = vertexBones[base + i].ids[b];
                    v.boneWeights[b] = vertexBones[base + i].weights[b];
                }

                verts << v;
            }

            // Get face indices.
            for(duint i = 0; i < mesh.mNumFaces; ++i)
            {
                aiFace const &face = mesh.mFaces[i];
                DENG2_ASSERT(face.mNumIndices == 3); // expecting triangles
                indx << face.mIndices[0] + base
                     << face.mIndices[1] + base
                     << face.mIndices[2] + base;
            }

            base += mesh.mNumVertices;
        }

        // Get rid of an earlier buffer.
        delete buffer;

        buffer = new VBuf;
        buffer->setVertices(verts, gl::Static);
        buffer->setIndices(gl::Triangles, indx, gl::Static);
    }

    // Animation ------------------------------------------------------------------------

    struct AccumData
    {
        ddouble time;
        aiAnimation const *anim;
        QVector<Matrix4f> finalTransforms;

        AccumData(int boneCount)
            : time(0)
            , anim(0)
            , finalTransforms(boneCount)
        {}

        aiNodeAnim const *findNodeAnim(aiNode const &node) const
        {
            for(duint i = 0; i < anim->mNumChannels; ++i)
            {
                aiNodeAnim const *na = anim->mChannels[i];
                if(na->mNodeName == node.mName)
                {
                    return na;
                }
            }
            return 0;
        }
    };

    void accumulateAnimationTransforms(ddouble time,
                                       aiAnimation const &anim,
                                       aiNode const &rootNode) const
    {
        ddouble const ticksPerSec = anim.mTicksPerSecond? anim.mTicksPerSecond : 25.0;
        ddouble const timeInTicks = time * ticksPerSec;

        AccumData data(boneCount());
        data.anim = &anim;
        data.time = std::fmod(timeInTicks, anim.mDuration);

        accumulateTransforms(rootNode, data);

        // Update the resulting matrices in the uniform.
        for(int i = 0; i < boneCount(); ++i)
        {
            uBoneMatrices.set(i, data.finalTransforms.at(i));
        }
    }

    void accumulateTransforms(aiNode const &node, AccumData &data,
                              Matrix4f const &parentTransform = Matrix4f()) const
    {
        Matrix4f nodeTransform = convertMatrix(node.mTransformation);

        if(aiNodeAnim const *anim = data.findNodeAnim(node))
        {
            // Interpolate for this point in time.
            Matrix4f const translation = Matrix4f::translate(interpolatePosition(data.time, *anim));
            Matrix4f const scaling     = Matrix4f::scale(interpolateScaling(data.time, *anim));
            Matrix4f const rotation    = convertMatrix(aiMatrix4x4(interpolateRotation(data.time, *anim).GetMatrix()));

            nodeTransform = translation * rotation * scaling;
        }

        Matrix4f globalTransform = parentTransform * nodeTransform;

        int boneIndex = findBone(String(node.mName.C_Str()));
        if(boneIndex >= 0)
        {
            data.finalTransforms[boneIndex] =
                    globalInverse * globalTransform * bones.at(boneIndex).offset;
        }

        // Descend to child nodes.
        for(duint i = 0; i < node.mNumChildren; ++i)
        {
            accumulateTransforms(*node.mChildren[i], data, globalTransform);
        }
    }

    template <typename Type>
    static duint findAnimKey(ddouble time, Type const *keys, duint count)
    {
        DENG2_ASSERT(count > 0);
        for(duint i = 0; i < count - 1; ++i)
        {
            if(time < keys[i + 1].mTime)
            {
                return i;
            }
        }
        DENG2_ASSERT(!"Failed to find animation key (invalid time?)");
        return 0;
    }

    static Vector3f interpolateVectorKey(ddouble time, aiVectorKey const *keys, duint at)
    {
        Vector3f const start(&keys[at]    .mValue.x);
        Vector3f const end  (&keys[at + 1].mValue.x);

        return start + (end - start) *
               float((time - keys[at].mTime) / (keys[at + 1].mTime - keys[at].mTime));
    }

    static aiQuaternion interpolateRotation(ddouble time, aiNodeAnim const &anim)
    {
        if(anim.mNumRotationKeys == 1)
        {
            return anim.mRotationKeys[0].mValue;
        }

        aiQuatKey const *key = anim.mRotationKeys + findAnimKey(time, anim.mRotationKeys, anim.mNumRotationKeys);;

        aiQuaternion interp;
        aiQuaternion::Interpolate(interp, key[0].mValue, key[1].mValue,
                                  (time - key[0].mTime) / (key[1].mTime - key[0].mTime));
        interp.Normalize();
        return interp;
    }

    static Vector3f interpolateScaling(ddouble time, aiNodeAnim const &anim)
    {
        if(anim.mNumScalingKeys == 1)
        {
            return Vector3f(&anim.mScalingKeys[0].mValue.x);
        }
        return interpolateVectorKey(time, anim.mScalingKeys,
                                    findAnimKey(time, anim.mScalingKeys,
                                                anim.mNumScalingKeys));
    }

    static Vector3f interpolatePosition(ddouble time, aiNodeAnim const &anim)
    {
        if(anim.mNumPositionKeys == 1)
        {
            return Vector3f(&anim.mPositionKeys[0].mValue.x);
        }
        return interpolateVectorKey(time, anim.mPositionKeys,
                                    findAnimKey(time, anim.mPositionKeys,
                                                anim.mNumPositionKeys));
    }

    // Drawing --------------------------------------------------------------------------

    void updateMatricesFromAnimation(Animator const *animation) const
    {
        if(!scene->HasAnimations() || !animation) return;

        // Apply all current animations.
        for(int i = 0; i < animation->count(); ++i)
        {
            Animator::Animation const &anim = animation->at(i);

            // The animation has been validated earlier.
            DENG2_ASSERT(duint(anim.animId) < scene->mNumAnimations);
            DENG2_ASSERT(nodeNameToPtr.contains(anim.node));

            accumulateAnimationTransforms(animation->currentTime(i),
                                          *scene->mAnimations[anim.animId],
                                          *nodeNameToPtr[anim.node]);
        }
    }

    void preDraw(Animator const *animation)
    {
        if(needMakeBuffer) makeBuffer();

        DENG2_ASSERT(program != 0);
        DENG2_ASSERT(buffer != 0);

        // Draw the meshes in this node.
        updateMatricesFromAnimation(animation);

        GLState::current().apply();

        program->bind(uBoneMatrices);
        program->beginUse();
    }

    void draw(Animator const *animation)
    {
        preDraw(animation);
        buffer->draw();
        postDraw();
    }

    void drawInstanced(GLBuffer const &attribs, Animator const *animation)
    {
        preDraw(animation);
        buffer->drawInstanced(attribs);
        postDraw();
    }

    void postDraw()
    {
        program->endUse();
        program->unbind(uBoneMatrices);
    }

    DENG2_PIMPL_AUDIENCE(AboutToGLInit)
};

DENG2_AUDIENCE_METHOD(ModelDrawable, AboutToGLInit)

ModelDrawable::TextureMap ModelDrawable::textToTextureMap(String const &text)
{
    struct { char const *text; TextureMap map; } const mappings[] {
        { "diffuse",  Diffuse  },
        { "normals",  Normals  },
        { "specular", Specular },
        { "emission", Emissive },
        { "height",   Height   }
    };

    for(auto const &mapping : mappings)
    {
        if(!text.compareWithoutCase(mapping.text))
            return mapping.map;
    }
    return Unknown;
}

ModelDrawable::ModelDrawable() : d(new Instance(this))
{
    *this += d->modelAsset;
}

void ModelDrawable::setImageLoader(IImageLoader &loader)
{
    d->imageLoader = &loader;
}

void ModelDrawable::useDefaultImageLoader()
{
    d->imageLoader = &defaultImageLoader;
}

void ModelDrawable::load(File const &file)
{
    LOG_AS("ModelDrawable");

    // Get rid of all existing data.
    clear();

    d->import(file);
}

void ModelDrawable::clear()
{
    glDeinit();
    d->clear();
}

int ModelDrawable::animationIdForName(String const &name) const
{
    Instance::AnimLookup::const_iterator found = d->animNameToIndex.constFind(name);
    if(found != d->animNameToIndex.constEnd())
    {
        return found.value();
    }
    return -1;
}

int ModelDrawable::animationCount() const
{
    if(!d->scene) return 0;
    return d->scene->mNumAnimations;
}

bool ModelDrawable::nodeExists(String const &name) const
{
    return d->nodeNameToPtr.contains(name);
}

void ModelDrawable::setAtlas(AtlasTexture &atlas)
{
    d->atlas = &atlas;
}

void ModelDrawable::unsetAtlas()
{
    d->releaseTexturesFromAtlas();
    d->atlas = 0;
}

ModelDrawable::Mapping ModelDrawable::diffuseNormalsSpecularEmission() // static
{
    return Mapping() << Diffuse << Normals << Specular << Emissive;
}

void ModelDrawable::setTextureMapping(Mapping mapsToUse)
{
    for(int i = 0; i < MAX_TEXTURES; ++i)
    {
        d->textureOrder[i] = (i < mapsToUse.size()? mapsToUse.at(i) : Unknown);

        // Height is an alias for normals.
        if(d->textureOrder[i] == Height) d->textureOrder[i] = Normals;
    }
    d->needMakeBuffer = true;
}

void ModelDrawable::setDefaultTexture(TextureMap textureType, Id const &atlasId)
{
    DENG2_ASSERT(textureType >= 0 && textureType < MAX_TEXTURES);
    if(textureType < 0 || textureType >= MAX_TEXTURES) return;

    d->defaultTexIds[textureType] = atlasId;
}

void ModelDrawable::glInit()
{
    d->glInit();
}

void ModelDrawable::glDeinit()
{
    d->glDeinit();
}

int ModelDrawable::materialId(String const &name) const
{
    return d->findMaterial(name);
}

void ModelDrawable::setTexturePath(int materialId, TextureMap tex, String const &path)
{
    if(d->atlas)
    {
        // Load immediately.
        d->setTexture(materialId, tex, d->imageLoader->loadImage(path));
    }
    else
    {
        // This will override what the model specifies.
        d->setCustomTexture(materialId, tex, path);
    }
}

void ModelDrawable::setProgram(GLProgram &program)
{
    d->program = &program;
}

void ModelDrawable::unsetProgram()
{
    d->program = 0;
}

void ModelDrawable::draw(Animator const *animation) const
{
    const_cast<ModelDrawable *>(this)->glInit();

    if(isReady() && d->program && d->atlas)
    {
        d->draw(animation);
    }
}

void ModelDrawable::drawInstanced(GLBuffer const &instanceAttribs,
                                  Animator const *animation) const
{
    const_cast<ModelDrawable *>(this)->glInit();

    if(isReady() && d->program && d->atlas)
    {
        d->drawInstanced(instanceAttribs, animation);
    }
}

Vector3f ModelDrawable::dimensions() const
{
    return d->maxPoint - d->minPoint;
}

Vector3f ModelDrawable::midPoint() const
{
    return (d->maxPoint + d->minPoint) / 2.f;
}

//---------------------------------------------------------------------------------------

DENG2_PIMPL_NOREF(ModelDrawable::Animator)
{
    ModelDrawable const *model;
    typedef QList<Animation> Animations;
    Animations anims;

    Instance(ModelDrawable const *mdl = 0) : model(mdl) {}

    Animation &add(Animation const &anim)
    {
        DENG2_ASSERT(model != 0);

        // Verify first.
        if(anim.animId < 0 || anim.animId >= model->animationCount())
        {
            throw InvalidError("ModelDrawable::Animator::add",
                               "Specified animation does not exist");
        }
        if(!model->nodeExists(anim.node))
        {
            throw InvalidError("ModelDrawable::Animator::add",
                               "Node '" + anim.node + "' does not exist");
        }

        anims.append(anim);
        return anims.last();
    }

    void stopByNode(String const &node)
    {
        QMutableListIterator<Animation> iter(anims);
        while(iter.hasNext())
        {
            iter.next();
            if(iter.value().node == node)
            {
                iter.remove();
            }
        }
    }

    bool isRunning(int animId, String const &rootNode) const
    {
        foreach(Animation const &anim, anims)
        {
            if(anim.animId == animId && anim.node == rootNode)
                return true;
        }
        return false;
    }
};

ModelDrawable::Animator::Animator() : d(new Instance)
{}

ModelDrawable::Animator::Animator(ModelDrawable const &model)
    : d(new Instance(&model))
{}

void ModelDrawable::Animator::setModel(ModelDrawable const &model)
{
    d->model = &model;
}

ModelDrawable const &ModelDrawable::Animator::model() const
{
    DENG2_ASSERT(d->model != 0);
    return *d->model;
}

int ModelDrawable::Animator::count() const
{
    return d->anims.size();
}

ModelDrawable::Animator::Animation const &ModelDrawable::Animator::at(int index) const
{
    return d->anims.at(index);
}

ModelDrawable::Animator::Animation &ModelDrawable::Animator::at(int index)
{
    return d->anims[index];
}

bool ModelDrawable::Animator::isRunning(String const &animName, String const &rootNode) const
{
    return d->isRunning(model().animationIdForName(animName), rootNode);
}

bool ModelDrawable::Animator::isRunning(int animId, String const &rootNode) const
{
    return d->isRunning(animId, rootNode);
}

ModelDrawable::Animator::Animation &
ModelDrawable::Animator::start(String const &animName, String const &rootNode)
{
    d->stopByNode(rootNode);

    Animation anim;
    anim.animId = model().animationIdForName(animName);
    anim.node   = rootNode;
    anim.time   = 0.0;
    return d->add(anim);
}

ModelDrawable::Animator::Animation &
ModelDrawable::Animator::start(int animId, String const &rootNode)
{
    d->stopByNode(rootNode);

    Animation anim;
    anim.animId = animId;
    anim.node   = rootNode;
    anim.time   = 0.0;
    return d->add(anim);
}

void ModelDrawable::Animator::stop(int index)
{
    d->anims.removeAt(index);
}

void ModelDrawable::Animator::clear()
{
    d->anims.clear();
}

void ModelDrawable::Animator::advanceTime(TimeDelta const &)
{
    // overridden
}

ddouble ModelDrawable::Animator::currentTime(int index) const
{
    return at(index).time;
}

} // namespace de
