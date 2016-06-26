/** @file modeldrawable.cpp  Drawable specialized for 3D models.
 *
 * @authors Copyright (c) 2014-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "de/ImageFile"

#include <de/Animation>
#include <de/App>
#include <de/ByteArrayFile>
#include <de/GLBuffer>
#include <de/GLProgram>
#include <de/GLState>
#include <de/GLUniform>
#include <de/Matrix>
#include <de/NativePath>
#include <de/TextureBank>

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
        switch (pOrigin)
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

/**
 * Adapter between FS2 and Assimp. Each ModelDrawable instance has its own
 * instance of this class.
 */
struct ImpIOSystem : public Assimp::IOSystem
{
    /// Reference for resolving relative paths. This is the folder of the
    /// model currently being imported.
    String referencePath;

    ImpIOSystem() {}
    ~ImpIOSystem() {}

    char getOsSeparator() const { return '/'; }

    Path resolvePath(char const *fn) const
    {
        Path path(fn);
        if (path.isAbsolute()) return path;
        return referencePath / path;
    }

    bool Exists(char const *pFile) const
    {
        return App::rootFolder().has(resolvePath(pFile));
    }

    Assimp::IOStream *Open(char const *pFile, char const *)
    {
        Path const path = resolvePath(pFile);
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
        LOG_GL_VERBOSE("[ai] %s") << message;
    }

    static void registerLogger()
    {
        if (registered) return;

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
        Image img = App::rootFolder().locate<ImageFile>(path).image();
        if (img.depth() == 24)
        {
            // Model texture atlases need to have an alpha channel.
            DENG2_ASSERT(img.canConvertToQImage());
            return Image(img.toQImage().convertToFormat(QImage::Format_ARGB32));
        }
        return img;
    }
};

static DefaultImageLoader defaultImageLoader;

} // namespace internal
using namespace internal;

static int const MAX_BONES = 64;
static int const MAX_BONES_PER_VERTEX = 4;
static int const MAX_TEXTURES = 4;

struct ModelVertex
{
    Vector3f pos;
    Vector4f color;
    Vector4f boneIds;
    Vector4f boneWeights;
    Vector3f normal;
    Vector3f tangent;
    Vector3f bitangent;
    Vector2f texCoord;
    Vector4f texBounds[4];

    LIBGUI_DECLARE_VERTEX_FORMAT(12)
};

AttribSpec const ModelVertex::_spec[12] = {
    { AttribSpec::Position,    3, GL_FLOAT, false, sizeof(ModelVertex),  0 },
    { AttribSpec::Color,       4, GL_FLOAT, false, sizeof(ModelVertex),  3 * sizeof(float) },
    { AttribSpec::BoneIDs,     4, GL_FLOAT, false, sizeof(ModelVertex),  7 * sizeof(float) },
    { AttribSpec::BoneWeights, 4, GL_FLOAT, false, sizeof(ModelVertex), 11 * sizeof(float) },
    { AttribSpec::Normal,      3, GL_FLOAT, false, sizeof(ModelVertex), 15 * sizeof(float) },
    { AttribSpec::Tangent,     3, GL_FLOAT, false, sizeof(ModelVertex), 18 * sizeof(float) },
    { AttribSpec::Bitangent,   3, GL_FLOAT, false, sizeof(ModelVertex), 21 * sizeof(float) },
    { AttribSpec::TexCoord0,   2, GL_FLOAT, false, sizeof(ModelVertex), 24 * sizeof(float) },
    { AttribSpec::TexBounds0,  4, GL_FLOAT, false, sizeof(ModelVertex), 26 * sizeof(float) },
    { AttribSpec::TexBounds1,  4, GL_FLOAT, false, sizeof(ModelVertex), 30 * sizeof(float) },
    { AttribSpec::TexBounds2,  4, GL_FLOAT, false, sizeof(ModelVertex), 34 * sizeof(float) },
    { AttribSpec::TexBounds3,  4, GL_FLOAT, false, sizeof(ModelVertex), 38 * sizeof(float) },
};
LIBGUI_VERTEX_FORMAT_SPEC(ModelVertex, 42 * sizeof(float))

static Matrix4f convertMatrix(aiMatrix4x4 const &aiMat)
{
    return Matrix4f(&aiMat.a1).transpose();
}

static ddouble secondsToTicks(ddouble seconds, aiAnimation const &anim)
{
    ddouble const ticksPerSec = anim.mTicksPerSecond? anim.mTicksPerSecond : 25.0;
    return seconds * ticksPerSec;
}

static ddouble ticksToSeconds(ddouble ticks, aiAnimation const &anim)
{
    return ticks / secondsToTicks(1.0, anim);
}

/// Bone used for vertices that have no bones.
static String const DUMMY_BONE_NAME = "__deng_dummy-bone__";

