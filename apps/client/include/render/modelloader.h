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

#ifndef DE_CLIENT_MODELLOADER_H
#define DE_CLIENT_MODELLOADER_H

#include <de/glprogram.h>
#include <de/modelbank.h>

namespace render {

/**
 * Observes available model assets, and loads and unloads 3D models as needed.
 */
class ModelLoader
{
public:
    DE_AUDIENCE(NewProgram, void newProgramCreated(de::GLProgram &))

    DE_ERROR(DefinitionError);
    DE_ERROR(TextureMappingError);

public:
    ModelLoader();

    de::ModelBank &bank();

    const de::ModelBank &bank() const;

    void glInit();

    void glDeinit();

    /**
     * Looks up the name of a shader based on a GLProgram instance.
     *
     * @param program  Shader program. Must be a shader program
     *                 created and owned by ModelRenderer.
     * @return Name of the shader (in the shader bank).
     */
    de::String shaderName(const de::GLProgram &program) const;

    /**
     * Looks up the definition of a shader based on a GLProgram instance.
     *
     * @param program  Shader program. Must be a shader program
     *                 created and owned by ModelRenderer.
     * @return Shader definition record.
     */
    const de::Record &shaderDefinition(const de::GLProgram &program) const;

public:
    static int identifierFromText(const de::String &text,
                                  const std::function<int (const de::String &)>& resolver);

    static const char *DEF_ANIMATION;
    static const char *DEF_MATERIAL;
    static const char *DEF_PASS;
    static const char *DEF_RENDER;

private:
    DE_PRIVATE(d)
};

} // namespace render

#endif // DE_CLIENT_MODELLOADER_H
