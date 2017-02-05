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

#include "de/ModelBank"
#include <de/App>
#include <de/Folder>

namespace de {

DENG2_PIMPL(ModelBank)
{
    Constructor modelConstructor;

    /// Source information for loading a model.
    struct Source : public ISource
    {
        String path; ///< Path to a model file.

        Source(String const &sourcePath) : path(sourcePath) {}
    };

    /// Loaded model instance.
    struct Data : public IData
    {
        std::unique_ptr<ModelDrawable> model;
        std::unique_ptr<IUserData> userData;

        Data(ModelDrawable *model, String const &path)
            : model(model)
        {
            model->load(App::rootFolder().locate<File>(path));
        }
    };

    Impl(Public *i, Constructor c)
        : Base(i)
        , modelConstructor(c? c : [] () { return new ModelDrawable; })
    {}
};

ModelBank::ModelBank(Constructor modelConstructor)
    : Bank("ModelBank", BackgroundThread)
    , d(new Impl(this, modelConstructor))
{}

void ModelBank::add(DotPath const &id, String const &sourcePath)
{
    return Bank::add(id, new Impl::Source(sourcePath));
}

ModelDrawable &ModelBank::model(DotPath const &id)
{
    return *data(id).as<Impl::Data>().model;
}

void ModelBank::setUserData(DotPath const &id, IUserData *anim)
{
    data(id).as<Impl::Data>().userData.reset(anim);
}

ModelBank::IUserData const *ModelBank::userData(DotPath const &id) const
{
    return data(id).as<Impl::Data>().userData.get();
}

ModelBank::ModelWithData ModelBank::modelAndData(DotPath const &id) const
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
