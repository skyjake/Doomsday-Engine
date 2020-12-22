/** @file modeldrawable.cpp  Drawable specialized for 3D models.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/modeldrawable.h"
#include "de/heightmap.h"
#include "de/imagefile.h"

#include <de/animation.h>
#include <de/app.h>
#include <de/bytearrayfile.h>
#include <de/folder.h>
#include <de/glbuffer.h>
#include <de/glprogram.h>
#include <de/glstate.h>
#include <de/gluniform.h>
#include <de/matrix.h>
#include <de/texturebank.h>
#include <de/hash.h>

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
    ImpIOStream(const ByteArrayFile &file) : _file(file), _pos(0)
    {}

    size_t Read(void *pvBuffer, size_t pSize, size_t pCount)
    {
        const size_t num = pSize * pCount;
        _file.get(_pos, reinterpret_cast<IByteArray::Byte *>(pvBuffer), num);
        _pos += num;
        return pCount;
    }

    size_t Write(const void *, size_t, size_t)
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
    const ByteArrayFile &_file;
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

    Path resolvePath(const char *fn) const
    {
        Path path(fn);
        if (path.isAbsolute()) return path;
        return referencePath / path;
    }

    bool Exists(const char *pFile) const
    {
        return App::rootFolder().has(resolvePath(pFile));
    }

    Assimp::IOStream *Open(const char *pFile, const char *)
    {
        const Path path = resolvePath(pFile);
        return new ImpIOStream(App::rootFolder().locate<ByteArrayFile const>(path));
    }

    void Close(Assimp::IOStream *pFile)
    {
        delete pFile;
    }
};

struct ImpLogger : public Assimp::LogStream
{
    void write(const char *message)
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
    Image loadImage(const String &path)
    {
        Image img = App::rootFolder().locate<ImageFile>(path).image();
        if (img.depth() == 24)
        {
            // Model texture atlases need to have an alpha channel.
            //DE_ASSERT(img.canConvertToQImage());
            return img.convertToFormat(Image::RGBA_8888);
        }
        return img;
    }
};

static DefaultImageLoader defaultImageLoader;

} // namespace internal
using namespace internal;

static const int MAX_BONES = 64;
static const int MAX_BONES_PER_VERTEX = 4;
static const int MAX_TEXTURES = 4;

static ModelDrawable::TextureMap const TEXTURE_MAP_TYPES[4]{
    ModelDrawable::Diffuse,
    ModelDrawable::Normals,
    ModelDrawable::Specular,
    ModelDrawable::Emissive,
};

struct ModelVertex
{
    Vec3f pos;
    Vec4f color;
    Vec4f boneIds;
    Vec4f boneWeights;
    Vec3f normal;
    Vec3f tangent;
    Vec3f bitangent;
    Vec2f texCoord;
    Vec4f texBounds[4];

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
    { AttribSpec::TexCoord,    2, GL_FLOAT, false, sizeof(ModelVertex), 24 * sizeof(float) },
    { AttribSpec::TexBounds0,  4, GL_FLOAT, false, sizeof(ModelVertex), 26 * sizeof(float) },
    { AttribSpec::TexBounds1,  4, GL_FLOAT, false, sizeof(ModelVertex), 30 * sizeof(float) },
    { AttribSpec::TexBounds2,  4, GL_FLOAT, false, sizeof(ModelVertex), 34 * sizeof(float) },
    { AttribSpec::TexBounds3,  4, GL_FLOAT, false, sizeof(ModelVertex), 38 * sizeof(float) },
};
LIBGUI_VERTEX_FORMAT_SPEC(ModelVertex, 42 * sizeof(float))

static Mat4f convertMatrix(const aiMatrix4x4 &aiMat)
{
    return Mat4f(&aiMat.a1).transpose();
}

static ddouble secondsToTicks(ddouble seconds, const aiAnimation &anim)
{
    const ddouble ticksPerSec = anim.mTicksPerSecond != 0.f? anim.mTicksPerSecond : 25.0;
    return seconds * ticksPerSec;
}

static ddouble ticksToSeconds(ddouble ticks, const aiAnimation &anim)
{
    return ticks / secondsToTicks(1.0, anim);
}

/// Bone used for vertices that have no bones.
static String const DUMMY_BONE_NAME{"__deng_dummy-bone__"};

DE_PIMPL(ModelDrawable)
{
    typedef GLBufferT<ModelVertex> VBuf;
    typedef Hash<String, int> AnimLookup;

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
            DE_ASSERT_FAIL("Unsupported texture type");
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
        Mat4f offset;
    };

    Asset            modelAsset;
    String           sourcePath;
    ImpIOSystem *    importerIoSystem; // not owned
    std::unique_ptr<Assimp::Importer> importer;
    const aiScene *  scene{nullptr};

    Vec3f minPoint; ///< Bounds in default pose.
    Vec3f maxPoint;
    Mat4f globalInverse;

    List<VertexBone>             vertexBones; // indexed by vertex
    Hash<String, duint16>        boneNameToIndex;
    Hash<String, const aiNode *> nodeNameToPtr;
    List<BoneData>               bones; // indexed by bone index
    AnimLookup                   animNameToIndex;
    List<Rangez>                 meshIndexRanges;

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
                Hash<int /*TextureMap*/, String> customPaths;

                MeshTextures() { zap(texIds); }
            };

            List<MeshTextures> meshTextures; // indexed by mesh index
            std::unique_ptr<VBuf> buffer;
        };

        /**
         * Source information for a texture used in one or more of the meshes.
         * Depends on the ModelDrawable's IImageLoader.
         */
        struct TextureSource : public TextureBank::ImageSource
        {
            GLData *d;

            TextureSource(TextureMap texMap, const String &path, GLData *glData)
                : ImageSource(int(texMap == Height? Normals : texMap), Path(path))
                , d(glData) {}

            Image load() const
            {
                return d->imageLoader->loadImage(sourcePath().toString());
            }
        };

        Id::Type   defaultTexIds[MAX_TEXTURES]; ///< Used if no other texture is provided.
        TextureMap textureOrder[MAX_TEXTURES];  ///< Order of textures for vertex buffer texcoords.
        IImageLoader *imageLoader{&defaultImageLoader};

        TextureBank      textureBank;
        List<Material *> materials; // owned
        bool             needMakeBuffer{false};

        String         sourcePath; ///< Location of the model file (imported with Assimp).
        const aiScene *scene{nullptr};

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
            deleteAll(materials);
        }

        void initMaterials()
        {
            deinitMaterials();
            addMaterial(); // default is at index zero
        }

        void deinitMaterials()
        {
            deleteAll(materials);
            materials.clear();
        }

        dsize addMaterial()
        {
            DE_ASSERT(scene != nullptr);

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

        void glInit(const String &modelSourcePath)
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

        void releaseTexture(const Id &id)
        {
            if (!id) return; // We don't own this, don't release.

            Path texPath = textureBank.sourcePathForAtlasId(id);
            DE_ASSERT(!texPath.isEmpty());

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
        void loadTextureImage(const MeshId &mesh, aiTextureType type)
        {
            DE_ASSERT(imageLoader != nullptr);

            const aiMesh &sceneMesh     = *scene->mMeshes[mesh.index];
            const aiMaterial &sceneMaterial = *scene->mMaterials[sceneMesh.mMaterialIndex];

            const auto &meshTextures = materials.at(mesh.material)->meshTextures[mesh.index];
            const TextureMap texMap = textureMapType(type);

            try
            {
                // Custom override path?
                if (meshTextures.customPaths.contains(texMap))
                {
                    LOG_GL_VERBOSE("Loading custom path \"%s\"") << meshTextures.customPaths[texMap];
                    return setTexture(mesh, texMap, meshTextures.customPaths[texMap]);
                }
            }
            catch (const Error &er)
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
                        setTexture(
                            mesh,
                            texMap,
                            Path::normalizeString(sourcePath.fileNamePath() / texPath.C_Str()));
                        break;
                    }
                }
                catch (const Error &er)
                {
                    LOG_GL_WARNING("Failed to load %s texture for mesh %i "
                                   "(material %i) based on info from model file: %s")
                            << textureMapToText(textureMapType(type))
                            << mesh.index << mesh.material << er.asText();
                }
            }
        }

        void setTexture(const MeshId &mesh, TextureMap texMap, String contentPath)
        {
            if (!scene) return;
            if (texMap == Unknown) return; // Ignore unmapped textures.
            if (mesh.material >= duint(materials.size())) return;
            if (mesh.index >= scene->mNumMeshes) return;

            DE_ASSERT(textureBank.atlas());

            Material &material = *materials[mesh.material];
            auto &meshTextures = material.meshTextures[mesh.index];

            Id::Type &destId = (texMap == Height? meshTextures.texIds[Normals] :
                                                  meshTextures.texIds[texMap]);

            /// @todo Release a previously loaded texture, if it isn't used
            /// in any material. -jk

            if (texMap == Height)
            {
                // Convert the height map into a normal map.
                contentPath = contentPath.concatenatePath("HeightMap.toNormals");
            }

            const Path path(contentPath);

            // If this image is unknown, add it now to the bank.
            if (!textureBank.has(path))
            {
                textureBank.add(path, new TextureSource(texMap, contentPath, this));
            }

            LOGDEV_GL_VERBOSE("material: %i mesh: %i file: \"%s\"")
                    << mesh.material
                    << mesh.index
                    << textureMapToText(texMap)
                    << contentPath;

            destId = textureBank.texture(path).id;

            // The buffer needs to be updated with the new texture bounds.
            needMakeBuffer = true;
        }

        void setTextureMapping(const Mapping &mapsToUse)
        {
            for (int i = 0; i < MAX_TEXTURES; ++i)
            {
                textureOrder[i] = (i < mapsToUse.sizei()? mapsToUse.at(i) : Unknown);

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
        void setCustomTexturePath(const MeshId &mesh, TextureMap texMap, const String &path)
        {
            DE_ASSERT(!textureBank.atlas(texMap)); // in-use textures cannot be replaced on the fly
            DE_ASSERT(mesh.index < scene->mNumMeshes);
            DE_ASSERT(mesh.material < duint(materials.size()));

            materials[mesh.material]->meshTextures[mesh.index].customPaths.insert(texMap, path);
        }
    };

    GLData     glData;
    Passes     defaultPasses;
    GLProgram *program{nullptr}; ///< Default program (overridden by pass shaders).

    mutable GLUniform uBoneMatrices { "uBoneMatrices", GLUniform::Mat4Array, MAX_BONES };

    Impl(Public *i) : Base(i)
    {
        // Get most kinds of log output.
        ImpLogger::registerLogger();
    }

    ~Impl()
    {
        glDeinit();
    }

    void import(const File &file)
    {
        LOG_GL_MSG("Loading model from %s") << file.description();

        // Use FS2 for file access.
        importer.reset(new Assimp::Importer);
        importer->SetIOHandler(importerIoSystem = new ImpIOSystem);

#if defined (DE_HAVE_CUSTOMIZED_ASSIMP)
        {
            /*
             * MD5: Multiple animation sequences are supported via multiple .md5anim files.
             * Autodetect if these exist and make a list of their names.
             */
            String anims;
            if (file.extension() == ".md5mesh")
            {
                const String baseName = file.name().fileNameWithoutExtension() + "_";
                file.parent()->forContents([&anims, &baseName] (String fileName, File &)
                {
                    if (fileName.beginsWith(baseName) &&
                        fileName.fileNameExtension() == ".md5anim")
                    {
                        if (!anims.isEmpty()) anims += ";";
                        anims += fileName.substr(baseName.sizeb()).fileNameWithoutExtension();
                    }
                    return LoopContinue;
                });
            }
            importer->SetPropertyString(AI_CONFIG_IMPORT_MD5_ANIM_SEQUENCE_NAMES,
                                        anims.toStdString());
        }
#endif

        scene = glData.scene = nullptr;
        sourcePath = file.path();
        importerIoSystem->referencePath = sourcePath.fileNamePath();

        // Read the model file and apply suitable postprocessing to clean up the data.
        if (!importer->ReadFile(sourcePath.c_str(),
                                aiProcess_CalcTangentSpace |
                                aiProcess_GenSmoothNormals |
                                aiProcess_JoinIdenticalVertices |
                                aiProcess_Triangulate |
                                aiProcess_GenUVCoords |
                                aiProcess_FlipUVs |
                                aiProcess_SortByPType))
        {
            throw LoadError("ModelDrawable::import",
                            stringf("Failed to load model from %s: %s",
                                    file.description().c_str(),
                                    importer->GetErrorString()));
        }

        scene = glData.scene = importer->GetScene();

        initBones();

        globalInverse = convertMatrix(scene->mRootNode->mTransformation).inverse();
        maxPoint      = Vec3f(1.0e-9f, 1.0e-9f, 1.0e-9f);
        minPoint      = Vec3f(1.0e9f,  1.0e9f,  1.0e9f);

        // Determine the total bounding box.
        for (duint i = 0; i < scene->mNumMeshes; ++i)
        {
            const aiMesh &mesh = *scene->mMeshes[i];
            for (duint i = 0; i < mesh.mNumVertices; ++i)
            {
                addToBounds(Vec3f(&mesh.mVertices[i].x));
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
            LOG_GL_VERBOSE("Animation #%i name:%s tps:%f")
                    << i << scene->mAnimations[i]->mName.C_Str()
                    << scene->mAnimations[i]->mTicksPerSecond;

            const String name = scene->mAnimations[i]->mName.C_Str();
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

    void buildNodeLookup(const aiNode &node)
    {
        const String name = node.mName.C_Str();
#ifdef DE_DEBUG
        debug("Node: %s", name.c_str());
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
        vertexBones.clear();
        boneNameToIndex.clear();
        nodeNameToPtr.clear();
        bones.clear();
        animNameToIndex.clear();
        meshIndexRanges.clear();
        importer.reset();
        scene = glData.scene = nullptr;
    }

    void glInit()
    {
        DE_ASSERT_IN_MAIN_THREAD();

        // Has a scene been imported successfully?
        if (!scene) return;

        if (modelAsset.isReady())
        {
            // Already good to go.
            return;
        }

        // Last minute notification in case some additional setup is needed.
        DE_NOTIFY_PUBLIC(AboutToGLInit, i)
        {
            i->modelAboutToGLInit(self());
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

    void addToBounds(const Vec3f &point)
    {
        minPoint = minPoint.min(point);
        maxPoint = maxPoint.max(point);
    }

    int findMaterial(const String &name) const
    {
        if (!scene) return -1;
        for (duint i = 0; i < scene->mNumMaterials; ++i)
        {
            const aiMaterial &material = *scene->mMaterials[i];
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
        return bones.sizei();
    }

    int addBone(const String &name)
    {
        int idx = boneCount();
        bones << BoneData();
        boneNameToIndex[name] = duint16(idx);
        return idx;
    }

    int findBone(const String &name) const
    {
        if (boneNameToIndex.contains(name))
        {
            return boneNameToIndex[name];
        }
        return -1;
    }

    int addOrFindBone(const String &name)
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
        DE_ASSERT_FAIL("Too many bone weights for a vertex");
    }

    /**
     * Initializes the per-vertex bone weight information, and indexes the bones
     * of the mesh in a sequential order.
     *
     * @param mesh        Source mesh.
     * @param vertexBase  Index of the first vertex of the mesh.
     */
    void initMeshBones(const aiMesh &mesh, duint vertexBase)
    {
        vertexBones.resize(vertexBase + mesh.mNumVertices);

        if (mesh.HasBones())
        {
            // Mark the per-vertex bone weights.
            for (duint i = 0; i < mesh.mNumBones; ++i)
            {
                const aiBone &bone = *mesh.mBones[i];

                const duint boneIndex = addOrFindBone(bone.mName.C_Str());
                bones[boneIndex].offset = convertMatrix(bone.mOffsetMatrix);

                for (duint w = 0; w < bone.mNumWeights; ++w)
                {
                    addVertexWeight(vertexBase + bone.mWeights[w].mVertexId,
                                    duint16(boneIndex),
                                    bone.mWeights[w].mWeight);
                }
            }
        }
        else
        {
            // No bones; make one dummy bone so we can render it the same way.
            const duint boneIndex = addOrFindBone(DUMMY_BONE_NAME);
            bones[boneIndex].offset = Mat4f();

            // All vertices fully affected by this bone.
            for (duint i = 0; i < mesh.mNumVertices; ++i)
            {
                addVertexWeight(vertexBase + i, duint16(boneIndex), 1.f);
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
            const aiMesh &mesh = *scene->mMeshes[i];

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
            const aiMesh &mesh = *scene->mMeshes[m];

            // Load vertices into the buffer.
            for (duint i = 0; i < mesh.mNumVertices; ++i)
            {
                const aiVector3D *pos      = &mesh.mVertices[i];
                const aiColor4D *color    = (mesh.HasVertexColors(0)? &mesh.mColors[0][i] : &white);
                const aiVector3D *normal   = (mesh.HasNormals()? &mesh.mNormals[i] : &zero);
                const aiVector3D *texCoord = (mesh.HasTextureCoords(0)? &mesh.mTextureCoords[0][i] : &zero);
                const aiVector3D *tangent  = (mesh.HasTangentsAndBitangents()? &mesh.mTangents[i] : &zero);
                const aiVector3D *bitang   = (mesh.HasTangentsAndBitangents()? &mesh.mBitangents[i] : &zero);

                VBuf::Type v;

                v.pos       = Vec3f(pos->x, pos->y, pos->z);
                v.color     = Vec4f(color->r, color->g, color->b, color->a);

                v.normal    = Vec3f(normal ->x, normal ->y, normal ->z);
                v.tangent   = Vec3f(tangent->x, tangent->y, tangent->z);
                v.bitangent = Vec3f(bitang ->x, bitang ->y, bitang ->z);

                v.texCoord     = Vec2f(texCoord->x, texCoord->y);
                v.texBounds[0] = Vec4f(0, 0, 1, 1);
                v.texBounds[1] = Vec4f(0, 0, 1, 1);
                v.texBounds[2] = Vec4f(0, 0, 1, 1);
                v.texBounds[3] = Vec4f(0, 0, 1, 1);

                const auto &meshTextures = material.meshTextures[m];

                for (int t = 0; t < MAX_TEXTURES; ++t)
                {
                    // Apply the specified order for the textures.
                    TextureMap map = glData.textureOrder[t];
                    if (map < 0 || map >= MAX_TEXTURES) continue;

                    if (meshTextures.texIds[map])
                    {
                        v.texBounds[t] = glData.textureBank.atlas(map)->imageRectf(meshTextures.texIds[map]).xywh();
                    }
                    else if (glData.defaultTexIds[map])
                    {
                        v.texBounds[t] = glData.textureBank.atlas(map)->imageRectf(glData.defaultTexIds[map]).xywh();
                    }
                    else
                    {
                        // Not included in material.
                        v.texBounds[t] = Vec4f();
                    }
                }

                for (int b = 0; b < MAX_BONES_PER_VERTEX; ++b)
                {
                    v.boneIds[b]     = vertexBones[base + i].ids[b];
                    v.boneWeights[b] = vertexBones[base + i].weights[b];
                }

                verts << v;
            }

            dsize firstFace = indx.size();

            // Get face indices.
            for (duint i = 0; i < mesh.mNumFaces; ++i)
            {
                const aiFace &face = mesh.mFaces[i];
                DE_ASSERT(face.mNumIndices == 3); // expecting triangles
                indx << VBuf::Index(face.mIndices[0] + base)
                     << VBuf::Index(face.mIndices[1] + base)
                     << VBuf::Index(face.mIndices[2] + base);
            }

            meshIndexRanges[m] = Rangez::fromSize(firstFace, mesh.mNumFaces * 3);

            base += mesh.mNumVertices;
        }

        std::unique_ptr<VBuf> buf(new VBuf);
        buf->setVertices(verts, gfx::Static);
        buf->setIndices(gfx::Triangles, indx, gfx::Static);
        material.buffer = std::move(buf);
    }

//- Animation ---------------------------------------------------------------------------

    struct AccumData
    {
        const Animator &   animator;
        ddouble            time = 0.0;
        const aiAnimation *anim = nullptr;
        List<Mat4f>        finalTransforms;

        AccumData(const Animator &animator, int boneCount)
            : animator(animator)
            , finalTransforms(boneCount)
        {}

        const aiNodeAnim *findNodeAnim(const aiNode &node) const
        {
            if (!anim) return nullptr;
            for (duint i = 0; i < anim->mNumChannels; ++i)
            {
                const aiNodeAnim *na = anim->mChannels[i];
                if (na->mNodeName == node.mName)
                {
                    return na;
                }
            }
            return nullptr;
        }
    };

    void accumulateAnimationTransforms(const Animator &animator,
                                       ddouble time,
                                       const aiAnimation *animSeq,
                                       const aiNode &rootNode) const
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

    void accumulateTransforms(const aiNode &node, AccumData &data,
                              const Mat4f &parentTransform = Mat4f()) const
    {
        Mat4f nodeTransform = convertMatrix(node.mTransformation);

        // Additional rotation?
        const Vec4f axisAngle = data.animator.extraRotationForNode(node.mName.C_Str());

        // Transform according to the animation sequence.
        if (const aiNodeAnim *anim = data.findNodeAnim(node))
        {
            // Interpolate for this point in time.
            const Mat4f translation = Mat4f::translate(interpolatePosition(data.time, *anim));
            const Mat4f scaling     = Mat4f::scale(interpolateScaling(data.time, *anim));
            Mat4f       rotation    = convertMatrix(aiMatrix4x4(interpolateRotation(data.time, *anim).GetMatrix()));

            if (!fequal(axisAngle.w, 0))
            {
                // Include the custom extra rotation.
                rotation = Mat4f::rotate(axisAngle.w, axisAngle) * rotation;
            }

            nodeTransform = translation * rotation * scaling;
        }
        else
        {
            // Model does not specify animation information for this node.
            // Only apply the possible additional rotation.
            if (!fequal(axisAngle.w, 0))
            {
                nodeTransform = Mat4f::rotate(axisAngle.w, axisAngle) * nodeTransform;
            }
        }

        Mat4f globalTransform = parentTransform * nodeTransform;

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
    static duint findAnimKey(ddouble time, const Type *keys, duint count)
    {
        DE_ASSERT(count > 0);
        for (duint i = 0; i < count - 1; ++i)
        {
            if (time < keys[i + 1].mTime)
            {
                return i;
            }
        }
        DE_ASSERT_FAIL("Failed to find animation key (invalid time?)");
        return 0;
    }

    static Vec3f interpolateVectorKey(ddouble time, const aiVectorKey *keys, duint at)
    {
        Vec3f const start(&keys[at]    .mValue.x);
        Vec3f const end  (&keys[at + 1].mValue.x);

        return start + (end - start) *
               float((time - keys[at].mTime) / (keys[at + 1].mTime - keys[at].mTime));
    }

    static aiQuaternion interpolateRotation(ddouble time, const aiNodeAnim &anim)
    {
        if (anim.mNumRotationKeys == 1)
        {
            return anim.mRotationKeys[0].mValue;
        }

        const aiQuatKey *key =
            anim.mRotationKeys + findAnimKey(time, anim.mRotationKeys, anim.mNumRotationKeys);

        aiQuaternion interp;
        aiQuaternion::Interpolate(interp,
                                  key[0].mValue,
                                  key[1].mValue,
                                  float((time - key[0].mTime) / (key[1].mTime - key[0].mTime)));
        interp.Normalize();
        return interp;
    }

    static Vec3f interpolateScaling(ddouble time, const aiNodeAnim &anim)
    {
        if (anim.mNumScalingKeys == 1)
        {
            return Vec3f(&anim.mScalingKeys[0].mValue.x);
        }
        return interpolateVectorKey(time, anim.mScalingKeys,
                                    findAnimKey(time, anim.mScalingKeys,
                                                anim.mNumScalingKeys));
    }

    static Vec3f interpolatePosition(ddouble time, const aiNodeAnim &anim)
    {
        if (anim.mNumPositionKeys == 1)
        {
            return Vec3f(&anim.mPositionKeys[0].mValue.x);
        }
        return interpolateVectorKey(time, anim.mPositionKeys,
                                    findAnimKey(time, anim.mPositionKeys,
                                                anim.mNumPositionKeys));
    }

    void updateMatricesFromAnimation(const Animator *animator) const
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
            const auto &animSeq = animator->at(i);

            // The animation has been validated earlier.
            DE_ASSERT(duint(animSeq.animId) < scene->mNumAnimations);
            DE_ASSERT(nodeNameToPtr.contains(animSeq.node));

            accumulateAnimationTransforms(*animator,
                                          animator->currentTime(i),
                                          scene->mAnimations[animSeq.animId],
                                          *nodeNameToPtr[animSeq.node]);
        }
    }

//- Drawing -----------------------------------------------------------------------------

    GLProgram *drawProgram = nullptr;
    const Pass *drawPass = nullptr;

    void preDraw(const Animator *animation)
    {
        if (glData.needMakeBuffer) makeBuffer();

        DE_ASSERT(drawProgram == nullptr);

        // Draw the meshes in this node.
        updateMatricesFromAnimation(animation);

        GLState::current().apply();
    }

    void setDrawProgram(GLProgram *prog, const Appearance *appearance = nullptr)
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

    void initRanges(GLBuffer::DrawRanges &ranges, const BitArray &meshes)
    {
        Rangez current;
        for (int i = 0; i < meshIndexRanges.sizei(); ++i)
        {
            if (!meshes.at(i)) continue;
            const auto &mesh = meshIndexRanges.at(i);
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

    void draw(const Appearance *appearance, const Animator *animation)
    {
        const Passes *passes =
            appearance && appearance->drawPasses ? appearance->drawPasses : &defaultPasses;
        preDraw(animation);

        try
        {
            GLBuffer::DrawRanges ranges;
            for (dsize i = 0; i < passes->size(); ++i)
            {
                const Pass &pass = passes->at(i);

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
                                       stringf("Rendering pass %d (\"%s\") has no shader program",
                                               i,
                                               pass.name.c_str()));
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
                        .setDepthTest (pass.depthFunc != gfx::Always)
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
        catch (const Error &er)
        {
            LOG_GL_ERROR("Failed to draw model \"%s\": %s")
                    << sourcePath
                    << er.asText();
        }

        postDraw();
    }

    void drawInstanced(const GLBuffer &attribs, const Animator *animation)
    {
        /// @todo Rendering passes for instanced drawing. -jk

        preDraw(animation);
        setDrawProgram(program);
        drawProgram->beginUse();
        glData.materials.at(0)->buffer->drawInstanced(attribs);
        drawProgram->endUse();
        postDraw();
    }

    void postDraw()
    {
        setDrawProgram(nullptr);
        drawPass = nullptr;
    }

    DE_PIMPL_AUDIENCE(AboutToGLInit)
};

DE_AUDIENCE_METHOD(ModelDrawable, AboutToGLInit)

namespace internal {

static struct {
    String const text;
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

ModelDrawable::TextureMap ModelDrawable::textToTextureMap(const String &text) // static
{
    for (const auto &mapping : internal::mappings)
    {
        if (!text.compareWithoutCase(mapping.text))
            return mapping.map;
    }
    return Unknown;
}

String ModelDrawable::textureMapToText(TextureMap map) // static
{
    for (const auto &mapping : internal::mappings)
    {
        if (mapping.map == map)
            return mapping.text;
    }
    return DE_STR("unknown");
}

ModelDrawable::ModelDrawable() : d(new Impl(this))
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

void ModelDrawable::load(const File &file)
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

int ModelDrawable::animationIdForName(const String &name) const
{
    auto found = d->animNameToIndex.find(name);
    if (found != d->animNameToIndex.end())
    {
        return found->second;
    }
    return -1;
}

String ModelDrawable::animationName(int id) const
{
    if (!d->scene || id < 0 || id >= int(d->scene->mNumAnimations))
    {
        return "";
    }
    const String name = d->scene->mAnimations[id]->mName.C_Str();
    if (name.isEmpty())
    {
        return Stringf("@%d", id);
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

int ModelDrawable::meshId(const String &name) const
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
    const String name = d->scene->mMeshes[id]->mName.C_Str();
    if (name.isEmpty())
    {
        return Stringf("@%d", id);
    }
    return name;
}

bool ModelDrawable::nodeExists(const String &name) const
{
    return d->nodeNameToPtr.contains(name);
}

void ModelDrawable::setAtlas(IAtlas &atlas)
{
    for (TextureMap tm : TEXTURE_MAP_TYPES)
    {
        setAtlas(tm, atlas); // same atlas for everything
    }
}

void ModelDrawable::setAtlas(TextureMap textureMap, IAtlas &atlas)
{
    d->glData.textureBank.setAtlas(int(textureMap), &atlas);
}

void ModelDrawable::unsetAtlas()
{
    d->glData.releaseTexturesFromAtlas();
    for (TextureMap tm : TEXTURE_MAP_TYPES)
    {
        d->glData.textureBank.setAtlas(int(tm), nullptr);
    }
}

IAtlas *ModelDrawable::atlas(TextureMap textureMap) const
{
    return d->glData.textureBank.atlas(textureMap);
}

ModelDrawable::Mapping ModelDrawable::diffuseNormalsSpecularEmission() // static
{
    return Mapping() << Diffuse << Normals << Specular << Emissive;
}

duint ModelDrawable::addMaterial()
{
    // This should only be done when the asset is not in use.
    DE_ASSERT(!d->modelAsset.isReady());

    return d->glData.addMaterial();
}

void ModelDrawable::resetMaterials()
{
    // This should only be done when the asset is not in use.
    DE_ASSERT(!d->modelAsset.isReady());

    d->glData.deinitMaterials();
    d->glData.initMaterials();
}

void ModelDrawable::setTextureMapping(const Mapping &mapsToUse)
{
    d->glData.setTextureMapping(mapsToUse);
}

void ModelDrawable::setDefaultTexture(TextureMap textureType, const Id &atlasId)
{
    DE_ASSERT(textureType >= 0 && textureType < MAX_TEXTURES);
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

int ModelDrawable::materialId(const String &name) const
{
    return d->findMaterial(name);
}

void ModelDrawable::setTexturePath(const MeshId &mesh, TextureMap textureMap, const String &path)
{
    if (d->glData.textureBank.atlas(textureMap))
    {
        // Load immediately.
        d->glData.setTexture(mesh, textureMap, path);
    }
    else
    {
        // This will override what the model specifies.
        d->glData.setCustomTexturePath(mesh, textureMap, path);
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

void ModelDrawable::draw(const Appearance *appearance,
                           const Animator *animation) const
{
    const_cast<ModelDrawable *>(this)->glInit();

    if (isReady() && d->glData.textureBank.atlas())
    {
        d->draw(appearance, animation);
    }
}

void ModelDrawable::drawInstanced(const GLBuffer &instanceAttribs,
                                  const Animator *animation) const
{
    const_cast<ModelDrawable *>(this)->glInit();

    if (isReady() && d->program && d->glData.textureBank.atlas())
    {
        d->drawInstanced(instanceAttribs, animation);
    }
#if defined (DE_DEBUG)
    else
    {
        debug("[ModelDrawable] drawInstanced isReady: %s program: %p atlas: %p",
              DE_BOOL_YESNO(isReady()),
              d->program,
              d->glData.textureBank.atlas());
    }
#endif
}

const ModelDrawable::Pass *ModelDrawable::currentPass() const
{
    return d->drawPass;
}

GLProgram *ModelDrawable::currentProgram() const
{
    return d->drawProgram;
}

Vec3f ModelDrawable::dimensions() const
{
    return d->maxPoint - d->minPoint;
}

Vec3f ModelDrawable::midPoint() const
{
    return (d->maxPoint + d->minPoint) / 2.f;
}

int ModelDrawable::Passes::findName(const String &name) const
{
    for (dsize i = 0; i < size(); ++i)
    {
        if (at(i).name == name) // case sensitive
            return int(i);
    }
    return -1;
}

} // namespace de

//---------------------------------------------------------------------------------------

namespace de {

DE_PIMPL_NOREF(ModelDrawable::Animator)
, DE_OBSERVES(Asset, Deletion)
{
    Constructor             constructor;
    const ModelDrawable *   model = nullptr;
    List<OngoingSequence *> anims;
    Flags                   flags = DefaultFlags;

    Impl(const Constructor &ctr, const ModelDrawable *mdl = nullptr)
        : constructor(ctr)
    {
        setModel(mdl);
    }

    ~Impl()
    {
        setModel(nullptr);
        deleteAll(anims);
    }

    void setModel(const ModelDrawable *mdl)
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
        DE_ASSERT(seq != nullptr);
        DE_ASSERT(model != nullptr);

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

    void stopByNode(const String &node)
    {
        for (auto i = anims.begin(); i != anims.end(); )
        {
            if ((*i)->node == node)
            {
                delete *i;
                i = anims.erase(i);
            }
            else
            {
                ++i;
            }
        }
    }

    const OngoingSequence *findAny(const String &rootNode) const
    {
        for (const OngoingSequence *anim : anims)
        {
            if (anim->node == rootNode)
                return anim;
        }
        return nullptr;
    }

    const OngoingSequence *find(int animId, const String &rootNode) const
    {
        for (const OngoingSequence *anim : anims)
        {
            if (anim->animId == animId && anim->node == rootNode)
                return anim;
        }
        return nullptr;
    }

    bool isRunning(int animId, const String &rootNode) const
    {
        return find(animId, rootNode) != nullptr;
    }
};

ModelDrawable::Animator::Animator(Constructor constructor)
    : d(new Impl(std::move(constructor)))
{}

ModelDrawable::Animator::Animator(const ModelDrawable &model, Constructor constructor)
    : d(new Impl(std::move(constructor), &model))
{}

void ModelDrawable::Animator::setModel(const ModelDrawable &model)
{
    d->setModel(&model);
}

void ModelDrawable::Animator::setFlags(const Flags &flags, FlagOp op)
{
    applyFlagOperation(d->flags, flags, op);
}

Flags ModelDrawable::Animator::flags() const
{
    return d->flags;
}

const ModelDrawable &ModelDrawable::Animator::model() const
{
    DE_ASSERT(d->model != nullptr);
    return *d->model;
}

int ModelDrawable::Animator::count() const
{
    return d->anims.sizei();
}

const ModelDrawable::Animator::OngoingSequence &
ModelDrawable::Animator::at(int index) const
{
    return *d->anims.at(index);
}

ModelDrawable::Animator::OngoingSequence &
ModelDrawable::Animator::at(int index)
{
    return *d->anims[index];
}

bool ModelDrawable::Animator::isRunning(const String &animName, const String &rootNode) const
{
    return d->isRunning(model().animationIdForName(animName), rootNode);
}

bool ModelDrawable::Animator::isRunning(int animId, const String &rootNode) const
{
    return d->isRunning(animId, rootNode);
}

ModelDrawable::Animator::OngoingSequence *ModelDrawable::Animator::find(const String &rootNode) const
{
    return const_cast<OngoingSequence *>(d->findAny(rootNode));
}

ModelDrawable::Animator::OngoingSequence *ModelDrawable::Animator::find(int animId, const String &rootNode) const
{
    return const_cast<OngoingSequence *>(d->find(animId, rootNode));
}

ModelDrawable::Animator::OngoingSequence &
ModelDrawable::Animator::start(const String &animName, const String &rootNode)
{
    return start(model().animationIdForName(animName), rootNode);
}

ModelDrawable::Animator::OngoingSequence &
ModelDrawable::Animator::start(int animId, const String &rootNode)
{
    d->stopByNode(rootNode);

    const aiScene &scene = *model().d->scene;

    if (animId < 0 || animId >= int(scene.mNumAnimations))
    {
        throw InvalidError("ModelDrawable::Animator::start",
                           stringf("Invalid animation ID %d", animId));
    }

    const auto &animData = *scene.mAnimations[animId];

    OngoingSequence *anim = d->constructor();
    anim->animId          = animId;
    anim->node            = rootNode;
    anim->time            = 0.0;
    anim->duration        = ticksToSeconds(animData.mDuration, animData);
    anim->initialize();
    return d->add(anim);
}

void ModelDrawable::Animator::stop(int index)
{
    d->anims.removeAt(index);
}

void ModelDrawable::Animator::clear()
{
    deleteAll(d->anims);
    d->anims.clear();
}

void ModelDrawable::Animator::advanceTime(TimeSpan )
{
    // overridden
}

ddouble ModelDrawable::Animator::currentTime(int index) const
{
    const auto &anim = at(index);
    ddouble t = anim.time;
    if (anim.flags.testFlag(OngoingSequence::ClampToDuration))
    {
        t = min(t, anim.duration - de::FLOAT_EPSILON);
    }
    return t;
}

Vec4f ModelDrawable::Animator::extraRotationForNode(const String &) const
{
    return Vec4f();
}

void ModelDrawable::Animator::operator >> (Writer &to) const
{
    to.writeObjects(d->anims);
}

void ModelDrawable::Animator::operator << (Reader &from)
{
    clear();
    from.readObjects<OngoingSequence>(d->anims, [this] () { return d->constructor(); });
}

void ModelDrawable::Animator::OngoingSequence::initialize()
{}

bool ModelDrawable::Animator::OngoingSequence::atEnd() const
{
    return time >= duration;
}

void ModelDrawable::Animator::OngoingSequence::operator >> (Writer &to) const
{
    to << animId
       << time
       << duration
       << node
       << duint32(flags);
}

void ModelDrawable::Animator::OngoingSequence::operator << (Reader &from)
{
    from >> animId
         >> time
         >> duration
         >> node;
    from.readAs<duint32>(flags);
}

ModelDrawable::Animator::OngoingSequence *
ModelDrawable::Animator::OngoingSequence::make() // static
{
    return new OngoingSequence;
}

} // namespace de

//uint qHash(const de::ModelDrawable::Pass &pass)
//{
//    return qHash(pass.name);
//}
