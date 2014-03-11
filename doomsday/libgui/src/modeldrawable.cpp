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

#include <de/App>
#include <de/ByteArrayFile>
#include <de/Matrix>
#include <de/GLBuffer>

#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#include <assimp/Importer.hpp>
#include <assimp/LogStream.hpp>
#include <assimp/DefaultLogger.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

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

/// Adapter between libdeng2 FS and Assimp.
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

} // namespace internal
using namespace internal;

DENG2_PIMPL(ModelDrawable)
{
    typedef GLBufferT<Vertex3NormalTangentTex> VBuf;

    Asset modelAsset;
    Assimp::Importer importer;
    Path sourcePath;
    QVector<VBuf *> meshBuffers;

    Instance(Public *i) : Base(i)
    {
        // Use libdeng2 for file access.
        importer.SetIOHandler(new ImpIOSystem);

        // Get most kinds of log output.
        ImpLogger::registerLogger();
    }

    /// Release all loaded model data.
    void clear()
    {
        modelAsset.setState(NotReady);
        meshBuffers.clear();
        importer.FreeScene();
    }

    void import(File const &file)
    {
        LOG_RES_MSG("Loading model from %s") << file.description();

        sourcePath = file.path();
        if(!importer.ReadFile(sourcePath.toString().toLatin1(), aiProcessPreset_TargetRealtime_Fast))
        {
            throw LoadError("ModelDrawable::import", String("Failed to load model from %s: %s")
                            .arg(file.description()).arg(importer.GetErrorString()));
        }
    }

    void glInit()
    {
        DENG2_ASSERT_IN_MAIN_THREAD();

        if(modelAsset.isReady())
        {
            // Already good to go.
            return;
        }

        // Has a scene been imported successfully?
        if(!importer.GetScene()) return;

        initFromScene(*importer.GetScene());

        // Ready to go!
        modelAsset.setState(Ready);
    }

    void glDeinit()
    {
        /// @todo Free all GL resources.

        modelAsset.setState(NotReady);
    }

    void initFromScene(aiScene const &scene)
    {
        meshBuffers.resize(scene.mNumMeshes);

        // Initialize all meshes in the scene.
        for(duint i = 0; i < scene.mNumMeshes; ++i)
        {
            qDebug() << "initing mesh" << i << "out of" << scene.mNumMeshes;
            meshBuffers[i] = makeBufferFromMesh(*scene.mMeshes[i]);
        }
    }

    VBuf *makeBufferFromMesh(aiMesh const &mesh)
    {
        VBuf::Vertices verts;
        VBuf::Indices indx;

        aiVector3D const zero(0, 0, 0);

        // Load vertices into the buffer.
        for(duint i = 0; i < mesh.mNumVertices; ++i)
        {
            aiVector3D const *pos      = &mesh.mVertices[i];
            aiVector3D const *normal   = (mesh.HasNormals()? &mesh.mNormals[i] : &zero);
            aiVector3D const *texCoord = (mesh.HasTextureCoords(0)? &mesh.mTextureCoords[0][i] : &zero);
            aiVector3D const *tangent  = (mesh.HasTangentsAndBitangents()? &mesh.mTangents[i] : &zero);
            aiVector3D const *bitang   = (mesh.HasTangentsAndBitangents()? &mesh.mBitangents[i] : &zero);

            VBuf::Type v;
            v.pos       = Vector3f(pos->x, pos->y, pos->z);
            v.normal    = Vector3f(normal->x, normal->y, normal->z);
            v.tangent   = Vector3f(tangent->x, tangent->y, tangent->z);
            v.bitangent = Vector3f(bitang->x, bitang->y, bitang->z);
            v.texCoord  = Vector2f(texCoord->x, texCoord->y);
            verts << v;
        }

        // Get face indices.
        for(duint i = 0; i < mesh.mNumFaces; ++i)
        {
            aiFace const &face = mesh.mFaces[i];
            DENG2_ASSERT(face.mNumIndices == 3); // expecting triangles
            indx << face.mIndices[0] << face.mIndices[1] << face.mIndices[2];
        }

        VBuf *buf = new VBuf;
        buf->setVertices(verts, gl::Static);
        buf->setIndices(gl::Triangles, indx, gl::Static);

        qDebug() << "new GLbuf" << buf << "name:" << mesh.mName.C_Str();

        //self.addBuffer(buf);
        return buf;
    }

    struct DrawState {
        Matrix4f transform;
    };

    /// Traverses the scene node tree and draws meshes in their current animated state.
    void draw()
    {
        qDebug() << "drawing scene";

        DrawState state;
        drawNode(*importer.GetScene()->mRootNode, state);
    }

    void drawNode(aiNode const &node, DrawState const &state)
    {
        Matrix4f const xf(&node.mTransformation.a1);

        qDebug() << "node:" << node.mName.C_Str() << "meshes:" << node.mNumMeshes;
        qDebug() << "transform:" << xf.asText();

        DrawState local = state;
        local.transform = state.transform * xf;
    }
};

ModelDrawable::ModelDrawable() : d(new Instance(this))
{
    *this += d->modelAsset;
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

void ModelDrawable::glInit()
{
    d->glInit();
}

void ModelDrawable::glDeinit()
{
    d->glDeinit();
}

void ModelDrawable::draw()
{
    glInit();
    if(isReady())
    {
        d->draw();
    }
}

} // namespace de
