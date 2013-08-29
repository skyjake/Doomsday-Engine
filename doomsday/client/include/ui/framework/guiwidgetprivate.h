#ifndef DENG_CLIENT_GUIWIDGETPRIVATE_H
#define DENG_CLIENT_GUIWIDGETPRIVATE_H

#include <de/libdeng2.h>
#include "guirootwidget.h"

class Style;

/**
 * Base class for GuiWidget-derived widgets' private implementation. Provides
 * easy access to the root widget and shared GL resources. This should be used
 * as the base class for private implementations if GL resources are being
 * used (i.e., glInit() and glDeinit() are being called).
 *
 * Use DENG_GUI_PIMPL() instead of the DENG2_PIMPL() macro.
 */
template <typename PublicType>
class GuiWidgetPrivate : public de::Private<PublicType>
{
public:
    typedef GuiWidgetPrivate<PublicType> Base; // shadows de::Private<>::Base

public:
    GuiWidgetPrivate(PublicType &i) : de::Private<PublicType>(i) {}

    GuiWidgetPrivate(PublicType *i) : de::Private<PublicType>(i) {}

    virtual ~GuiWidgetPrivate()
    {
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

    bool hasRoot() const
    {
        return Base::self.hasRoot();
    }

    GuiRootWidget &root() const
    {
        DENG2_ASSERT(hasRoot());
        return Base::self.root();
    }

    de::AtlasTexture &atlas() const
    {
        return root().atlas();
    }

    de::GLUniform &uAtlas() const
    {
        return root().uAtlas();
    }

    de::GLShaderBank &shaders() const
    {
        return root().shaders();
    }

    Style const &style() const
    {
        return Base::self.style();
    }
};

#define DENG_GUI_PIMPL(ClassName) \
    typedef ClassName Public; \
    struct ClassName::Instance : public GuiWidgetPrivate<ClassName>

#endif // DENG_CLIENT_GUIWIDGETPRIVATE_H
