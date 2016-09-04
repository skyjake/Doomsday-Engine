/** @file glshaderbank.cpp  Bank containing GL shaders.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/GLShaderBank"
#include "de/GLProgram"
#include "de/GLShader"
#include "de/GLUniform"

#include <de/App>
#include <de/ArrayValue>
#include <de/ByteArrayFile>
#include <de/DictionaryValue>
#include <de/Folder>
#include <de/ScriptedInfo>
#include <de/math.h>

#include <QMap>

namespace de {

DENG2_PIMPL(GLShaderBank)
{
    struct Source : public ISource
    {
        /// Information about a shader source.
        struct ShaderSource
        {
            String source;
            enum Type
            {
                FilePath,
                ShaderSourceText
            };
            Type type;

            ShaderSource(String const &str = "", Type t = ShaderSourceText)
                : source(str), type(t) {}

            void convertToSourceText()
            {
                if (type == FilePath)
                {
                    source = String::fromLatin1(Block(App::rootFolder().locate<File const>(source)));
                    type = ShaderSourceText;
                }
            }

            void insertFromFile(String const &path)
            {
                convertToSourceText();
                source += "\n";
                Block combo = GLShader::prefixToSource(source.toLatin1(),
                        Block(App::rootFolder().locate<File const>(path)));
                source = String::fromLatin1(combo);
            }

            void insertDefinition(String const &macroName, String const &content)
            {
                convertToSourceText();
                Block combo = GLShader::prefixToSource(source.toLatin1(),
                        Block(String("#define %1 %2\n").arg(macroName).arg(content).toLatin1()));
                source = String::fromLatin1(combo);
            }
        };

        GLShaderBank &bank;
        String id;
        ShaderSource vertex;
        ShaderSource fragment;

        Source(GLShaderBank &b, String const &id, ShaderSource const &vtx, ShaderSource const &frag)
            : bank(b), id(id), vertex(vtx), fragment(frag)
        {}

        Time sourceModifiedAt(ShaderSource const &src) const
        {
            if (src.type == ShaderSource::FilePath && !src.source.isEmpty())
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

            if (src.type == ShaderSource::FilePath)
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
        QSet<GLUniform *> defaultUniforms;

        Data(GLShader *v, GLShader *f)
            : vertex(holdRef(v))
            , fragment(holdRef(f))
        {}

        ~Data()
        {
            qDeleteAll(defaultUniforms);
            releaseRef(vertex);
            releaseRef(fragment);
        }
    };

    typedef QMap<String, GLShader *> Shaders; // path -> shader
    Shaders shaders;

    Impl(Public *i) : Base(i)
    {}

    ~Impl()
    {
        clearShaders();
    }

    void clearShaders()
    {
        // Release all of our references to the shaders.
        foreach (GLShader *shader, shaders.values())
        {
            shader->release();
        }
        shaders.clear();
    }

    GLShader *findShader(String const &path, GLShader::Type type)
    {
        /// @todo Should check the modification time of the file to determine
        /// if recompiling the shader is appropriate.

        if (shaders.contains(path))
        {
            return shaders[path];
        }

        // We don't have this one yet, load and compile it now.
        GLShader *shader = new GLShader(type, App::rootFolder().locate<ByteArrayFile const>(path));
        shaders.insert(path, shader);
        return shader;
    }
};

GLShaderBank::GLShaderBank() : InfoBank("GLShaderBank"), d(new Impl(this))
{}

void GLShaderBank::clear()
{
    d->clearShaders();
}

void GLShaderBank::addFromInfo(File const &file)
{
    LOG_AS("GLShaderBank");
    parse(file);
    addFromInfoBlocks("shader");
}

GLShader &GLShaderBank::shader(DotPath const &path, GLShader::Type type) const
{
    Impl::Data &i = data(path).as<Impl::Data>();

    if (type == GLShader::Vertex)
    {
        return *i.vertex;
    }
    else
    {
        return *i.fragment;
    }
}

GLProgram &GLShaderBank::build(GLProgram &program, DotPath const &path) const
{
    Impl::Data &i = data(path).as<Impl::Data>();
    program.build(i.vertex, i.fragment);

    // Bind the default uniforms. These will be used if no overriding
    // uniforms are bound.
    for (GLUniform *uniform : i.defaultUniforms)
    {
        program << *uniform;
    }

    return program;
}

Bank::ISource *GLShaderBank::newSourceFromInfo(String const &id)
{
    typedef Impl::Source Source;
    typedef Impl::Source::ShaderSource ShaderSource;

    Record const &def = info()[id];

    ShaderSource vtx;
    ShaderSource frag;

    // Vertex shader definition.
    if (def.has("vertex"))
    {
        vtx = ShaderSource(def["vertex"], ShaderSource::ShaderSourceText);
    }
    else if (def.has("path.vertex"))
    {
        vtx = ShaderSource(absolutePathInContext(def, def["path.vertex"]), ShaderSource::FilePath);
    }
    else if (def.has("path"))
    {
        vtx = ShaderSource(absolutePathInContext(def, def.gets("path") + ".vsh"), ShaderSource::FilePath);
    }

    // Fragment shader definition.
    if (def.has("fragment"))
    {
        frag = ShaderSource(def["fragment"], ShaderSource::ShaderSourceText);
    }
    else if (def.has("path.fragment"))
    {
        frag = ShaderSource(absolutePathInContext(def, def["path.fragment"]), ShaderSource::FilePath);
    }
    else if (def.has("path"))
    {
        frag = ShaderSource(absolutePathInContext(def, def.gets("path") + ".fsh"), ShaderSource::FilePath);
    }

    // Additional shaders to append to the main source.
    if (def.has("include.vertex"))
    {
        // Including in reverse to retain order -- each one is prepended.
        auto const &incs = def["include.vertex"].value().as<ArrayValue>().elements();
        for (int i = incs.size() - 1; i >= 0; --i)
        {
            vtx.insertFromFile(absolutePathInContext(def, incs.at(i)->asText()));
        }
    }
    if (def.has("include.fragment"))
    {
        // Including in reverse to retain order -- each one is prepended.
        auto const &incs = def["include.fragment"].value().as<ArrayValue>().elements();
        for (int i = incs.size() - 1; i >= 0; --i)
        {
            frag.insertFromFile(absolutePathInContext(def, incs.at(i)->asText()));
        }
    }

    if (def.has("defines"))
    {
        DictionaryValue const &dict = def.getdt("defines");
        for (auto i : dict.elements())
        {
            String const macroName = i.first.value->asText();
            String const content   = i.second->asText();
            vtx .insertDefinition(macroName, content);
            frag.insertDefinition(macroName, content);
        }
    }

    return new Source(*this, id, vtx, frag);
}

Bank::IData *GLShaderBank::loadFromSource(ISource &source)
{
    Impl::Source &src = source.as<Impl::Source>();
    std::unique_ptr<Impl::Data> data(new Impl::Data(src.load(GLShader::Vertex),
                                                            src.load(GLShader::Fragment)));
    // Create default uniforms.
    Record const &def = info()[src.id];
    auto const vars = ScriptedInfo::subrecordsOfType(QStringLiteral("variable"), def);
    for (auto i = vars.begin(); i != vars.end(); ++i)
    {
        std::unique_ptr<GLUniform> uniform;
        Block const uName = i.key().toLatin1();

        if (!i.value()->has(QStringLiteral("value"))) continue;

        // Initialize the appropriate type of value animation and uniform,
        // depending on the "value" key in the definition.
        Value const &valueDef = i.value()->get(QStringLiteral("value"));
        if (auto const *array = valueDef.maybeAs<ArrayValue>())
        {
            switch (array->size())
            {
            default:
                throw DefinitionError("GLShaderBank::loadFromSource",
                                      QString("%1: Invalid initial value size (%2) for shader variable")
                                      .arg(ScriptedInfo::sourceLocation(*i.value()))
                                      .arg(array->size()));

            case 1:
                uniform.reset(new GLUniform(uName, GLUniform::Float));
                *uniform = array->element(0).asNumber();
                break;

            case 2:
                uniform.reset(new GLUniform(uName, GLUniform::Vec2));
                *uniform = vectorFromValue<Vector2f>(*array);
                break;

            case 3:
                uniform.reset(new GLUniform(uName, GLUniform::Vec3));
                *uniform = vectorFromValue<Vector3f>(*array);
                break;

            case 4:
                uniform.reset(new GLUniform(uName, GLUniform::Vec4));
                *uniform = vectorFromValue<Vector4f>(*array);
                break;
            }
        }
        else
        {
            uniform.reset(new GLUniform(uName, GLUniform::Float));
            *uniform = valueDef.asNumber();
        }

        data->defaultUniforms.insert(uniform.release());
    }

    return data.release();
}

} // namespace de
