/** @file guiwidgetprivate.h  Template for GUI widget private implementation.
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

#ifndef LIBAPPFW_GUIWIDGETPRIVATE_H
#define LIBAPPFW_GUIWIDGETPRIVATE_H

#include <de/libcore.h>
#include "de/guirootwidget.h"

namespace de {

class Style;
class Font;

/**
 * Base class for GuiWidget-derived widgets' private implementation. Provides
 * easy access to the root widget and shared GL resources. This should be used
 * as the base class for private implementations if GL resources are being
 * used (i.e., glInit() and glDeinit() are being called).
 *
 * Use DE_GUI_PIMPL() instead of the DE_PIMPL() macro.
 *
 * Note that GuiWidgetPrivate automatically observes the root widget's atlas
 * content repositioning, so derived private implementations can just override
 * the observer method if necessary.
 *
 * @ingroup appfw
 */
template <typename PublicType>
class GuiWidgetPrivate : public Private<PublicType>,
                         DE_OBSERVES(Atlas, Reposition),
                         DE_OBSERVES(Asset, Deletion)
{
public:
    typedef GuiWidgetPrivate<PublicType> Base; // shadows Private<>::Base

public:
    GuiWidgetPrivate(PublicType &i) : Private<PublicType>(i) {}

    GuiWidgetPrivate(PublicType *i) : Private<PublicType>(i) {}

    virtual ~GuiWidgetPrivate()
    {
        forgetRootAtlas();

        /**
         * Ensure that the derived's class's glDeinit() method has been
         * called before the private class instance is destroyed. At least
         * classes that have GuiWidget as the immediate parent class need to
         * call deinitialize() in their destructors.
         *
         * @see GuiWidget::destroy()
         */
        DE_ASSERT(!Base::self().isInitialized());
    }

    void forgetRootAtlas()
    {
        if (_observingAtlas)
        {
            _observingAtlas->audienceForReposition() -= this;
            _observingAtlas->Asset::audienceForDeletion() -= this;
            _observingAtlas = nullptr;
        }
    }

    void observeRootAtlas() const
    {
        if (!_observingAtlas)
        {
            // Automatically start observing the root atlas.
            _observingAtlas = &root().atlas();
            _observingAtlas->audienceForReposition() += this;
            _observingAtlas->Asset::audienceForDeletion() += this;
        }
    }

    bool hasRoot() const
    {
        return Base::self().hasRoot();
    }

    GuiRootWidget &root() const
    {
        DE_ASSERT(hasRoot());
        return Base::self().root();
    }

    AtlasTexture &atlas() const
    {
        observeRootAtlas();
        return *_observingAtlas;
    }

    GLUniform &uAtlas() const
    {
        observeRootAtlas();
        return root().uAtlas();
    }

    GLShaderBank &shaders() const
    {
        return root().shaders();
    }

    const Style &style() const
    {
        return Base::self().style();
    }

    const Rule &rule(const DotPath &path) const
    {
        return Base::self().rule(path);
    }

    const Font &font(const DotPath &path) const
    {
        return style().fonts().font(path);
    }

    void atlasContentRepositioned(Atlas &atlas)
    {
        if (_observingAtlas == &atlas)
        {
            // Make sure the new texture coordinates get used by the widget.
            Base::self().requestGeometry();
        }
    }

    void assetBeingDeleted(Asset &a)
    {
        if (_observingAtlas == &a)
        {
            _observingAtlas = nullptr;
        }
    }

private:
    mutable AtlasTexture *_observingAtlas = nullptr;
};

#define DE_GUI_PIMPL(ClassName) \
    struct ClassName::Impl : public de::GuiWidgetPrivate<ClassName>

} // namespace de

#endif // LIBAPPFW_GUIWIDGETPRIVATE_H
