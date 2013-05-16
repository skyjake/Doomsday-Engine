/** @file glshaderbank.cpp  Bank containing GL shaders.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "de/GLShaderBank"
#include "de/GLProgram"
#include "de/GLShader"

#include <de/App>
#include <de/ScriptedInfo>
#include <de/ByteArrayFile>
#include <de/math.h>

#include <QMap>

namespace de {

DENG2_PIMPL(GLShaderBank)
{
    struct Source : public ISource
    {
        /// Information about a shader source.
        struct ShaderSource {
            String source;
            enum Type {
                FilePath,
                ShaderSourceText
            };
            Type type;

            ShaderSource(String const &str = "", Type t = ShaderSourceText)
                : source(str), type(t) {}
        };

        GLShaderBank &bank;
        ShaderSource vertex;
        ShaderSource fragment;

        Source(GLShaderBank &b, ShaderSource const &vtx, ShaderSource const &frag)
            : bank(b), vertex(vtx), fragment(frag)
        {}

        Time sourceModifiedAt(ShaderSource const &src) const
        {
            if(src.type == ShaderSource::FilePath)
            {
                return App::rootFolder().locate<File const>(src.source).status().modifiedAt;
            }
            return bank.sourceModifiedAt();
        }

        Time modifiedAt() const
        {
            Time vtxTime  = sourceModifiedAt(vertex);
            Time fragTime = sourceModifiedAt(fragment);
            return de::max(vtxTime, fragTime);
        }

        GLShader *load(GLShader::Type type) const
        {
            ShaderSource const &src = (type == GLShader::Vertex? vertex : fragment);

            if(src.type == ShaderSource::FilePath)
            {
                return bank.d->findShader(src.source, type);
            }
            // The program will hold the only ref to this shader.
            return refless(new GLShader(type, src.source.toLatin1()));
        }
    };

    struct Data : public IData
    {
        GLShader *vertex;
        GLShader *fragment;

        Data(GLShader *v, GLShader *f)
            : vertex(holdRef(v)), fragment(holdRef(f)) {}
        ~Data() {
            releaseRef(vertex);
            releaseRef(fragment);
        }
    };

    typedef QMap<String, GLShader *> Shaders; // path -> shader
    Shaders shaders;

    String relativeToPath;

    Instance(Public *i) : Base(i)
    {}

    ~Instance()
    {
        clearShaders();
    }

    void clearShaders()
    {
        // Release all of our references to the shaders.
        foreach(GLShader *shader, shaders.values())
        {
            shader->release();
        }
        shaders.clear();
    }

    GLShader *findShader(String const &path, GLShader::Type type)
    {
        /// @todo Should check the modification time of the file to determine
        /// if recompiling the shader is appropriate.

        if(shaders.contains(path))
        {
            return shaders[path];
        }

        // We don't have this one yet, load and compile it now.
        GLShader *shader = new GLShader(type, App::rootFolder().locate<ByteArrayFile const>(path));
        shaders.insert(path, shader);
        return shader;
    }
};

GLShaderBank::GLShaderBank() : d(new Instance(this))
{}

void GLShaderBank::addFromInfo(File const &file)
{
    LOG_AS("GLShaderBank");
    d->relativeToPath = file.path().fileNamePath();
    parse(file);
    addFromInfoBlocks("shader");
}

GLShader &GLShaderBank::shader(Path const &path, GLShader::Type type) const
{
    Instance::Data &i = static_cast<Instance::Data &>(data(path));

    if(type == GLShader::Vertex)
    {
        return *i.vertex;
    }
    else
    {
        return *i.fragment;
    }
}

GLProgram &GLShaderBank::build(GLProgram &program, Path const &path) const
{
    Instance::Data &i = static_cast<Instance::Data &>(data(path));
    program.build(i.vertex, i.fragment);
    return program;
}

Bank::ISource *GLShaderBank::newSourceFromInfo(String const &id)
{
    typedef Instance::Source Source;
    typedef Instance::Source::ShaderSource ShaderSource;

    Record const &def = info()[id];

    ShaderSource vtx;
    ShaderSource frag;

    // Vertex shader definition.
    if(def.has("path.vertex"))
    {
        vtx = ShaderSource(d->relativeToPath / def["path.vertex"], ShaderSource::FilePath);
    }
    else if(def.has("path"))
    {
        vtx = ShaderSource(d->relativeToPath / def["path"] + ".vsh", ShaderSource::FilePath);
    }
    else if(def.has("vertex"))
    {
        vtx = ShaderSource(def["vertex"], ShaderSource::ShaderSourceText);
    }

    // Fragment shader definition.
    if(def.has("path.fragment"))
    {
        frag = ShaderSource(d->relativeToPath / def["path.fragment"], ShaderSource::FilePath);
    }
    else if(def.has("path"))
    {
        frag = ShaderSource(d->relativeToPath / def["path"] + ".fsh", ShaderSource::FilePath);
    }
    else if(def.has("fragment"))
    {
        frag = ShaderSource(def["fragment"], ShaderSource::ShaderSourceText);
    }

    return new Source(*this, vtx, frag);
}

Bank::IData *GLShaderBank::loadFromSource(ISource &source)
{
    Instance::Source &src = static_cast<Instance::Source &>(source);

    return new Instance::Data(src.load(GLShader::Vertex),
                              src.load(GLShader::Fragment));
}

} // namespace de