DENG2_PIMPL(ModelDrawable)
{
    typedef GLBufferT<ModelVertex> VBuf;
    typedef QHash<String, int> AnimLookup;

    static TextureMap textureMapType(aiTextureType type)
    {
        switch (type)
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
        switch (map)
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

    Asset modelAsset;
    String sourcePath;
    ImpIOSystem *importerIoSystem; // not owned
    Assimp::Importer importer;
    aiScene const *scene { nullptr };

    Vector3f minPoint; ///< Bounds in default pose.
    Vector3f maxPoint;
    Matrix4f globalInverse;

    QVector<VertexBone> vertexBones; // indexed by vertex
    QHash<String, duint16> boneNameToIndex;
    QHash<String, aiNode const *> nodeNameToPtr;
    QVector<BoneData> bones; // indexed by bone index
    AnimLookup animNameToIndex;
    QVector<Rangeui> meshIndexRanges;

    /**
     * Management of texture maps.
     */
    struct GLData
    {
        /**
         * Each material has its own VBO with a particular, fixed set of texture
         * coordinates.
         */
        struct Material
        {
            struct MeshTextures
            {
                Id::Type texIds[MAX_TEXTURES];
                QHash<TextureMap, String> customPaths;

                MeshTextures() { zap(texIds); }
            };

            QVector<MeshTextures> meshTextures; // indexed by mesh index
            std::unique_ptr<VBuf> buffer;
        };

        /**
         * Source information for a texture used in one or more of the meshes.
         * Depends on the ModelDrawable's IImageLoader.
         */
        struct TextureSource : public TextureBank::ImageSource
        {
            GLData *d;

            TextureSource(String const &path, GLData *glData)
                : ImageSource(Path(path))
                , d(glData) {}

            Image load() const
            {
                return d->imageLoader->loadImage(sourcePath().toString());
            }
        };

        Id::Type defaultTexIds[MAX_TEXTURES];  ///< Used if no other texture is provided.
        TextureMap textureOrder[MAX_TEXTURES]; ///< Order of textures for vertex buffer texcoords.
        IImageLoader *imageLoader { &defaultImageLoader };

        TextureBank textureBank;
        QList<Material *> materials; // owned
        bool needMakeBuffer { false };

        String sourcePath; ///< Location of the model file (imported with Assimp).
        aiScene const *scene { nullptr };

        GLData()
        {
            // We use file paths as identifiers.
            textureBank.setSeparator('/');

            textureOrder[0] = Diffuse;
            textureOrder[1] = textureOrder[2] = textureOrder[3] = Unknown;
            zap(defaultTexIds);
        }

        ~GLData()
        {
            qDeleteAll(materials);
        }

        void initMaterials()
        {
            deinitMaterials();
            addMaterial(); // default is at index zero
        }

        void deinitMaterials()
        {
            qDeleteAll(materials);
            materials.clear();
        }

        duint addMaterial()
        {
            DENG2_ASSERT(scene != nullptr);

            // Each material has its own GLBuffer.
            needMakeBuffer = true;

            Material *material = new Material;
            for (duint i = 0; i < scene->mNumMeshes; ++i)
            {
                material->meshTextures << Material::MeshTextures();
            }
            materials << material;
            return materials.size() - 1;
        }

        void glInit(String modelSourcePath)
        {
            sourcePath = modelSourcePath;

            // Materials.
            initTextures();
        }

        void glDeinit()
        {
            releaseTexturesFromAtlas();
            //deinitMaterials();
            //scene = nullptr;
        }

        void releaseTexture(Id const &id)
        {
            if (!id) return; // We don't own this, don't release.

            Path texPath = textureBank.sourcePathForAtlasId(id);
            DENG2_ASSERT(!texPath.isEmpty());

            LOGDEV_GL_VERBOSE("Releasing model texture '%s' path: \"%s\"")
                    << id.asText() << texPath;
            textureBank.unload(texPath);
        }

        void releaseTexturesFromAtlas()
        {
            textureBank.unloadAll(Bank::ImmediatelyInCurrentThread);
            for (Material *mat : materials)
            {
                for (auto &mesh : mat->meshTextures)
                {
                    zap(mesh.texIds);
                }
            }
            textureBank.clear();
        }

        void fallBackToDefaultTexture(Material::MeshTextures &mesh,
                                      TextureMap map) const
        {
            if (!mesh.texIds[map])
            {
                mesh.texIds[map] = defaultTexIds[map];
            }
        }

        /**
         * Load all the textures of the model, for all materials. The textures
         * are allocated into the atlas provided to the model.
         *
         * Only a single copy of each texture image is kept in the atlas even
         * if the same image is beig used in many meshes.
         */
        void initTextures()
        {
            for (duint matIdx = 0; matIdx < duint(materials.size()); ++matIdx)
            {
                for (duint i = 0; i < scene->mNumMeshes; ++i)
                {
                    MeshId const mesh(i, matIdx);
                    auto &textures = materials[matIdx]->meshTextures[i];

                    // Load all known types of textures, falling back to defaults.
                    loadTextureImage(mesh, aiTextureType_DIFFUSE);
                    fallBackToDefaultTexture(textures, Diffuse);

                    loadTextureImage(mesh, aiTextureType_NORMALS);
                    if (!textures.texIds[Normals])
                    {
                        // Try a height field instead. This will be converted to a normal map.
                        loadTextureImage(mesh, aiTextureType_HEIGHT);
                    }
                    fallBackToDefaultTexture(textures, Normals);

                    loadTextureImage(mesh, aiTextureType_SPECULAR);
                    fallBackToDefaultTexture(textures, Specular);

                    loadTextureImage(mesh, aiTextureType_EMISSIVE);
                    fallBackToDefaultTexture(textures, Emissive);
                }
            }
            // All textures loaded.
            textureBank.atlas()->commit();
        }

        /**
         * Attempts to load a texture image specified in the material. Also checks if
         * an overridden custom path is provided, though.
         *
         * @param mesh  Identifies the mesh whose texture is being loaded.
         * @param type  AssImp texture type.
         */
        void loadTextureImage(MeshId const &mesh, aiTextureType type)
        {
            DENG2_ASSERT(imageLoader != 0);

            aiMesh     const &sceneMesh     = *scene->mMeshes[mesh.index];
            aiMaterial const &sceneMaterial = *scene->mMaterials[sceneMesh.mMaterialIndex];

            auto const &meshTextures = materials.at(mesh.material)->meshTextures[mesh.index];
            TextureMap const map = textureMapType(type);

            try
            {
                // Custom override path?
                if (meshTextures.customPaths.contains(map))
                {
                    LOG_GL_VERBOSE("Loading custom path \"%s\"") << meshTextures.customPaths[map];
                    return setTexture(mesh, map, meshTextures.customPaths[map]);
                }
            }
            catch (Error const &er)
            {
                LOG_GL_WARNING("Failed to load user-defined %s texture for "
                               "mesh %i (material %i): %s")
                        << textureMapToText(textureMapType(type))
                        << mesh.index << mesh.material
                        << er.asText();
            }

            // Load the texture based on the information specified in the model's materials.
            aiString texPath;
            for (duint s = 0; s < sceneMaterial.GetTextureCount(type); ++s)
            {
                try
                {
                    if (sceneMaterial.GetTexture(type, s, &texPath) == AI_SUCCESS)
                    {
                        setTexture(mesh, map, sourcePath.fileNamePath() / NativePath(texPath.C_Str()));
                        break;
                    }
                }
                catch (Error const &er)
                {
                    LOG_GL_WARNING("Failed to load %s texture for mesh %i "
                                   "(material %i) based on info from model file: %s")
                            << textureMapToText(textureMapType(type))
                            << mesh.index << mesh.material << er.asText();
                }
            }
        }

        void setTexture(MeshId const &mesh, TextureMap map, String contentPath)
        {
            if (!scene) return;
            if (map == Unknown) return; // Ignore unmapped textures.
            if (mesh.material >= duint(materials.size())) return;
            if (mesh.index >= scene->mNumMeshes) return;

            DENG2_ASSERT(textureBank.atlas());

            Material &material = *materials[mesh.material];
            auto &meshTextures = material.meshTextures[mesh.index];

            Id::Type &destId = (map == Height? meshTextures.texIds[Normals] :
                                               meshTextures.texIds[map]);

            /// @todo Release a previously loaded texture, if it isn't used
            /// in any material. -jk

            if (map == Height)
            {
                // Convert the height map into a normal map.
                contentPath = contentPath.concatenatePath("HeightMap.toNormals");
            }

            Path const path(contentPath);

            // If this image is unknown, add it now to the bank.
            if (!textureBank.has(path))
            {
                textureBank.add(path, new TextureSource(contentPath, this));
            }

            LOGDEV_GL_VERBOSE("material: %i mesh: %i file: \"%s\"")
                    << mesh.material
                    << mesh.index
                    << textureMapToText(map)
                    << contentPath;

            destId = textureBank.texture(path);

            // The buffer needs to be updated with the new texture bounds.
            needMakeBuffer = true;
        }

        void setTextureMapping(Mapping mapsToUse)
        {
            for (int i = 0; i < MAX_TEXTURES; ++i)
            {
                textureOrder[i] = (i < mapsToUse.size()? mapsToUse.at(i) : Unknown);

                // Height is an alias for normals.
                if (textureOrder[i] == Height) textureOrder[i] = Normals;
            }
            needMakeBuffer = true;
        }

        /**
         * Sets a custom texture that will be loaded when the model is glInited.
         *
         * @param matId  Material.
         * @param tex    Texture map.
         * @param path   Image file path.
         */
        void setCustomTexturePath(MeshId const &mesh, TextureMap map, String const &path)
        {
            DENG2_ASSERT(!textureBank.atlas()); // in-use textures cannot be replaced on the fly
            DENG2_ASSERT(mesh.index < scene->mNumMeshes);
            DENG2_ASSERT(mesh.material < duint(materials.size()));

            materials[mesh.material]->meshTextures[mesh.index].customPaths.insert(map, path);
        }
    };

    GLData glData;
    Passes defaultPasses;
    GLProgram *program { nullptr }; ///< Default program (overridden by pass shaders).

    mutable GLUniform uBoneMatrices { "uBoneMatrices", GLUniform::Mat4Array, MAX_BONES };

    Instance(Public *i) : Base(i)
    {
        // Use FS2 for file access.
        importer.SetIOHandler(importerIoSystem = new ImpIOSystem);

        // Get most kinds of log output.
        ImpLogger::registerLogger();
    }

    ~Instance()
    {
        glDeinit();
    }

    void import(File const &file)
    {
        LOG_GL_MSG("Loading model from %s") << file.description();

        /*
         * MD5: Multiple animation sequences are supported via multiple .md5anim files.
         * Autodetect if these exist and make a list of their names.
         */
        String anims;
        if (file.extension() == ".md5mesh")
        {
            String const baseName = file.name().fileNameWithoutExtension() + "_";
            file.parent()->forContents([&anims, &baseName] (String fileName, File &)
            {
                if (fileName.startsWith(baseName) &&
                    fileName.fileNameExtension() == ".md5anim")
                {
                    if (!anims.isEmpty()) anims += ";";
                    anims += fileName.substr(baseName.size()).fileNameWithoutExtension();
                }
                return LoopContinue;
            });
        }
        importer.SetPropertyString(AI_CONFIG_IMPORT_MD5_ANIM_SEQUENCE_NAMES, anims.toStdString());

        scene = glData.scene = nullptr;
        sourcePath = file.path();
        importerIoSystem->referencePath = sourcePath.fileNamePath();

        // Read the model file and apply suitable postprocessing to clean up the data.
        if (!importer.ReadFile(sourcePath.toUtf8(),
                              aiProcess_CalcTangentSpace |
                              aiProcess_GenSmoothNormals |
                              aiProcess_JoinIdenticalVertices |
                              aiProcess_Triangulate |
                              aiProcess_GenUVCoords |
                              aiProcess_FlipUVs |
                              aiProcess_SortByPType))
        {
            throw LoadError("ModelDrawable::import", String("Failed to load model from %1: %2")
                            .arg(file.description()).arg(importer.GetErrorString()));
        }

        scene = glData.scene = importer.GetScene();

        initBones();

        globalInverse = convertMatrix(scene->mRootNode->mTransformation).inverse();
        maxPoint      = Vector3f(1.0e-9f, 1.0e-9f, 1.0e-9f);
        minPoint      = Vector3f(1.0e9f,  1.0e9f,  1.0e9f);

        // Determine the total bounding box.
        for (duint i = 0; i < scene->mNumMeshes; ++i)
        {
            aiMesh const &mesh = *scene->mMeshes[i];
            for (duint i = 0; i < mesh.mNumVertices; ++i)
            {
                addToBounds(Vector3f(&mesh.mVertices[i].x));
            }
        }

        // Print some information.
        LOG_GL_VERBOSE("Bone count: %i\n"
                       "Animation count: %i")
                << boneCount()
                << scene->mNumAnimations;

        // Animations.
        animNameToIndex.clear();
        for (duint i = 0; i < scene->mNumAnimations; ++i)
        {
            LOG_GL_VERBOSE("Animation #%i name:%s") << i << scene->mAnimations[i]->mName.C_Str();

            String const name = scene->mAnimations[i]->mName.C_Str();
            if (!name.isEmpty())
            {
                animNameToIndex.insert(name, i);
            }
        }

        // Create a lookup for node names.
        nodeNameToPtr.clear();
        nodeNameToPtr.insert("", scene->mRootNode);
        buildNodeLookup(*scene->mRootNode);

        glData.initMaterials();

        // Default rendering passes to use if none specified.
        Pass pass;
        pass.meshes.resize(scene->mNumMeshes);
        pass.meshes.fill(true);
        defaultPasses << pass;
    }

    void buildNodeLookup(aiNode const &node)
    {
        String const name = node.mName.C_Str();
#ifdef DENG2_DEBUG
        qDebug() << "Node:" << name;
#endif
        if (!name.isEmpty())
        {
            nodeNameToPtr.insert(name, &node);
        }

        for (duint i = 0; i < node.mNumChildren; ++i)
        {
            buildNodeLookup(*node.mChildren[i]);
        }
    }

    /// Release all loaded model data.
    void clear()
    {
        glDeinit();

        sourcePath.clear();
        defaultPasses.clear();
        importer.FreeScene();
        scene = glData.scene = nullptr;
    }

    void glInit()
    {
        DENG2_ASSERT_IN_MAIN_THREAD();

        // Has a scene been imported successfully?
        if (!scene) return;

        if (modelAsset.isReady())
        {
            // Already good to go.
            return;
        }

        // Last minute notification in case some additional setup is needed.
        DENG2_FOR_PUBLIC_AUDIENCE2(AboutToGLInit, i)
        {
            i->modelAboutToGLInit(self);
        }

        glData.glInit(sourcePath);

        // Initialize all meshes in the scene into a single GL buffer.
        makeBuffer();

        // Ready to go!
        modelAsset.setState(Ready);
    }

    void glDeinit()
    {
        glData.glDeinit();
        clearBones();

        modelAsset.setState(NotReady);
    }

    void addToBounds(Vector3f const &point)
    {
        minPoint = minPoint.min(point);
        maxPoint = maxPoint.max(point);
    }

    int findMaterial(String const &name) const
    {
        if (!scene) return -1;
        for (duint i = 0; i < scene->mNumMaterials; ++i)
        {
            aiMaterial const &material = *scene->mMaterials[i];
            aiString matName;
            if (material.Get(AI_MATKEY_NAME, matName) == AI_SUCCESS)
            {
                if (name == matName.C_Str()) return i;
            }
        }
        return -1;
    }

//- Bone & Mesh Setup -------------------------------------------------------------------

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
        if (boneNameToIndex.contains(name))
        {
            return boneNameToIndex[name];
        }
        return -1;
    }

    int addOrFindBone(String const &name)
    {
        int i = findBone(name);
        if (i >= 0)
        {
            return i;
        }
        return addBone(name);
    }

    void addVertexWeight(duint vertexIndex, duint16 boneIndex, dfloat weight)
    {
        VertexBone &vb = vertexBones[vertexIndex];
        for (int i = 0; i < MAX_BONES_PER_VERTEX; ++i)
        {
            if (vb.weights[i] == 0.f)
            {
                // Here's a free one.
                vb.ids[i] = boneIndex;
                vb.weights[i] = weight;
                return;
            }
        }
        LOG_GL_WARNING("\"%s\": too many weights for vertex %i (only 4 supported), bone index: %i")
            << sourcePath << vertexIndex << boneIndex;
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

        if (mesh.HasBones())
        {
            // Mark the per-vertex bone weights.
            for (duint i = 0; i < mesh.mNumBones; ++i)
            {
                aiBone const &bone = *mesh.mBones[i];

                duint const boneIndex = addOrFindBone(bone.mName.C_Str());
                bones[boneIndex].offset = convertMatrix(bone.mOffsetMatrix);

                for (duint w = 0; w < bone.mNumWeights; ++w)
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
            for (duint i = 0; i < mesh.mNumVertices; ++i)
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
        for (duint i = 0; i < scene->mNumMeshes; ++i)
        {
            aiMesh const &mesh = *scene->mMeshes[i];

            LOGDEV_GL_VERBOSE("Initializing %i bones for mesh #%i %s")
                    << mesh.mNumBones << i << mesh.mName.C_Str();

            initMeshBones(mesh, base);
            base += mesh.mNumVertices;
        }
    }

    void makeBuffer()
    {
        glData.needMakeBuffer = false;
        for (GLData::Material *material : glData.materials)
        {
            makeBuffer(*material);
        }
    }

    /**
     * Allocates and fills in the GL buffer containing vertex information.
     *
     * @param material  Material whose vertex buffer to create.
     */
    void makeBuffer(GLData::Material &material)
    {
        VBuf::Vertices verts;
        VBuf::Indices indx;

        aiVector3D const zero(0, 0, 0);
        aiColor4D const white(1, 1, 1, 1);

        int base = 0;
        meshIndexRanges.clear();
        meshIndexRanges.resize(scene->mNumMeshes);

        // All of the scene's meshes are combined into one GL buffer.
        for (duint m = 0; m < scene->mNumMeshes; ++m)
        {
            aiMesh const &mesh = *scene->mMeshes[m];

            // Load vertices into the buffer.
            for (duint i = 0; i < mesh.mNumVertices; ++i)
            {
                aiVector3D const *pos      = &mesh.mVertices[i];
                aiColor4D  const *color    = (mesh.HasVertexColors(0)? &mesh.mColors[0][i] : &white);
                aiVector3D const *normal   = (mesh.HasNormals()? &mesh.mNormals[i] : &zero);
                aiVector3D const *texCoord = (mesh.HasTextureCoords(0)? &mesh.mTextureCoords[0][i] : &zero);
                aiVector3D const *tangent  = (mesh.HasTangentsAndBitangents()? &mesh.mTangents[i] : &zero);
                aiVector3D const *bitang   = (mesh.HasTangentsAndBitangents()? &mesh.mBitangents[i] : &zero);

                VBuf::Type v;

                v.pos       = Vector3f(pos->x, pos->y, pos->z);
                v.color     = Vector4f(color->r, color->g, color->b, color->a);

                v.normal    = Vector3f(normal ->x, normal ->y, normal ->z);
                v.tangent   = Vector3f(tangent->x, tangent->y, tangent->z);
                v.bitangent = Vector3f(bitang ->x, bitang ->y, bitang ->z);

                v.texCoord     = Vector2f(texCoord->x, texCoord->y);
                v.texBounds[0] = Vector4f(0, 0, 1, 1);
                v.texBounds[1] = Vector4f(0, 0, 1, 1);
                v.texBounds[2] = Vector4f(0, 0, 1, 1);
                v.texBounds[3] = Vector4f(0, 0, 1, 1);

                auto const &meshTextures = material.meshTextures[m];

                for (int t = 0; t < MAX_TEXTURES; ++t)
                {
                    // Apply the specified order for the textures.
                    TextureMap map = glData.textureOrder[t];
                    if (map < 0 || map >= MAX_TEXTURES) continue;

                    if (meshTextures.texIds[map])
                    {
                        v.texBounds[t] = glData.textureBank.atlas()->imageRectf(meshTextures.texIds[map]).xywh();
                    }
                    else if (glData.defaultTexIds[map])
                    {
                        v.texBounds[t] = glData.textureBank.atlas()->imageRectf(glData.defaultTexIds[map]).xywh();
                    }
                    else
                    {
                        // Not included in material.
                        v.texBounds[t] = Vector4f();
                    }
                }

                for (int b = 0; b < MAX_BONES_PER_VERTEX; ++b)
                {
                    v.boneIds[b]     = vertexBones[base + i].ids[b];
                    v.boneWeights[b] = vertexBones[base + i].weights[b];
                }

                verts << v;
            }

            duint firstFace = indx.size();

            // Get face indices.
            for (duint i = 0; i < mesh.mNumFaces; ++i)
            {
                aiFace const &face = mesh.mFaces[i];
                DENG2_ASSERT(face.mNumIndices == 3); // expecting triangles
                indx << face.mIndices[0] + base
                     << face.mIndices[1] + base
                     << face.mIndices[2] + base;
            }

            meshIndexRanges[m] = Rangeui::fromSize(firstFace, mesh.mNumFaces * 3);

            base += mesh.mNumVertices;
        }

        std::unique_ptr<VBuf> buf(new VBuf);
        buf->setVertices(verts, gl::Static);
        buf->setIndices(gl::Triangles, indx, gl::Static);
        material.buffer = std::move(buf);
    }

//- Animation ---------------------------------------------------------------------------

    struct AccumData
    {
        Animator const &animator;
        ddouble time = 0.0;
        aiAnimation const *anim = nullptr;
        QVector<Matrix4f> finalTransforms;

        AccumData(Animator const &animator, int boneCount)
            : animator(animator)
            , finalTransforms(boneCount)
        {}

        aiNodeAnim const *findNodeAnim(aiNode const &node) const
        {
            if (!anim) return nullptr;
            for (duint i = 0; i < anim->mNumChannels; ++i)
            {
                aiNodeAnim const *na = anim->mChannels[i];
                if (na->mNodeName == node.mName)
                {
                    return na;
                }
            }
            return nullptr;
        }
    };

    void accumulateAnimationTransforms(Animator const &animator,
                                       ddouble time,
                                       aiAnimation const *animSeq,
                                       aiNode const &rootNode) const
    {
        AccumData data(animator, boneCount());
        data.anim = animSeq;
        // Wrap animation time.
        data.time = animSeq? std::fmod(secondsToTicks(time, *animSeq), animSeq->mDuration) : time;

        accumulateTransforms(rootNode, data);

        // Update the resulting matrices in the uniform.
        for (int i = 0; i < boneCount(); ++i)
        {
            uBoneMatrices.set(i, data.finalTransforms.at(i));
        }
    }

    void accumulateTransforms(aiNode const &node, AccumData &data,
                              Matrix4f const &parentTransform = Matrix4f()) const
    {
        Matrix4f nodeTransform = convertMatrix(node.mTransformation);

        // Additional rotation?
        Vector4f const axisAngle = data.animator.extraRotationForNode(node.mName.C_Str());

        // Transform according to the animation sequence.
        if (aiNodeAnim const *anim = data.findNodeAnim(node))
        {
            // Interpolate for this point in time.
            Matrix4f const translation = Matrix4f::translate(interpolatePosition(data.time, *anim));
            Matrix4f const scaling     = Matrix4f::scale(interpolateScaling(data.time, *anim));
            Matrix4f       rotation    = convertMatrix(aiMatrix4x4(interpolateRotation(data.time, *anim).GetMatrix()));

            if (!fequal(axisAngle.w, 0))
            {
                // Include the custom extra rotation.
                rotation = Matrix4f::rotate(axisAngle.w, axisAngle) * rotation;
            }

            nodeTransform = translation * rotation * scaling;
        }
        else
        {
            // Model does not specify animation information for this node.
            // Only apply the possible additional rotation.
            if (!fequal(axisAngle.w, 0))
            {
                nodeTransform = Matrix4f::rotate(axisAngle.w, axisAngle) * nodeTransform;
            }
        }

        Matrix4f globalTransform = parentTransform * nodeTransform;

        int boneIndex = findBone(String(node.mName.C_Str()));
        if (boneIndex >= 0)
        {
            data.finalTransforms[boneIndex] =
                    globalInverse * globalTransform * bones.at(boneIndex).offset;
        }

        // Descend to child nodes.
        for (duint i = 0; i < node.mNumChildren; ++i)
        {
            accumulateTransforms(*node.mChildren[i], data, globalTransform);
        }
    }

    template <typename Type>
    static duint findAnimKey(ddouble time, Type const *keys, duint count)
    {
        DENG2_ASSERT(count > 0);
        for (duint i = 0; i < count - 1; ++i)
        {
            if (time < keys[i + 1].mTime)
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
        if (anim.mNumRotationKeys == 1)
        {
            return anim.mRotationKeys[0].mValue;
        }

        aiQuatKey const *key = anim.mRotationKeys + findAnimKey(time, anim.mRotationKeys, anim.mNumRotationKeys);

        aiQuaternion interp;
        aiQuaternion::Interpolate(interp, key[0].mValue, key[1].mValue,
                                  (time - key[0].mTime) / (key[1].mTime - key[0].mTime));
        interp.Normalize();
        return interp;
    }

    static Vector3f interpolateScaling(ddouble time, aiNodeAnim const &anim)
    {
        if (anim.mNumScalingKeys == 1)
        {
            return Vector3f(&anim.mScalingKeys[0].mValue.x);
        }
        return interpolateVectorKey(time, anim.mScalingKeys,
                                    findAnimKey(time, anim.mScalingKeys,
                                                anim.mNumScalingKeys));
    }

    static Vector3f interpolatePosition(ddouble time, aiNodeAnim const &anim)
    {
        if (anim.mNumPositionKeys == 1)
        {
            return Vector3f(&anim.mPositionKeys[0].mValue.x);
        }
        return interpolateVectorKey(time, anim.mPositionKeys,
                                    findAnimKey(time, anim.mPositionKeys,
                                                anim.mNumPositionKeys));
    }

    void updateMatricesFromAnimation(Animator const *animator) const
    {
        // Cannot do anything without an Animator.
        if (!animator) return;

        if (!scene->HasAnimations() || !animator->count())
        {
            // If requested, run through the bone transformations even when
            // no animations are active.
            if (animator->flags().testFlag(Animator::AlwaysTransformNodes))
            {
                accumulateAnimationTransforms(*animator, 0, nullptr, *scene->mRootNode);
                return;
            }
        }

        // Apply all current animations.
        for (int i = 0; i < animator->count(); ++i)
        {
            auto const &animSeq = animator->at(i);

            // The animation has been validated earlier.
            DENG2_ASSERT(duint(animSeq.animId) < scene->mNumAnimations);
            DENG2_ASSERT(nodeNameToPtr.contains(animSeq.node));

            accumulateAnimationTransforms(*animator,
                                          animator->currentTime(i),
                                          scene->mAnimations[animSeq.animId],
                                          *nodeNameToPtr[animSeq.node]);
        }
    }

//- Drawing -----------------------------------------------------------------------------

    GLProgram *drawProgram = nullptr;
    Pass const *drawPass = nullptr;

    void preDraw(Animator const *animation)
    {
        if (glData.needMakeBuffer) makeBuffer();

        DENG2_ASSERT(drawProgram == nullptr);

        // Draw the meshes in this node.
        updateMatricesFromAnimation(animation);

        GLState::current().apply();
    }

    void setDrawProgram(GLProgram *prog, Appearance const *appearance = nullptr)
    {
        if (drawProgram)
        {
            drawProgram->unbind(uBoneMatrices);
            if (appearance && appearance->programCallback)
            {
                appearance->programCallback(*drawProgram, Unbound);
            }
        }

        drawProgram = prog;

        if (drawProgram)
        {
            if (appearance && appearance->programCallback)
            {
                appearance->programCallback(*drawProgram, AboutToBind);
            }
            drawProgram->bind(uBoneMatrices);
        }
    }

    void initRanges(GLBuffer::DrawRanges &ranges, QBitArray const &meshes)
    {
        Rangeui current;
        for (int i = 0; i < meshIndexRanges.size(); ++i)
        {
            if (!meshes.at(i)) continue;
            auto const &mesh = meshIndexRanges.at(i);
            if (current.isEmpty())
            {
                current = mesh;
            }
            else if (current.end == mesh.start)
            {
                // Combine.
                current.end = mesh.end;
            }
            else
            {
                // Need a new range.
                ranges.append(current);
                current = mesh;
            }
        }
        // The final range.
        if (!current.isEmpty())
        {
            ranges.append(current);
        }
    }

    void draw(Appearance const *appearance, Animator const *animation)
    {
        Passes const *passes = appearance && appearance->drawPasses?
                    appearance->drawPasses : &defaultPasses;
        preDraw(animation);

        try
        {
            GLBuffer::DrawRanges ranges;
            for (int i = 0; i < passes->size(); ++i)
            {
                Pass const &pass = passes->at(i);

                // Is this pass disabled?
                if (appearance && !appearance->passMask.isEmpty() &&
                                 !appearance->passMask.testBit(i))
                {
                    continue;
                }

                drawPass = &pass;
                setDrawProgram(pass.program? pass.program : program, appearance);
                if (!drawProgram)
                {
                    throw ProgramError("ModelDrawable::draw",
                                       QString("Rendering pass %1 (\"%2\") has no shader program")
                                        .arg(i).arg(pass.name));
                }

                if (appearance && appearance->passCallback)
                {
                    appearance->passCallback(pass, PassBegun);
                }

                duint material = 0;
                if (appearance && appearance->passMaterial.size() >= passes->size())
                {
                    material = appearance->passMaterial.at(i);
                }

                ranges.clear();
                initRanges(ranges, pass.meshes);

                GLState::push()
                        .setBlendFunc (pass.blendFunc)
                        .setBlendOp   (pass.blendOp)
                        .setDepthTest (pass.depthFunc != gl::Always)
                        .setDepthFunc (pass.depthFunc)
                        .setDepthWrite(pass.depthWrite)
                        .apply();
                {
                    drawProgram->beginUse();
                    glData.materials.at(material)->buffer->draw(&ranges);
                    drawProgram->endUse();
                }
                GLState::pop();

                if (appearance && appearance->passCallback)
                {
                    appearance->passCallback(pass, PassEnded);
                }
            }
        }
        catch (Error const &er)
        {
            LOG_GL_ERROR("Failed to draw model \"%s\": %s")
                    << sourcePath
                    << er.asText();
        }

        postDraw();
    }

    void drawInstanced(GLBuffer const &attribs, Animator const *animation)
    {
        /// @todo Rendering passes for instanced drawing. -jk

        preDraw(animation);
        setDrawProgram(program);
        glData.materials.at(0)->buffer->drawInstanced(attribs);
        postDraw();
    }

    void postDraw()
    {
        setDrawProgram(nullptr);
        drawPass = nullptr;
    }

    DENG2_PIMPL_AUDIENCE(AboutToGLInit)
};

DENG2_AUDIENCE_METHOD(ModelDrawable, AboutToGLInit)

namespace internal {

static struct {
    char const *text;
    ModelDrawable::TextureMap map;
} const mappings[] {
    { "diffuse",  ModelDrawable::Diffuse  },
    { "normals",  ModelDrawable::Normals  },
    { "specular", ModelDrawable::Specular },
    { "emission", ModelDrawable::Emissive },
    { "height",   ModelDrawable::Height   },
    { "unknown",  ModelDrawable::Unknown  }
};

} // namespace internal

ModelDrawable::TextureMap ModelDrawable::textToTextureMap(String const &text) // static
{
    for (auto const &mapping : internal::mappings)
    {
        if (!text.compareWithoutCase(mapping.text))
            return mapping.map;
    }
    return Unknown;
}

String ModelDrawable::textureMapToText(TextureMap map) // static
{
    for (auto const &mapping : internal::mappings)
    {
        if (mapping.map == map)
            return mapping.text;
    }
    return "unknown";
}

ModelDrawable::ModelDrawable() : d(new Instance(this))
{
    *this += d->modelAsset;
}

void ModelDrawable::setImageLoader(IImageLoader &loader)
{
    d->glData.imageLoader = &loader;
}

void ModelDrawable::useDefaultImageLoader()
{
    d->glData.imageLoader = &defaultImageLoader;
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
    if (found != d->animNameToIndex.constEnd())
    {
        return found.value();
    }
    return -1;
}

String ModelDrawable::animationName(int id) const
{
    if (!d->scene || id < 0 || id >= int(d->scene->mNumAnimations))
    {
        return "";
    }
    String const name = d->scene->mAnimations[id]->mName.C_Str();
    if (name.isEmpty())
    {
        return QString("@%1").arg(id);
    }
    return name;
}

int ModelDrawable::animationCount() const
{
    if (!d->scene) return 0;
    return d->scene->mNumAnimations;
}

int ModelDrawable::meshCount() const
{
    if (!d->scene) return 0;
    return d->scene->mNumMeshes;
}

int ModelDrawable::meshId(String const &name) const
{
    if (!d->scene) return -1;
    for (duint i = 0; i < d->scene->mNumMeshes; ++i)
    {
        if (name == d->scene->mMeshes[i]->mName.C_Str())
        {
            return i;
        }
    }
    return -1;
}

String ModelDrawable::meshName(int id) const
{
    if (!d->scene || id < 0 || id >= int(d->scene->mNumMeshes))
    {
        return "";
    }
    String const name = d->scene->mMeshes[id]->mName.C_Str();
    if (name.isEmpty())
    {
        return QString("@%1").arg(id);
    }
    return name;
}

bool ModelDrawable::nodeExists(String const &name) const
{
    return d->nodeNameToPtr.contains(name);
}

void ModelDrawable::setAtlas(IAtlas &atlas)
{
    d->glData.textureBank.setAtlas(&atlas);
}

void ModelDrawable::unsetAtlas()
{
    d->glData.releaseTexturesFromAtlas();
    d->glData.textureBank.setAtlas(nullptr);
}

IAtlas *ModelDrawable::atlas() const
{
    return d->glData.textureBank.atlas();
}

ModelDrawable::Mapping ModelDrawable::diffuseNormalsSpecularEmission() // static
{
    return Mapping() << Diffuse << Normals << Specular << Emissive;
}

duint ModelDrawable::addMaterial()
{
    // This should only be done when the asset is not in use.
    DENG2_ASSERT(!d->modelAsset.isReady());

    return d->glData.addMaterial();
}

void ModelDrawable::resetMaterials()
{
    // This should only be done when the asset is not in use.
    DENG2_ASSERT(!d->modelAsset.isReady());

    d->glData.deinitMaterials();
    d->glData.initMaterials();
}

void ModelDrawable::setTextureMapping(Mapping mapsToUse)
{
    d->glData.setTextureMapping(mapsToUse);
}

void ModelDrawable::setDefaultTexture(TextureMap textureType, Id const &atlasId)
{
    DENG2_ASSERT(textureType >= 0 && textureType < MAX_TEXTURES);
    if (textureType < 0 || textureType >= MAX_TEXTURES) return;

    d->glData.defaultTexIds[textureType] = atlasId;
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

void ModelDrawable::setTexturePath(MeshId const &mesh, TextureMap tex, String const &path)
{
    if (d->glData.textureBank.atlas())
    {
        // Load immediately.
        d->glData.setTexture(mesh, tex, path);
    }
    else
    {
        // This will override what the model specifies.
        d->glData.setCustomTexturePath(mesh, tex, path);
    }
}

void ModelDrawable::setProgram(GLProgram *program)
{
    d->program = program;
}

GLProgram *ModelDrawable::program() const
{
    return d->program;
}

void ModelDrawable::draw(Appearance const *appearance,
                           Animator const *animation) const
{
    const_cast<ModelDrawable *>(this)->glInit();

    if (isReady() && d->glData.textureBank.atlas())
    {
        d->draw(appearance, animation);
    }
}

void ModelDrawable::drawInstanced(GLBuffer const &instanceAttribs,
                                  Animator const *animation) const
{
    const_cast<ModelDrawable *>(this)->glInit();

    if (isReady() && d->program && d->glData.textureBank.atlas())
    {
        d->drawInstanced(instanceAttribs, animation);
    }
}

ModelDrawable::Pass const *ModelDrawable::currentPass() const
{
    return d->drawPass;
}

GLProgram *ModelDrawable::currentProgram() const
{
    return d->drawProgram;
}

Vector3f ModelDrawable::dimensions() const
{
    return d->maxPoint - d->minPoint;
}

Vector3f ModelDrawable::midPoint() const
{
    return (d->maxPoint + d->minPoint) / 2.f;
}

int ModelDrawable::Passes::findName(String const &name) const
{
    for (int i = 0; i < size(); ++i)
    {
        if (at(i).name == name) // case sensitive
            return i;
    }
    return -1;
}

} // namespace de

//---------------------------------------------------------------------------------------

namespace de {

DENG2_PIMPL_NOREF(ModelDrawable::Animator)
, DENG2_OBSERVES(Asset, Deletion)
{
    Constructor constructor;
    ModelDrawable const *model = nullptr;
    QList<OngoingSequence *> anims;
    Flags flags = DefaultFlags;

    Instance(Constructor ctr, ModelDrawable const *mdl = 0)
        : constructor(ctr)
    {
        setModel(mdl);
    }

    ~Instance()
    {
        setModel(nullptr);
        qDeleteAll(anims);
    }

    void setModel(ModelDrawable const *mdl)
    {
        if (model) model->audienceForDeletion() -= this;
        model = mdl;
        if (model) model->audienceForDeletion() += this;
    }

    void assetBeingDeleted(Asset &a)
    {
        if (model == &a) model = nullptr;
    }

    OngoingSequence &add(OngoingSequence *seq)
    {
        DENG2_ASSERT(seq != nullptr);
        DENG2_ASSERT(model != nullptr);

        // Verify first.
        if (seq->animId < 0 || seq->animId >= model->animationCount())
        {
            throw InvalidError("ModelDrawable::Animator::add",
                               "Specified animation does not exist");
        }
        if (!model->nodeExists(seq->node))
        {
            throw InvalidError("ModelDrawable::Animator::add",
                               "Node '" + seq->node + "' does not exist");
        }

        anims.append(seq);
        return *anims.last();
    }

    void stopByNode(String const &node)
    {
        QMutableListIterator<OngoingSequence *> iter(anims);
        while (iter.hasNext())
        {
            iter.next();
            if (iter.value()->node == node)
            {
                delete iter.value();
                iter.remove();
            }
        }
    }

    OngoingSequence const *findAny(String const &rootNode) const
    {
        foreach (OngoingSequence const *anim, anims)
        {
            if (anim->node == rootNode)
                return anim;
        }
        return nullptr;
    }

    OngoingSequence const *find(int animId, String const &rootNode) const
    {
        foreach (OngoingSequence const *anim, anims)
        {
            if (anim->animId == animId && anim->node == rootNode)
                return anim;
        }
        return nullptr;
    }

    bool isRunning(int animId, String const &rootNode) const
    {
        return find(animId, rootNode) != nullptr;
    }
};

ModelDrawable::Animator::Animator(Constructor constructor)
    : d(new Instance(constructor))
{}

ModelDrawable::Animator::Animator(ModelDrawable const &model, Constructor constructor)
    : d(new Instance(constructor, &model))
{}

void ModelDrawable::Animator::setModel(ModelDrawable const &model)
{
    d->setModel(&model);
}

void ModelDrawable::Animator::setFlags(Flags const &flags, FlagOp op)
{
    applyFlagOperation(d->flags, flags, op);
}

ModelDrawable::Animator::Flags ModelDrawable::Animator::flags() const
{
    return d->flags;
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

ModelDrawable::Animator::OngoingSequence const &
ModelDrawable::Animator::at(int index) const
{
    return *d->anims.at(index);
}

ModelDrawable::Animator::OngoingSequence &
ModelDrawable::Animator::at(int index)
{
    return *d->anims[index];
}

bool ModelDrawable::Animator::isRunning(String const &animName, String const &rootNode) const
{
    return d->isRunning(model().animationIdForName(animName), rootNode);
}

bool ModelDrawable::Animator::isRunning(int animId, String const &rootNode) const
{
    return d->isRunning(animId, rootNode);
}

ModelDrawable::Animator::OngoingSequence *ModelDrawable::Animator::find(String const &rootNode) const
{
    return const_cast<OngoingSequence *>(d->findAny(rootNode));
}

ModelDrawable::Animator::OngoingSequence *ModelDrawable::Animator::find(int animId, String const &rootNode) const
{
    return const_cast<OngoingSequence *>(d->find(animId, rootNode));
}

ModelDrawable::Animator::OngoingSequence &
ModelDrawable::Animator::start(String const &animName, String const &rootNode)
{
    return start(model().animationIdForName(animName), rootNode);
}

ModelDrawable::Animator::OngoingSequence &
ModelDrawable::Animator::start(int animId, String const &rootNode)
{
    d->stopByNode(rootNode);

    aiScene const &scene = *model().d->scene;

    if (animId < 0 || animId >= int(scene.mNumAnimations))
    {
        throw InvalidError("ModelDrawable::Animator::start",
                           QString("Invalid animation ID %1").arg(animId));
    }

    auto const &animData = *scene.mAnimations[animId];

    OngoingSequence *anim = d->constructor();
    anim->animId = animId;
    anim->node   = rootNode;
    anim->time   = 0.0;
    anim->duration = ticksToSeconds(animData.mDuration, animData);
    anim->initialize();
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
    auto const &anim = at(index);
    ddouble t = anim.time;
    if (anim.flags.testFlag(OngoingSequence::ClampToDuration))
    {
        t = min(t, anim.duration - de::FLOAT_EPSILON);
    }
    return t;
}

Vector4f ModelDrawable::Animator::extraRotationForNode(String const &) const
{
    return Vector4f();
}

void ModelDrawable::Animator::OngoingSequence::initialize()
{}

bool ModelDrawable::Animator::OngoingSequence::atEnd() const
{
    return time >= duration;
}

ModelDrawable::Animator::OngoingSequence *
ModelDrawable::Animator::OngoingSequence::make() // static
{
    return new OngoingSequence;
}

} // namespace de

uint qHash(de::ModelDrawable::Pass const &pass)
{
    return qHash(pass.name);
}
