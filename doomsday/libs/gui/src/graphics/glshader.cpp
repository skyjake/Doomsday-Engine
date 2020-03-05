/** @file glshader.cpp  GL shader.
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

#include "de/GLShader"
#include "de/GLInfo"
#include "de/GuiApp"
#include "de/graphics/opengl.h"
#include <de/Block>
#include <de/String>

namespace de {

DE_PIMPL(GLShader)
{
    GLuint name = 0;
    Type   type = Vertex;

    Impl(Public *i) : Base(i)
    {}

    ~Impl()
    {
        release();
    }

    void alloc()
    {
        LIBGUI_ASSERT_GL_CONTEXT_ACTIVE();
        if (!name)
        {
#if defined (DE_OPENGL)
            name = glCreateShader(type == Vertex   ? GL_VERTEX_SHADER
                                : type == Geometry ? GL_GEOMETRY_SHADER
                                                   : GL_FRAGMENT_SHADER);
#elif defined (DE_OPENGL_ES)
            DE_ASSERT(type != Geometry);
            name = glCreateShader(type == Vertex ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
#endif
            LIBGUI_ASSERT_GL_OK();
            if (!name)
            {
                throw AllocError("GLShader::alloc", "Failed to create shader");
            }
        }
    }

    void release()
    {
        if (name)
        {
            glDeleteShader(name);
            name = 0;
        }
        self().setState(Asset::NotReady);
    }
};

GLShader::GLShader() : d(new Impl(this))
{}

GLShader::GLShader(Type shaderType, const IByteArray &source) : d(new Impl(this))
{
    try
    {
        compile(shaderType, source);
    }
    catch (...)
    {
        // Construction was aborted.
        addRef(-1);
        throw;
    }
}

GLShader::Type GLShader::type() const
{
    return d->type;
}

GLuint GLShader::glName() const
{
    return d->name;
}

void GLShader::clear()
{
    d->release();
}

String GLShader::prefixToSource(const String &source, const String &prefix)
{
    String src = source;
    if (auto versionPos = src.indexOf("#version "))
    {
        // Append prefix after version.
        auto pos = src.indexOf("\n", versionPos);
        src.insert(pos + 1, prefix);
    }
    else
    {
        src = prefix + src;
    }
    return src;
}

void GLShader::compile(Type shaderType, const IByteArray &shaderSource)
{
#if defined (DE_OPENGL)
    static const String DEFAULT_VERSION("#version 330 core\n");
    // With non-ES OpenGL, ignore the precision attributes.
    static const String PREFIX("#ifndef GL_ES\n"
                               "#  define lowp\n"
                               "#  define mediump\n"
                               "#  define highp\n"
                               "#endif\n");
#else
    const int glesVer = DE_OPENGL_ES;
    static Block const DEFAULT_VERSION(glesVer == 30? "#version 300 es\n" : "#version 100\n");
    static Block const PREFIX("\n");
#endif

    DE_ASSERT(shaderType == Vertex || shaderType == Geometry || shaderType == Fragment);

    String preamble;
    String source = String(shaderSource);

    if (!source.contains("#version"))
    {
        preamble += DEFAULT_VERSION;
    }
    else
    {
        // Move the #version line to the preamble.
        const auto versionPos = source.indexOf("#version ");
        const auto endPos = source.indexOf("\n", versionPos);
        const auto len = endPos - versionPos + 1;
        const String versionLine = source.substr(versionPos, len);
        source.remove(versionPos, len);
        preamble += versionLine;
    }

    preamble += PREFIX;

//     Keep a copy of the source for possible recompilation.
//    d->compiledSource = src;

    setState(NotReady);
    d->type = shaderType;
    d->alloc();

    // Additional predefined symbols for the shader.
    if (shaderType == Vertex)
    {
        preamble += "#define DE_VERTEX_SHADER\n";

#if defined (DE_OPENGL) || (defined (DE_OPENGL_ES) && DE_OPENGL_ES == 30)
        preamble += "#define DE_VAR out\n"
                    "#define DE_ATTRIB in\n";
#else
        preamble += "#define DE_VAR varying\n"
                    "#define DE_ATTRIB attribute\n";
#endif
    }
    else if (shaderType == Geometry)
    {
        preamble += "#define DE_GEOMETRY_SHADER\n";
    }
    else
    {
        preamble += "#define DE_FRAGMENT_SHADER\n";

#if defined (DE_OPENGL_ES)
        // Precision qualifiers required in fragment shaders.
        preamble += "precision highp float;\n"
                    "precision highp int;\n";
#endif

#if defined (DE_OPENGL) || (defined (DE_OPENGL_ES) && DE_OPENGL_ES == 30)
        preamble += "#define DE_VAR in\n"
                   "layout(location = 0) out vec4 out_FragColor;\n";
#else
        preamble += "#define DE_VAR varying\n"
                    "#define out_FragColor gl_FragColor\n";
#endif
    }
    preamble += Stringf("#define DE_MAX_BATCH_UNIFORMS %d\n", MAX_BATCH_UNIFORMS);

#if defined (DE_OPENGL) || (defined (DE_OPENGL_ES) && DE_OPENGL_ES == 30)
    preamble += "#define DE_LAYOUT_LOC(x) layout(location = x)\n";
#else
    preamble += "#define DE_LAYOUT_LOC(x)\n";
#endif

    preamble += "#line 1\n";

    const char *srcPtr[] = {preamble.c_str(), source.c_str()};
    glShaderSource(d->name, 2, srcPtr, 0);
    glCompileShader(d->name);
    LIBGUI_ASSERT_GL_OK();

    // Check the compilation status.
    GLint status;
    glGetShaderiv(d->name, GL_COMPILE_STATUS, &status);
    if (!status)
    {
        dint32 logSize = 0;
        dint32 count = 0;
        glGetShaderiv(d->name, GL_INFO_LOG_LENGTH, &logSize);

        Block log{dsize(logSize)};
        glGetShaderInfoLog(d->name, logSize, &count, reinterpret_cast<GLchar *>(log.data()));

#if defined (DE_DEBUG)
        {
            int lineNum = 1;
            for (const auto &line : source.splitRef("\n"))
            {
                debug("%4i: %s", lineNum++, line.toStdString().c_str());
            }
        }
#endif

        throw CompilerError("GLShader::compile",
                            "Compilation of " + String(d->type == Fragment? "fragment" :
                                                       d->type == Geometry? "geometry" :
                                                                            "vertex") +
                            " shader failed:\n" + log);
    }

    setState(Ready);
}

//void GLShader::recompile()
//{
//    d->release();
//    compile(d->type, d->compiledSource);
//    DE_ASSERT(isReady());
//}

} // namespace de
