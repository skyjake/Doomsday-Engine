/** @file modelloader.h  Model loader.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_MODELLOADER_H
#define DENG_CLIENT_MODELLOADER_H

#include <de/GLProgram>
#include <de/ModelBank>

namespace render {

/**
 * Observes available model assets, and loads and unloads 3D models as needed.
 */
class ModelLoader
{
public:
    DENG2_DEFINE_AUDIENCE2(NewProgram, void newProgramCreated(de::GLProgram &))

    DENG2_ERROR(DefinitionError);
    DENG2_ERROR(TextureMappingError);

public:
    ModelLoader();

    de::ModelBank &bank();

    de::ModelBank const &bank() const;

    void glInit();

    void glDeinit();

    /**
     * Looks up the name of a shader based on a GLProgram instance.
     *
     * @param program  Shader program. Must be a shader program
     *                 created and owned by ModelRenderer.
     * @return Name of the shader (in the shader bank).
     */
    de::String shaderName(de::GLProgram const &program) const;

    /**
     * Looks up the definition of a shader based on a GLProgram instance.
     *
     * @param program  Shader program. Must be a shader program
     *                 created and owned by ModelRenderer.
     * @return Shader definition record.
     */
    de::Record const &shaderDefinition(de::GLProgram const &program) const;

public:
    static int identifierFromText(de::String const &text,
                                  std::function<int (de::String const &)> resolver);

private:
    DENG2_PRIVATE(d)
};

} // namespace render

#endif // DENG_CLIENT_MODELLOADER_H
