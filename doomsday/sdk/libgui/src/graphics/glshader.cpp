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

//int const GLShader::MAX_BATCH_UNIFORMS = 64;

DENG2_PIMPL(GLShader)
{
    GLuint name;
    Type type;
    Block compiledSource;

    Impl(Public *i) : Base(i), name(0), type(Vertex)
    {}

    ~Impl()
    {
        release();
    }

    void alloc()
    {
        if (!name)
        {
            name = LIBGUI_GL.glCreateShader(type == Vertex? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
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
            LIBGUI_GL.glDeleteShader(name);
            name = 0;
        }
        self().setState(Asset::NotReady);
    }
};

GLShader::GLShader() : d(new Impl(this))
{}

GLShader::GLShader(Type shaderType, IByteArray const &source) : d(new Impl(this))
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

Block GLShader::prefixToSource(Block const &source, Block const &prefix)
{
    Block src = source;
    int versionPos = src.indexOf("#version ");
    if (versionPos >= 0)
    {
        // Append prefix after version.
        int pos = src.indexOf('\n', versionPos);
        src.insert(pos + 1, prefix);
    }
    else
    {
        src = prefix + src;
    }
    return src;
}

void GLShader::compile(Type shaderType, IByteArray const &source)
{
#if defined (DENG_OPENGL)
    static QByteArray const DEFAULT_VERSION("#version 330\n");
    // With non-ES OpenGL, ignore the precision attributes.
    static QByteArray const PREFIX("#ifndef GL_ES\n"
                                   "#  define lowp\n"
                                   "#  define mediump\n"
                                   "#  define highp\n"
                                   "#endif\n");
#else
    int const glesVer = DENG_OPENGL_ES;
    static QByteArray const DEFAULT_VERSION(glesVer == 30? "#version 300 es\n" : "#version 100\n");
    static QByteArray const PREFIX("\n");
#endif

    DENG2_ASSERT(shaderType == Vertex || shaderType == Fragment);

    setState(NotReady);

    // Keep a copy of the source for possible recompilation.
    d->compiledSource = source;

    d->type = shaderType;
    d->alloc();

    // Additional predefined symbols for the shader.
    QByteArray predefs;
    if (shaderType == Vertex)
    {
        predefs = QByteArray("#define DENG_VERTEX_SHADER\n");

#if defined (DENG_OPENGL) || (defined (DENG_OPENGL_ES) && DENG_OPENGL_ES == 30)
        predefs += "#define DENG_VAR out\n"
                   "#define DENG_ATTRIB in\n";
#else
        predefs += "#define DENG_VAR varying\n"
                   "#define DENG_ATTRIB attribute\n";
#endif
    }
    else
    {
        predefs = QByteArray("#define DENG_FRAGMENT_SHADER\n");

#if defined (DENG_OPENGL_ES)
        // Precision qualifiers required in fragment shaders.
        predefs += "precision highp float;\n"
                   "precision highp int;\n";
#endif

#if defined (DENG_OPENGL) || (defined (DENG_OPENGL_ES) && DENG_OPENGL_ES == 30)
        predefs += "#define DENG_VAR in\n"
                   "out vec4 out_FragColor;\n";
#else
        predefs += "#define DENG_VAR varying\n"
                   "#define out_FragColor gl_FragColor\n";
#endif
    }
    predefs += "#define DENG_MAX_BATCH_UNIFORMS " + QByteArray::number(MAX_BATCH_UNIFORMS) + "\n";

#if defined (DENG_OPENGL) || (defined (DENG_OPENGL_ES) && DENG_OPENGL_ES == 30)
    predefs += "#define DENG_LAYOUT_LOC(x) layout(location = x)\n";
#else
    predefs += "#define DENG_LAYOUT_LOC(x)\n";
#endif

    // Prepare the shader source. This would be the time to substitute any
    // remaining symbols in the shader source.
    Block src = prefixToSource(source, PREFIX + predefs);

    // If version has not been specified, use the default one.
    if (!src.contains("#version"))
    {
        src = DEFAULT_VERSION + src;
    }

    char const *srcPtr = src.constData();
    LIBGUI_GL.glShaderSource(d->name, 1, &srcPtr, 0);

    LIBGUI_GL.glCompileShader(d->name);

    // Check the compilation status.
    GLint status;
    LIBGUI_GL.glGetShaderiv(d->name, GL_COMPILE_STATUS, &status);
    if (!status)
    {
        dint32 logSize = 0;
        dint32 count = 0;
        LIBGUI_GL.glGetShaderiv(d->name, GL_INFO_LOG_LENGTH, &logSize);

        Block log(logSize);
        LIBGUI_GL.glGetShaderInfoLog(d->name, logSize, &count, reinterpret_cast<GLchar *>(log.data()));

        throw CompilerError("GLShader::compile",
                            "Compilation of " + String(d->type == Fragment? "fragment" : "vertex") +
                            " shader failed:\n" + log);
    }

    setState(Ready);
}

void GLShader::recompile()
{
    d->release();
    compile(d->type, d->compiledSource);
    DENG2_ASSERT(isReady());
}

} // namespace de
