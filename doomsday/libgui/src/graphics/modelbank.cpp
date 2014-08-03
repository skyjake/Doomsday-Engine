/** @file modelbank.cpp  Bank containing 3D models.
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

#include "de/ModelBank"
#include <de/App>

namespace de {

DENG2_PIMPL(ModelBank)
{
    /// Source information for loading a model.
    struct Source : public ISource
    {
        String path; ///< Path to a model file.

        Source(String const &sourcePath) : path(sourcePath) {}
    };

    /// Loaded model instance.
    struct Data : public IData
    {
        ModelDrawable model;
        QScopedPointer<IAnimation> animation;

        Data(String const &path)
        {
            model.load(App::rootFolder().locate<File>(path));
        }
    };

    Instance(Public *i) : Base(i)
    {}
};

ModelBank::ModelBank() : Bank("ModelBank", BackgroundThread)
{}

void ModelBank::add(DotPath const &id, String const &sourcePath)
{
    return Bank::add(id, new Instance::Source(sourcePath));
}

ModelDrawable &ModelBank::model(DotPath const &id)
{
    return data(id).as<Instance::Data>().model;
}

void ModelBank::setAnimation(DotPath const &id, IAnimation *anim)
{
    data(id).as<Instance::Data>().animation.reset(anim);
}

ModelBank::IAnimation const *ModelBank::animation(DotPath const &id) const
{
    return data(id).as<Instance::Data>().animation.data();
}

Bank::IData *ModelBank::loadFromSource(ISource &source)
{
    return new Instance::Data(source.as<Instance::Source>().path);
}

} // namespace de
