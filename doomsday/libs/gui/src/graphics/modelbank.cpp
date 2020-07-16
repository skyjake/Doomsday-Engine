/** @file modelbank.cpp  Bank containing 3D models.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/modelbank.h"
#include <de/app.h>
#include <de/folder.h>

namespace de {

DE_PIMPL(ModelBank)
{
    Constructor modelConstructor;

    /// Source information for loading a model.
    struct Source : public ISource
    {
        String path; ///< Path to a model file.

        Source(const String &sourcePath) : path(sourcePath) {}
    };

    /// Loaded model instance.
    struct Data : public IData
    {
        std::unique_ptr<ModelDrawable> model;
        std::unique_ptr<IUserData> userData;

        Data(ModelDrawable *model, const String &path)
            : model(model)
        {
            model->load(App::rootFolder().locate<File>(path));
        }
    };

    Impl(Public *i, const Constructor& c)
        : Base(i)
        , modelConstructor(c? c : [] () { return new ModelDrawable; })
    {}
};

ModelBank::ModelBank(const Constructor& modelConstructor)
    : Bank("ModelBank", BackgroundThread)
    , d(new Impl(this, modelConstructor))
{}

void ModelBank::add(const DotPath &id, const String &sourcePath)
{
    return Bank::add(id, new Impl::Source(sourcePath));
}

ModelDrawable &ModelBank::model(const DotPath &id)
{
    return *data(id).as<Impl::Data>().model;
}

void ModelBank::setUserData(const DotPath &id, IUserData *anim)
{
    data(id).as<Impl::Data>().userData.reset(anim);
}

const ModelBank::IUserData *ModelBank::userData(const DotPath &id) const
{
    return data(id).as<Impl::Data>().userData.get();
}

ModelBank::ModelWithData ModelBank::modelAndData(const DotPath &id) const
{
    auto &item = data(id).as<Impl::Data>();
    return ModelWithData(item.model.get(), item.userData.get());
}

Bank::IData *ModelBank::loadFromSource(ISource &source)
{
    return new Impl::Data(d->modelConstructor(),
                              source.as<Impl::Source>().path);
}

} // namespace de
