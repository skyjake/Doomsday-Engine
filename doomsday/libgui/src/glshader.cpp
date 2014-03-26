/** @file glshader.cpp  GL shader.
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

#include "de/GLShader"
#include "de/GuiApp"
#include "de/gui/opengl.h"
#include <de/Block>
#include <de/String>

namespace de {

DENG2_PIMPL(GLShader)
, DENG2_OBSERVES(GuiApp, GLContextChange)
{
    GLuint name;
    Type type;
    Block compiledSource;

    Instance(Public *i) : Base(i), name(0), type(Vertex)
    {
        //DENG2_GUI_APP->audienceForGLContextChange += this;
    }

    ~Instance()
    {
        //DENG2_GUI_APP->audienceForGLContextChange -= this;
        release();
    }

    void alloc()
    {
        if(!name)
        {
            name = glCreateShader(type == Vertex? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
            LIBGUI_ASSERT_GL_OK();
            if(!name)
            {
                throw AllocError("GLShader::alloc", "Failed to create shader");
            }
        }
    }

    void release()
    {
        if(name)
        {
            glDeleteShader(name);
            name = 0;
        }
        self.setState(Asset::NotReady);
    }

    void appGLContextChanged()
    {
        /*
        qDebug() << "Recompiling shader" << name;

        self.recompile();
        */
    }
};

GLShader::GLShader() : d(new Instance(this))
{}

GLShader::GLShader(Type shaderType, IByteArray const &source) : d(new Instance(this))
{
    try
    {
        compile(shaderType, source);
    }
    catch(...)
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
    if(versionPos >= 0)
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
#ifndef LIBGUI_GLES2
    // With non-ES OpenGL, ignore the precision attributes.
    static QByteArray prefix("#ifndef GL_ES\n#define lowp\n#define mediump\n#define highp\n#endif\n");
#endif

    DENG2_ASSERT(shaderType == Vertex || shaderType == Fragment);

    setState(NotReady);

    // Keep a copy of the source for possible recompilation.
    d->compiledSource = source;

    d->type = shaderType;
    d->alloc();

    // Prepare the shader source. This would be the time to substitute any
    // remaining symbols in the shader source.
    Block src = prefixToSource(source, prefix);
    src.append('\0');

    char const *srcPtr = src.constData();
    glShaderSource(d->name, 1, &srcPtr, 0);

    glCompileShader(d->name);

    // Check the compilation status.
    GLint status;
    glGetShaderiv(d->name, GL_COMPILE_STATUS, &status);
    if(!status)
    {
        dint32 logSize = 0;
        dint32 count = 0;
        glGetShaderiv(d->name, GL_INFO_LOG_LENGTH, &logSize);

        Block log(logSize);
        glGetShaderInfoLog(d->name, logSize, &count, reinterpret_cast<GLchar *>(log.data()));

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
