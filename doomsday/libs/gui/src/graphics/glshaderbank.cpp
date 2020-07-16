/** @file glshaderbank.cpp  Bank containing GL shaders.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/glshaderbank.h"
#include "de/glprogram.h"
#include "de/glshader.h"
#include "de/gluniform.h"

#include <de/arrayvalue.h>
#include <de/bytearrayfile.h>
#include <de/dictionaryvalue.h>
#include <de/filesystem.h>
#include <de/keymap.h>
#include <de/regexp.h>
#include <de/scripting/scriptedinfo.h>
#include <de/math.h>

namespace de {

static String processIncludes(String source, const String &sourceFolderPath)
{
    static const RegExp includeRegex(R"(#include\s+['"]([^"']+)['"])");

    RegExpMatch found;
    while (includeRegex.match(source, found))
    {
        const auto capStr = found.capturedCStr(1);

        String incFilePath = sourceFolderPath / capStr;
        String incSource   = String::fromUtf8(FS::locate<File const>(incFilePath));
        incSource          = processIncludes(incSource, incFilePath.fileNamePath());

        source = CString{source.begin(), found.begin()}
               + incSource
               + CString{found.end(), source.end()};
    }
    return source;
}

DE_PIMPL(GLShaderBank)
{
    struct Source : public ISource
    {
        /// Information about a shader source.
        struct ShaderSource
        {
            enum Type { None, FilePath, ShaderSourceText };

            Type   type;
            String source;

            ShaderSource(const String &str = "", Type t = None)
                : type(t), source(str) {}

            void convertToSourceText()
            {
                if (type == FilePath)
                {
                    source = String::fromLatin1(Block(FS::locate<File const>(source)));
                    type = ShaderSourceText;
                }
            }

            void insertFromFile(const String &path)
            {
                if (type == None) return;
                convertToSourceText();
                source += "\n";
                Block combo = GLShader::prefixToSource(source.toLatin1(),
                        Block(FS::locate<File const>(path)));
                source = String::fromLatin1(combo);
            }

            void insertDefinition(const String &macroName, const String &content)
            {
                if (type == None) return;
                convertToSourceText();
                Block combo = GLShader::prefixToSource(
                    source, Stringf("#define %s %s\n", macroName.c_str(), content.c_str()));
                source = combo;
            }

            void insertIncludes(const GLShaderBank &bank, const Record &def)
            {
                if (type == None) return;
                convertToSourceText();
                source = processIncludes(source, bank.absolutePathInContext(def, ".").fileNamePath());
            }
        };

        GLShaderBank &bank;
        String        id;
        ShaderSource  sources[3]; // GLShader::Type as index

        Source(GLShaderBank &      b,
               const String &      id,
               const ShaderSource &vtx,
               const ShaderSource &geo,
               const ShaderSource &frag)
            : bank(b)
            , id(id)
            , sources{vtx, geo, frag}
        {}

        Time sourceModifiedAt(const ShaderSource &src) const
        {
            if (src.type == ShaderSource::FilePath && src.source)
            {
                return FS::locate<File const>(src.source).status().modifiedAt;
            }
            return bank.sourceModifiedAt();
        }

        Time modifiedAt() const
        {
            return de::max(sourceModifiedAt(sources[GLShader::Vertex]),
                           sourceModifiedAt(sources[GLShader::Geometry]),
                           sourceModifiedAt(sources[GLShader::Fragment]));
        }

        GLShader *load(GLShader::Type type) const
        {
            const ShaderSource &src = sources[type];
            if (src.type == ShaderSource::None)
        {
                return nullptr;
            }
            if (src.type == ShaderSource::FilePath)
            {
                return bank.d->findShader(src.source, type);
            }
            // The program will hold the only ref to this shader.
            return refless(
                new GLShader(type, bank.d->prependPredefines(Block(src.source.toLatin1()))));
        }
    };

    struct Data : public IData
    {
        GLShader *shaders[3]; // GLShader::Type as index
        Set<GLUniform *> defaultUniforms;

        Data(GLShader *v, GLShader *g, GLShader *f)
            : shaders{holdRef(v), g? holdRef(g) : nullptr, holdRef(f)}
        {}

        ~Data()
        {
            deleteAll(defaultUniforms);
            for (auto &shd : shaders) releaseRef(shd);
        }
    };

    typedef KeyMap<String, GLShader *> Shaders; // path -> shader
    Shaders shaders;
    std::unique_ptr<DictionaryValue> preDefines;

    Impl(Public *i) : Base(i)
    {}

    ~Impl()
    {
        clearShaders();
    }

    void clearShaders()
    {
        // Release all of our references to the shaders.
        for (const auto &i : shaders)
        {
            i.second->release();
        }
        shaders.clear();
    }

    Block prependPredefines(const IByteArray &source) const
    {
        Block sourceText = source;
        if (preDefines)
        {
            Block predefs;
            for (auto i : preDefines->elements())
            {
                predefs += String::format("#define %s %s\n",
                                          i.first.value->asText().toLatin1().constData(),
                                          i.second->asText().toLatin1().constData())
                               .toLatin1();
            }
            predefs += "#line 1\n";
            sourceText = GLShader::prefixToSource(sourceText, predefs);
        }
        return sourceText;
    }

    GLShader *findShader(const String &path, GLShader::Type type)
    {
        /// @todo Should check the modification time of the file to determine
        /// if recompiling the shader is appropriate.

        if (shaders.contains(path))
        {
            return shaders[path];
        }

        // We don't have this one yet, load and compile it now.
        GLShader *shader =
            new GLShader(type, prependPredefines(FS::locate<const ByteArrayFile>(path)));
        shaders.insert(path, shader);
        return shader;
    }
};

GLShaderBank::GLShaderBank() : InfoBank("GLShaderBank"), d(new Impl(this))
{}

void GLShaderBank::clear()
{
    d->clearShaders();
    InfoBank::clear();
}

void GLShaderBank::addFromInfo(const File &file)
{
    LOG_AS("GLShaderBank");
    parse(file);
    addFromInfoBlocks("shader");
}

GLShader &GLShaderBank::shader(const DotPath &path, GLShader::Type type) const
{
    Impl::Data &i = data(path).as<Impl::Data>();
    return *i.shaders[type];
    }

GLProgram &GLShaderBank::build(GLProgram &program, const DotPath &path) const
{
    Impl::Data &i = data(path).as<Impl::Data>();

    List<const GLShader *> shaders;
    for (auto *s : i.shaders)
    {
        if (s) shaders << s;
    }
    program.build(shaders);

    // Bind the default uniforms. These will be used if no overriding
    // uniforms are bound.
    for (GLUniform *uniform : i.defaultUniforms)
    {
        program << *uniform;
    }

    return program;
}

void GLShaderBank::setPreprocessorDefines(const DictionaryValue &preDefines)
{
    d->preDefines.reset(static_cast<DictionaryValue *>(preDefines.duplicate()));
}

Bank::ISource *GLShaderBank::newSourceFromInfo(const String &id)
{
    using Source       = Impl::Source;
    using ShaderSource = Impl::Source::ShaderSource;

    const Record &def = info()[id];

    ShaderSource sources[3];

    static String const typeTokens[3]    = {"vertex", "geometry", "fragment"};
    static String const pathTokens[3]    = {"path.vertex", "path.geometry", "path.fragment"};
    static String const includeTokens[3] = {"include.vertex", "include.geometry", "include.fragment"};
    static String const fileExt[3]       = {".vsh", ".gsh", ".fsh"};

    // Shader definition.
    for (int i = 0; i < 3; ++i)
    {
        if (def.has(typeTokens[i]))
        {
            sources[i] = ShaderSource(def[typeTokens[i]], ShaderSource::ShaderSourceText);
        }
        else if (def.has(pathTokens[i]))
        {
            sources[i] = ShaderSource(absolutePathInContext(def, def[pathTokens[i]]),
                                      ShaderSource::FilePath);
        }
        else if (def.has("path"))
        {
            String spath = absolutePathInContext(def, def.gets("path") + fileExt[i]);
            if (i == GLShader::Geometry && !FS::tryLocate<File const>(spath))
            {
                continue; // Geometry shader not provided.
            }
            sources[i] = ShaderSource(spath, ShaderSource::FilePath);
        }

        // Additional shaders to append to the main source.
        if (def.has(includeTokens[i]))
        {
            // Including in reverse to retain order -- each one is prepended.
            const auto &incs = def[includeTokens[i]].value<ArrayValue>().elements();
            for (int j = incs.sizei() - 1; j >= 0; --j)
            {
                sources[i].insertFromFile(absolutePathInContext(def, incs.at(j)->asText()));
            }
        }

        // Handle #include directives in the source.
        sources[i].insertIncludes(*this, def);
    }

    // Preprocessor defines (built-in and from the Info).
    if (def.has("defines"))
    {
        const DictionaryValue &dict = def.getdt("defines");
        for (const auto &i : dict.elements())
        {
            const String macroName = i.first.value->asText();
            const String content   = i.second->asText();
            for (auto &ss : sources)
            {
                ss.insertDefinition(macroName, content);
            }
        }
    }

    return new Source(*this, id, sources[0], sources[1], sources[2]);
}

Bank::IData *GLShaderBank::loadFromSource(ISource &source)
{
    Impl::Source &src = source.as<Impl::Source>();

    std::unique_ptr<Impl::Data> data(new Impl::Data(
        src.load(GLShader::Vertex), src.load(GLShader::Geometry), src.load(GLShader::Fragment)));

    // Create default uniforms.
    const Record &def  = info()[src.id];
    const auto    vars = ScriptedInfo::subrecordsOfType(DE_STR("variable"), def);

    for (auto i = vars.begin(); i != vars.end(); ++i)
    {
        std::unique_ptr<GLUniform> uniform;
        const String &uName = i->first;

        if (!i->second->has(DE_STR("value"))) continue;

        // Initialize the appropriate type of value animation and uniform,
        // depending on the "value" key in the definition.
        const Value &valueDef = i->second->get(DE_STR("value"));
        if (const auto *array = maybeAs<ArrayValue>(valueDef))
        {
            switch (array->size())
            {
            default:
                throw DefinitionError(
                    "GLShaderBank::loadFromSource",
                    stringf("%s: Invalid initial value size (%zu) for shader variable",
                            ScriptedInfo::sourceLocation(*i->second).c_str(),
                            array->size()));

            case 1:
                uniform.reset(new GLUniform(uName, GLUniform::Float));
                *uniform = array->element(0).asNumber();
                break;

            case 2:
                uniform.reset(new GLUniform(uName, GLUniform::Vec2));
                *uniform = vectorFromValue<Vec2f>(*array);
                break;

            case 3:
                uniform.reset(new GLUniform(uName, GLUniform::Vec3));
                *uniform = vectorFromValue<Vec3f>(*array);
                break;

            case 4:
                uniform.reset(new GLUniform(uName, GLUniform::Vec4));
                *uniform = vectorFromValue<Vec4f>(*array);
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
