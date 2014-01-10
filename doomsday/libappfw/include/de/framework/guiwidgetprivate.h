#ifndef LIBAPPFW_GUIWIDGETPRIVATE_H
#define LIBAPPFW_GUIWIDGETPRIVATE_H

#include <de/libdeng2.h>
#include "../GuiRootWidget"

namespace de {

class Style;

/**
 * Base class for GuiWidget-derived widgets' private implementation. Provides
 * easy access to the root widget and shared GL resources. This should be used
 * as the base class for private implementations if GL resources are being
 * used (i.e., glInit() and glDeinit() are being called).
 *
 * Use DENG_GUI_PIMPL() instead of the DENG2_PIMPL() macro.
 *
 * Note that GuiWidgetPrivate automatically observes the root widget's atlas
 * content repositioning, so derived private implementations can just override
 * the observer method if necessary.
 */
template <typename PublicType>
class GuiWidgetPrivate : public Private<PublicType>,
                         DENG2_OBSERVES(Atlas, Reposition)
{
public:
    typedef GuiWidgetPrivate<PublicType> Base; // shadows Private<>::Base

public:
    GuiWidgetPrivate(PublicType &i)
        : Private<PublicType>(i),
          _observingAtlas(0)
    {}

    GuiWidgetPrivate(PublicType *i)
        : Private<PublicType>(i),
          _observingAtlas(0)
    {}

    virtual ~GuiWidgetPrivate()
    {
        if(_observingAtlas)
        {
            _observingAtlas->audienceForReposition -= this;
        }

        /**
         * Ensure that the derived's class's glDeinit() method has been
         * called before the private class instance is destroyed. At least
         * classes that have GuiWidget as the immediate parent class need to
         * call deinitialize() in their destructors.
         *
         * @see GuiWidget::destroy()
         */
        DENG2_ASSERT(!Base::self.isInitialized());
    }

    void observeRootAtlas() const
    {
        if(!_observingAtlas)
        {
            // Automatically start observing the root atlas.
            _observingAtlas = &root().atlas();
            _observingAtlas->audienceForReposition += this;
        }
    }

    bool hasRoot() const
    {
        return Base::self.hasRoot();
    }

    GuiRootWidget &root() const
    {
        DENG2_ASSERT(hasRoot());
        return Base::self.root();
    }

    AtlasTexture &atlas() const
    {        
        observeRootAtlas();
        return root().atlas();
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

    Style const &style() const
    {
        return Base::self.style();
    }

    void atlasContentRepositioned(Atlas &atlas)
    {
        if(_observingAtlas == &atlas)
        {
            // Make sure the new texture coordinates get used by the widget.
            Base::self.requestGeometry();
        }
    }

private:
    mutable Atlas *_observingAtlas;
};

#define DENG_GUI_PIMPL(ClassName) \
    typedef ClassName Public; \
    struct ClassName::Instance : public de::GuiWidgetPrivate<ClassName>

} // namespace de

#endif // LIBAPPFW_GUIWIDGETPRIVATE_H
