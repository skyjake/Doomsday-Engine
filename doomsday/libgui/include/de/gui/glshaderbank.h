/** @file glshaderbank.h  Bank containing GL shaders.
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

#ifndef LIBGUI_GLSHADERBANK_H
#define LIBGUI_GLSHADERBANK_H

#include <de/InfoBank>
#include "../GLShader"

namespace de {

class GLProgram;

/**
 * Bank containing GL shaders.
 *
 * Shader objects are automatically shared between created programs. Programs
 * can be are created based on definitions from an Info file.
 *
 * Shaders and programs cannot be accessed until OpenGL is ready.
 */
class GLShaderBank : public InfoBank
{
public:
    GLShaderBank();

    void addFromInfo(File const &file);

    GLShader &shader(Path const &path, GLShader::Type type) const;

    /**
     * Builds a GL program using the defined shaders.
     *
     * @param program  Program to build.
     * @param path     Identifier of the shader.
     *
     * @return Reference to @a program.
     */
    GLProgram &build(GLProgram &program, Path const &path) const;

protected:
    ISource *newSourceFromInfo(String const &id);
    IData *loadFromSource(ISource &source);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_GLSHADERBANK_H
