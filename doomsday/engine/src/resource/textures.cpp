/**
 * @file textures.cpp
 * Textures collection. @ingroup resource
 *
 * @authors Copyright &copy; 2010-2012 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <cstdlib>

#include "de_base.h"
#include "de_console.h"
#include "gl/gl_texmanager.h"
#include "m_misc.h" // for M_NumDigits

#include <QtAlgorithms>
#include <QList>
#include <de/Error>
#include <de/Log>
#include <de/PathTree>

#include "resource/compositetexture.h"
#include "resource/texturemetafile.h"
#include "resource/textures.h"

char const *TexSource_Name(TexSource source)
{
    if(source == TEXS_ORIGINAL) return "original";
    if(source == TEXS_EXTERNAL) return "external";
    return "none";
}

D_CMD(ListTextures);
D_CMD(InspectTexture);
#if _DEBUG
D_CMD(PrintTextureStats);
#endif

namespace de {

static Uri emptyUri;

Texture *Textures::ResourceClass::interpret(TextureMetaFile &metafile, Size2Raw const &dimensions,
    Texture::Flags flags, void *userData)
{
    LOG_AS("Textures::ResourceClass::interpret");
    return new Texture(metafile.lookupTextureId(), dimensions, flags, userData);
}

Texture *Textures::ResourceClass::interpret(TextureMetaFile &metafile, Texture::Flags flags,
    void *userData)
{
    return interpret(metafile, Size2Raw(0, 0), flags, userData);
}

TextureMetaFile::TextureMetaFile(PathTree::NodeArgs const &args) : Node(args),
    uniqueId_(0), resourceUri_(), texture_(0)
{}

TextureMetaFile::~TextureMetaFile()
{
    LOG_AS("~TextureMetaFile");
    if(texture_)
    {
#if _DEBUG
        LOG_WARNING("\"%s\" still has an associated texture!")
            << composeUri();
#endif
        delete texture_;
    }

    textures().deindex(*this);
}

Textures &TextureMetaFile::textures()
{
    return *App_Textures();
}

Texture *TextureMetaFile::define(Size2Raw const &dimensions, Texture::Flags flags)
{
    if(Texture *tex = texture())
    {
#if _DEBUG
        LOG_WARNING("A Texture with uri \"%s\" already exists, returning existing.")
            << composeUri();
#endif
        tex->setDimensions(dimensions);
        tex->flagCustom(!!(flags & Texture::Custom));

        /// @todo Materials and Surfaces should be notified of this!
        return tex;
    }

    Texture *tex = Textures::ResourceClass::interpret(*this, dimensions, flags);
    if(!texture()) setTexture(tex);
    return tex;
}

Texture *TextureMetaFile::define(Texture::Flags flags)
{
    return define(Size2Raw(0, 0), flags);
}

Textures::Scheme &TextureMetaFile::scheme() const
{
    LOG_AS("TextureMetaFile::scheme");
    /// @todo Optimize: TextureMetaFile should contain a link to the owning Textures::Scheme.
    Textures::Schemes const &schemes = textures().allSchemes();
    DENG2_FOR_EACH_CONST(Textures::Schemes, i, schemes)
    {
        Textures::Scheme &scheme = **i;
        if(&scheme.index() == &tree()) return scheme;
    }

    // This should never happen...
    /// @throw Error Failed to determine the scheme of the metafile.
    throw Error("TextureMetaFile::scheme", String("Failed to determine scheme for metafile [%p].").arg(de::dintptr(this)));
}

Uri TextureMetaFile::composeUri(QChar sep) const
{
    return Uri(scheme().name(), path(sep));
}

Uri TextureMetaFile::composeUrn() const
{
    return Uri("urn", String("%1:%2").arg(scheme().name()).arg(uniqueId_, 0, 10));
}

Uri const &TextureMetaFile::resourceUri() const
{
    return resourceUri_;
}

bool TextureMetaFile::setResourceUri(Uri const &newUri)
{
    // Avoid resolving; compare as text.
    if(resourceUri_.asText() != newUri.asText())
    {
        resourceUri_ = newUri;
        return true;
    }
    return false;
}

textureid_t TextureMetaFile::lookupTextureId() const
{
    // If we have bound a texture it can provide the id.
    if(texture_)
    {
        textureid_t texId = texture_->primaryBind();
        if(texId != NOTEXTUREID) return texId;
    }

    // Otherwise look it up.
    return textures().idForMetaFile(*this);
}

Texture *TextureMetaFile::texture() const
{
    return texture_;
}

void TextureMetaFile::setTexture(Texture *newTexture)
{
    texture_ = newTexture;
}

int TextureMetaFile::uniqueId() const
{
    return uniqueId_;
}

bool TextureMetaFile::setUniqueId(int newUniqueId)
{
    if(uniqueId_ == newUniqueId) return false;

    uniqueId_ = newUniqueId;
    // We'll need to rebuild the id map too.
    scheme().markUniqueIdLutDirty();
    return true;
}

struct Textures::Scheme::Instance
{
    /// Symbolic name of the scheme.
    String name;

    /// Mappings from paths to metafiles.
    Textures::Scheme::Index *index_;

    /// LUT which translates scheme-unique-ids to their associated metafile (if any).
    /// Index with uniqueId - uniqueIdBase.
    QList<TextureMetaFile *> uniqueIdLut;
    bool uniqueIdLutDirty;
    int uniqueIdBase;

    Instance(String symbolicName)
        : name(symbolicName), index_(new Textures::Scheme::Index()),
          uniqueIdLut(), uniqueIdLutDirty(false), uniqueIdBase(0)
    {}

    ~Instance()
    {
        if(index_)
        {
            PathTreeIterator<PathTree> iter(index_->leafNodes());
            while(iter.hasNext())
            {
                TextureMetaFile &metafile = static_cast<TextureMetaFile &>(iter.next());
                deindex(metafile);
            }
            delete index_;
        }
    }

    bool inline uniqueIdInLutRange(int uniqueId) const
    {
        return (uniqueId - uniqueIdBase >= 0 && (uniqueId - uniqueIdBase) < uniqueIdLut.size());
    }

    void findUniqueIdRange(int *minId, int *maxId)
    {
        if(!minId && !maxId) return;

        if(!index_)
        {
            if(minId) *minId = 0;
            if(maxId) *maxId = 0;
            return;
        }

        if(minId) *minId = DDMAXINT;
        if(maxId) *maxId = DDMININT;

        PathTreeIterator<PathTree> iter(index_->leafNodes());
        while(iter.hasNext())
        {
            TextureMetaFile &metafile = static_cast<TextureMetaFile &>(iter.next());
            int const uniqueId = metafile.uniqueId();
            if(minId && uniqueId < *minId) *minId = uniqueId;
            if(maxId && uniqueId > *maxId) *maxId = uniqueId;
        }
    }

    void deindex(TextureMetaFile &metafile)
    {
        /// @todo Only destroy the texture if this is the last remaining reference.
        Texture *texture = metafile.texture();
        if(texture)
        {
            delete texture;
            metafile.setTexture(0);
        }

        unlinkInUniqueIdLut(metafile);
    }

    /// @pre uniqueIdLut is large enough if initialized!
    void unlinkInUniqueIdLut(TextureMetaFile &metafile)
    {
        // If the lut is already considered 'dirty' do not unlink.
        if(!uniqueIdLutDirty)
        {
            int uniqueId = metafile.uniqueId();
            DENG_ASSERT(uniqueIdInLutRange(uniqueId));
            uniqueIdLut[uniqueId - uniqueIdBase] = 0;
        }
    }

    /// @pre uniqueIdLut has been initialized and is large enough!
    void linkInUniqueIdLut(TextureMetaFile &metafile)
    {
        int uniqueId = metafile.uniqueId();
        DENG_ASSERT(uniqueIdInLutRange(uniqueId));
        uniqueIdLut[uniqueId - uniqueIdBase] = &metafile;
    }

    void rebuildUniqueIdLut()
    {
        // Is a rebuild necessary?
        if(!uniqueIdLutDirty) return;

        // Determine the size of the LUT.
        int minId, maxId;
        findUniqueIdRange(&minId, &maxId);

        int lutSize = 0;
        if(minId > maxId) // None found?
        {
            uniqueIdBase = 0;
        }
        else
        {
            uniqueIdBase = minId;
            lutSize = maxId - minId + 1;
        }

        // Fill the LUT with initial values.
        uniqueIdLut.reserve(lutSize);
        int i = 0;
        for(; i < uniqueIdLut.size(); ++i)
        {
            uniqueIdLut[i] = 0;
        }
        for(; i < lutSize; ++i)
        {
            uniqueIdLut.push_back(0);
        }

        if(lutSize)
        {
            // Populate the LUT.
            PathTreeIterator<PathTree> iter(index_->leafNodes());
            while(iter.hasNext())
            {
                linkInUniqueIdLut(static_cast<TextureMetaFile &>(iter.next()));
            }
        }

        uniqueIdLutDirty = false;
    }
};

Textures::Scheme::Scheme(String symbolicName)
{
    d = new Instance(symbolicName);
}

Textures::Scheme::~Scheme()
{
    delete d;
}

void Textures::Scheme::clear()
{
    if(d->index_)
    {
        PathTreeIterator<PathTree> iter(d->index_->leafNodes());
        while(iter.hasNext())
        {
            TextureMetaFile &metafile = static_cast<TextureMetaFile &>(iter.next());
            d->deindex(metafile);
        }
        d->index_->clear();
        d->uniqueIdLutDirty = true;
    }
}

String const &Textures::Scheme::name() const
{
    return d->name;
}

int Textures::Scheme::size() const
{
    return d->index_->size();
}

TextureMetaFile &Textures::Scheme::insertMetaFile(Path const &path)
{
    int sizeBefore = d->index_->size();
    TextureMetaFile &metafile = d->index_->insert(path);
    if(d->index_->size() != sizeBefore)
    {
        // We'll need to rebuild the unique id LUT after this.
        d->uniqueIdLutDirty = true;
    }
    return metafile;
}

TextureMetaFile const &Textures::Scheme::find(Path const &path) const
{
    try
    {
        return d->index_->find(path, PathTree::NoBranch | PathTree::MatchFull);
    }
    catch(Scheme::Index::NotFoundError const &er)
    {
        throw NotFoundError("Textures::Scheme::find", er.asText());
    }
}

TextureMetaFile &Textures::Scheme::find(Path const &path)
{
    TextureMetaFile const &found = const_cast<Textures::Scheme const *>(this)->find(path);
    return const_cast<TextureMetaFile &>(found);
}

TextureMetaFile const &Textures::Scheme::findByResourceUri(Uri const &uri) const
{
    if(!uri.isEmpty())
    {
        PathTreeIterator<PathTree> iter(d->index_->leafNodes());
        while(iter.hasNext())
        {
            TextureMetaFile &metafile = static_cast<TextureMetaFile &>(iter.next());
            if(metafile.resourceUri() == uri)
            {
                return metafile;
            }
        }
    }
    /// @throw NotFoundError  No metafile was found with a matching resource URI.
    throw NotFoundError("Textures::Scheme::findByResourceUri", "No metafile found with a resource URI matching \"" + uri + "\"");
}

TextureMetaFile &Textures::Scheme::findByResourceUri(Uri const &uri)
{
    TextureMetaFile const &found = const_cast<Textures::Scheme const *>(this)->findByResourceUri(uri);
    return const_cast<TextureMetaFile &>(found);
}

TextureMetaFile const &Textures::Scheme::findByUniqueId(int uniqueId) const
{
    d->rebuildUniqueIdLut();

    if(d->uniqueIdInLutRange(uniqueId))
    {
        TextureMetaFile *metafile = d->uniqueIdLut[uniqueId - d->uniqueIdBase];
        if(metafile) return *metafile;
    }
    /// @throw NotFoundError  No metafile was found with a matching resource URI.
    throw NotFoundError("Textures::Scheme::findByUniqueId", "No metafile found with a unique ID matching \"" + QString("%1").arg(uniqueId) + "\"");
}

TextureMetaFile &Textures::Scheme::findByUniqueId(int uniqueId)
{
    TextureMetaFile const &found = const_cast<Textures::Scheme const *>(this)->findByUniqueId(uniqueId);
    return const_cast<TextureMetaFile &>(found);
}

Textures::Scheme::Index const &Textures::Scheme::index() const
{
    return *d->index_;
}

void Textures::Scheme::markUniqueIdLutDirty()
{
    d->uniqueIdLutDirty = true;
}

enum ValidateUriFlag
{
    AnyScheme  = 0x1, ///< The scheme of the URI may be of zero-length; signifying "any scheme".
    NotUrn     = 0x2  ///< Do not accept a URN.
};
Q_DECLARE_FLAGS(ValidateUriFlags, ValidateUriFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(ValidateUriFlags)

struct Textures::Instance
{
    Textures& self;

    // LUT which translates textureid_t => TextureMetaFile*. Index with textureid_t-1
    QList<TextureMetaFile *> textureIdLut;

    /// System subspace schemes containing the textures.
    Textures::Schemes schemes;

    Instance(Textures *d) : self(*d)
    {}

    ~Instance()
    {
        DENG2_FOR_EACH(Textures::Schemes, i, schemes)
        {
            delete *i;
        }
        schemes.clear();
    }

    inline bool validTextureId(textureid_t id) const
    {
        return (id != NOTEXTUREID && id <= textureIdLut.size());
    }

    /// @pre textureIdLut has been initialized and is large enough!
    void unlinkFromTextureIdLut(TextureMetaFile &metafile)
    {
        textureid_t texId = metafile.lookupTextureId();
        if(!validTextureId(texId)) return; // Not linked.
        textureIdLut[texId - 1/*1-based index*/] = 0;
    }

    TextureMetaFile *metafileByTextureId(textureid_t id)
    {
        if(!validTextureId(id)) return 0;
        return textureIdLut[id - 1/*1-based index*/];
    }

    TextureMetaFile *metafileByUri(Uri const &validatedUri)
    {
        // Is this a URN? (of the form "urn:schemename:uniqueid")
        if(!validatedUri.scheme().compareWithoutCase("urn"))
        {
            String const &pathStr = validatedUri.path().toStringRef();
            int uIdPos = pathStr.indexOf(':');
            if(uIdPos > 0)
            {
                String schemeName = pathStr.left(uIdPos);
                int uniqueId      = pathStr.mid(uIdPos + 1 /*skip delimiter*/).toInt();

                try
                {
                    return &self.scheme(schemeName).findByUniqueId(uniqueId);
                }
                catch(Textures::Scheme::NotFoundError const &)
                {} // Ignore this error.
            }
            return 0; // Not found.
        }

        // No, this is a URI.
        String const &path = validatedUri.path();

        // Does the user want a metafile in a specific scheme?
        if(!validatedUri.scheme().isEmpty())
        {
            try
            {
                return &self.scheme(validatedUri.scheme()).find(path);
            }
            catch(Textures::Scheme::NotFoundError const &)
            {} // Ignore this error.
            return 0;
        }

        // No, check in each of these schemes (in priority order).
        /// @todo This priorty order should be defined by the user.
        static String const order[] = {
            "Sprites",
            "Textures",
            "Flats",
            "Patches",
            "System",
            "Details",
            "Reflections",
            "Masks",
            "ModelSkins",
            "ModelReflectionSkins",
            "Lightmaps",
            "Flaremaps",
            ""
        };
        for(int i = 0; !order[i].isEmpty(); ++i)
        {
            try
            {
                return &self.scheme(order[i]).find(path);
            }
            catch(Textures::Scheme::NotFoundError const &)
            {} // Ignore this error.
        }
        return 0; // Not found.
    }

    /**
     * @param uri       Uri to be validated.
     * @param flags     Validation flags.
     * @param quiet     @c true= Do not output validation remarks to the log.
     *
     * @return  @c true if @a Uri passes validation.
     */
    bool validateUri(Uri const &uri, ValidateUriFlags flags, bool quiet = false)
    {
        LOG_AS("validateUri");
        bool const isUrn = !uri.scheme().compareWithoutCase("urn");

        if(uri.isEmpty())
        {
            if(!quiet) LOG_MSG("Empty path in texture %s \"%s\".") << uri << (isUrn? "URN" : "URI");
            return false;
        }

        // If this is a URN we extract the scheme from the path.
        String schemeString;
        if(isUrn)
        {
            if(flags.testFlag(NotUrn))
            {
                if(!quiet) LOG_MSG("Texture URN \"%s\" supplied, URI required.") << uri;
                return false;
            }

            String const &pathStr = uri.path().toStringRef();
            int const uIdPos      = pathStr.indexOf(':');
            if(uIdPos > 0)
            {
                schemeString = pathStr.left(uIdPos);
            }
        }
        else
        {
            schemeString = uri.scheme();
        }

        if(schemeString.isEmpty())
        {
            if(!flags.testFlag(AnyScheme))
            {
                if(!quiet) LOG_MSG("Missing scheme in texture %s \"%s\".") << uri << (isUrn? "URN" : "URI");
                return false;
            }
        }
        else if(!self.knownScheme(schemeString))
        {
            if(!quiet) LOG_MSG("Unknown scheme in texture %s \"%s\".") << uri << (isUrn? "URN" : "URI");
            return false;
        }

        return true;
    }
};

void Textures::consoleRegister()
{
    C_CMD("inspecttexture", "ss",   InspectTexture)
    C_CMD("inspecttexture", "s",    InspectTexture)
    C_CMD("listtextures",   "ss",   ListTextures)
    C_CMD("listtextures",   "s",    ListTextures)
    C_CMD("listtextures",   "",     ListTextures)
#if _DEBUG
    C_CMD("texturestats",   NULL,   PrintTextureStats)
#endif
}

Textures::Textures()
{
    d = new Instance(this);
}

Textures::~Textures()
{
    clearAllSchemes();
    delete d;
}

Textures::Scheme& Textures::createScheme(String name)
{
    DENG_ASSERT(name.length() >= Scheme::min_name_length);

    // Ensure this is a unique name.
    if(knownScheme(name)) return scheme(name);

    // Create a new scheme.
    Scheme* newScheme = new Scheme(name);
    d->schemes.push_back(newScheme);
    return *newScheme;
}

int Textures::size() const
{
    return d->textureIdLut.size();
}

static void release(Texture *tex)
{
    /// Stub.
    GL_ReleaseGLTexturesByTexture(reinterpret_cast<texture_s *>(tex));
    /// @todo Update any Materials (and thus Surfaces) which reference this.
}

Texture *Textures::toTexture(textureid_t id) const
{
    LOG_AS("Textures::toTexture");
    if(TextureMetaFile *metafile = d->metafileByTextureId(id))
    {
        return metafile->texture();
    }

#if _DEBUG
    if(id != NOTEXTUREID)
        LOG_WARNING("Failed to locate texture for id #%i, returning 0.") << id;
#endif
    return 0;
}

TextureMetaFile *Textures::find(Uri const &uri) const
{
    LOG_AS("Textures::find");

    if(!Textures::size()) return 0;

    if(!d->validateUri(uri, AnyScheme, true /*quiet please*/))
    {
#if _DEBUG
        LOG_WARNING("URI \"%s\" failed validation, returning NULL.") << uri;
#endif
        return 0;
    }

    // Perform the search.
    return d->metafileByUri(uri);
}

TextureMetaFile *Textures::declare(Uri const &uri, int uniqueId, Uri const *resourceUri)
{
    LOG_AS("Textures::declare");

    if(uri.isEmpty()) return 0;

    // We require a properly formed uri (but not a urn - this is a path).
    if(!d->validateUri(uri, NotUrn, (verbose >= 1)))
    {
        LOG_WARNING("Failed declaring texture \"%s\" (invalid URI), ignoring.") << uri;
        return 0;
    }

    // Have we already created a binding for this?
    TextureMetaFile *metafile = d->metafileByUri(uri);
    if(!metafile)
    {
        /*
         * A new binding.
         */
        metafile = &scheme(uri.scheme()).insertMetaFile(uri.path());
        metafile->setUniqueId(uniqueId);

        // Link it into the id LUT.
        d->textureIdLut.push_back(metafile);
    }

    /**
     * (Re)configure this binding.
     */

    // We don't care whether these identfiers are truely unique. Our only
    // responsibility is to release textures when they change.
    bool releaseTexture = false;

    if(resourceUri && metafile->setResourceUri(*resourceUri))
    {
        releaseTexture = true;
    }

    if(metafile->setUniqueId(uniqueId))
    {
        releaseTexture = true;
    }

    if(releaseTexture && metafile->texture())
    {
        // The mapped resource is being replaced, so release any existing Texture.
        /// @todo Only release if this Texture is bound to only this binding.
        release(metafile->texture());
    }

    return metafile;
}

int Textures::uniqueId(textureid_t id) const
{
    LOG_AS("Textures::uniqueId");
    if(TextureMetaFile *metafile = d->metafileByTextureId(id))
    {
        return metafile->uniqueId();
    }

#if _DEBUG
    if(id != NOTEXTUREID)
        LOG_WARNING("Attempted with unbound textureId #%u, returning zero.") << id;
#endif
    return 0;
}

Uri const &Textures::resourceUri(textureid_t id) const
{
    LOG_AS("Textures::resourcePath");
    if(TextureMetaFile *metafile = d->metafileByTextureId(id))
    {
        return metafile->resourceUri();
    }

#if _DEBUG
    if(id != NOTEXTUREID)
        LOG_WARNING("Attempted with unbound textureId #%u, returning null-object.") << id;
#endif
    return emptyUri;
}

textureid_t Textures::id(Texture &tex) const
{
    LOG_AS("Textures::id");
    return tex.primaryBind();
}

textureid_t Textures::idForMetaFile(Textures::MetaFile const &metafile) const
{
    LOG_AS("Textures::idForMetaFile");
    /// @todo Optimize: (Low priority) do not use a linear search.
    int index = d->textureIdLut.indexOf(const_cast<Textures::MetaFile *>(&metafile));
    if(index >= 0)
    {
        return textureid_t(index + 1); // 1-based index.
    }
    return NOTEXTUREID; // Not linked.
}

bool Textures::knownScheme(String name) const
{
    if(!name.isEmpty())
    {
        DENG2_FOR_EACH(Schemes, i, d->schemes)
        {
            if(!(*i)->name().compareWithoutCase(name))
                return true;
        }
        //Schemes::iterator found = d->schemes.find(name.toLower());
        //if(found != d->schemes.end()) return true;
    }
    return false;
}

Textures::Scheme &Textures::scheme(String name) const
{
    LOG_AS("Textures::scheme");
    DENG2_FOR_EACH(Schemes, i, d->schemes)
    {
        if(!(*i)->name().compareWithoutCase(name))
            return **i;
    }
    /// @throw UnknownSchemeError An unknown scheme was referenced.
    throw Textures::UnknownSchemeError("Textures::scheme", "No scheme found matching '" + name + "'");
}

Textures::Schemes const& Textures::allSchemes() const
{
    return d->schemes;
}

Uri Textures::composeUri(textureid_t id) const
{
    LOG_AS("Textures::composeUri");
    if(TextureMetaFile *metafile = d->metafileByTextureId(id))
    {
        return metafile->composeUri();
    }

#if _DEBUG
    if(id != NOTEXTUREID)
        LOG_WARNING("Attempted with unbound textureId #%u, returning null-object.") << id;
#endif
    return Uri();
}

int Textures::iterate(String nameOfScheme,
    int (*callback)(Texture &tex, void *parameters), void *parameters) const
{
    if(!callback) return 0;

    if(!nameOfScheme.isEmpty())
    {
        PathTreeIterator<PathTree> iter(scheme(nameOfScheme).index().leafNodes());
        while(iter.hasNext())
        {
            TextureMetaFile &metafile = static_cast<TextureMetaFile &>(iter.next());
            if(!metafile.texture()) continue;

            int result = callback(*metafile.texture(), parameters);
            if(result) return result;
        }
    }
    else
    {
        DENG2_FOR_EACH_CONST(Schemes, i, d->schemes)
        {
            PathTreeIterator<PathTree> iter((*i)->index().leafNodes());
            while(iter.hasNext())
            {
                TextureMetaFile &metafile = static_cast<TextureMetaFile &>(iter.next());
                if(!metafile.texture()) continue;

                int result = callback(*metafile.texture(), parameters);
                if(result) return result;
            }
        }
    }
    return 0;
}

int Textures::iterateDeclared(String nameOfScheme,
    int (*callback)(TextureMetaFile &metafile, void *parameters), void *parameters) const
{
    if(!callback) return 0;

    if(!nameOfScheme.isEmpty())
    {
        PathTreeIterator<PathTree> iter(scheme(nameOfScheme).index().leafNodes());
        while(iter.hasNext())
        {
            TextureMetaFile &metafile = static_cast<TextureMetaFile &>(iter.next());
            int result = callback(metafile, parameters);
            if(result) return result;
        }
    }
    else
    {
        DENG2_FOR_EACH_CONST(Schemes, i, d->schemes)
        {
            PathTreeIterator<PathTree> iter((*i)->index().leafNodes());
            while(iter.hasNext())
            {
                TextureMetaFile &metafile = static_cast<TextureMetaFile &>(iter.next());
                int result = callback(metafile, parameters);
                if(result) return result;
            }
        }
    }

    return 0;
}

void Textures::deindex(TextureMetaFile &metafile)
{
    d->unlinkFromTextureIdLut(metafile);
}

static void printVariantInfo(TextureVariant &variant)
{
    float s, t;
    variant.coords(&s, &t);
    Con_Printf("  Source:%s Masked:%s Prepared:%s Uploaded:%s\n  Coords:(s:%g t:%g)\n",
               TexSource_Name(variant.source()),
               variant.isMasked()  ? "yes":"no",
               variant.isPrepared()? "yes":"no",
               variant.isUploaded()? "yes":"no", s, t);

    Con_Printf("  Specification: ");
    GL_PrintTextureVariantSpecification(variant.spec());
}

static void printTextureInfo(Texture &tex)
{
    Textures &textures = *App_Textures();
    Uri uri = textures.composeUri(textures.id(tex));
    QByteArray path = NativePath(uri.asText()).pretty().toUtf8();

    Con_Printf("Texture \"%s\" [%p] x%u origin:%s\n",
               path.constData(), (void *)&tex, tex.variantCount(),
               tex.isCustom()? "addon" : "game");

    bool willAssumeImageDimensions = (tex.width() == 0 && tex.height() == 0);
    if(willAssumeImageDimensions)
        Con_Printf("Dimensions: unknown (not yet loaded)\n");
    else
        Con_Printf("Dimensions: %d x %d\n", tex.width(), tex.height());

    uint variantIdx = 0;
    DENG2_FOR_EACH_CONST(Texture::Variants, i, tex.variantList())
    {
        TextureVariant &variant = **i;

        Con_Printf("Variant #%i: GLName:%u\n", variantIdx, variant.glName());
        printVariantInfo(variant);

        ++variantIdx;
    }
}

static void printTextureSummary(TextureMetaFile &metafile, bool printSchemeName)
{
    Uri uri = metafile.composeUri();
    QByteArray path = printSchemeName? uri.asText().toUtf8() : QByteArray::fromPercentEncoding(uri.path().toStringRef().toUtf8());

    QByteArray resourceUri = metafile.resourceUri().asText().toUtf8();
    if(resourceUri.isEmpty()) resourceUri = "N/A";

    Con_FPrintf(!metafile.texture()? CPF_LIGHT : CPF_WHITE,
                "%-*s %-6s x%u %s\n", printSchemeName? 22 : 14, path.constData(),
                !metafile.texture()? "unknown" : metafile.texture()->isCustom()? "addon" : "game",
                metafile.texture()->variantCount(), resourceUri.constData());
}

/**
 * @todo This logic should be implemented in de::PathTree -ds
 */
static QList<TextureMetaFile *> collectTextureMetaFiles(Textures::Scheme *scheme,
    Path const &path, QList<TextureMetaFile *> *storage = 0)
{
    int count = 0;

    if(scheme)
    {
        // Only consider textures in this scheme.
        PathTreeIterator<Textures::Scheme::Index> iter(scheme->index().leafNodes());
        while(iter.hasNext())
        {
            TextureMetaFile &metafile = static_cast<TextureMetaFile &>(iter.next());
            if(!path.isEmpty())
            {
                /// @todo Use PathTree::Node::compare()
                if(!metafile.path().toString().beginsWith(path, Qt::CaseInsensitive)) continue;
            }

            if(storage) // Store mode.
            {
                storage->push_back(&metafile);
            }
            else // Count mode.
            {
                ++count;
            }
        }
    }
    else
    {
        // Consider textures in any scheme.
        Textures::Schemes const &schemes = App_Textures()->allSchemes();
        DENG2_FOR_EACH_CONST(Textures::Schemes, i, schemes)
        {
            PathTreeIterator<Textures::Scheme::Index> iter((*i)->index().leafNodes());
            while(iter.hasNext())
            {
                TextureMetaFile &metafile = static_cast<TextureMetaFile &>(iter.next());
                if(!path.isEmpty())
                {
                    /// @todo Use PathTree::Node::compare()
                    if(!metafile.path().toString().beginsWith(path, Qt::CaseInsensitive)) continue;
                }

                if(storage) // Store mode.
                {
                    storage->push_back(&metafile);
                }
                else // Count mode.
                {
                    ++count;
                }
            }
        }
    }

    if(storage)
    {
        return *storage;
    }

    QList<TextureMetaFile*> result;
    if(count == 0) return result;

    result.reserve(count);
    return collectTextureMetaFiles(scheme, path, &result);
}

/**
 * Decode and then lexicographically compare the two metafile
 * paths, returning @c true if @a is less than @a b.
 */
static bool compareTextureMetaFilePathsAssending(TextureMetaFile const *a,
                                          TextureMetaFile const *b)
{
    String pathA = QString(QByteArray::fromPercentEncoding(a->path().toUtf8()));
    String pathB = QString(QByteArray::fromPercentEncoding(b->path().toUtf8()));
    return pathA.compareWithoutCase(pathB) < 0;
}

/**
 * @defgroup printTextureFlags  Print Texture Flags
 * @ingroup flags
 */
///@{
#define PTF_TRANSFORM_PATH_NO_SCHEME        0x1 ///< Do not print the scheme.
///@}

#define DEFAULT_PRINTTEXTUREFLAGS           0

/**
 * @param flags  @ref printTextureFlags
 */
static int printTextures2(Textures::Scheme *scheme, Path const &path, int flags)
{
    QList<TextureMetaFile *> found = collectTextureMetaFiles(scheme, path);
    if(found.isEmpty()) return 0;

    bool const printSchemeName = !(flags & PTF_TRANSFORM_PATH_NO_SCHEME);

    // Compose a heading.
    String heading = "Known textures";
    if(!printSchemeName && scheme)
        heading += " in scheme '" + scheme->name() + "'";
    if(!path.isEmpty())
        heading += " like \"" + NativePath(path).withSeparators('/') + "\"";
    heading += ":\n";

    // Print the result heading.
    Con_FPrintf(CPF_YELLOW, "%s", heading.toUtf8().constData());

    // Print the result index key.
    int numFoundDigits = MAX_OF(3/*idx*/, M_NumDigits(found.count()));
    int numUidDigits   = MAX_OF(3/*uid*/, M_NumDigits(App_Textures()->count()));

    Con_Printf(" %*s: %-*s %*s origin n# uri\n", numFoundDigits, "idx",
               printSchemeName? 22 : 14, printSchemeName? "scheme:path" : "path",
               numUidDigits, "uid");
    Con_PrintRuler();

    // Sort and print the index.
    qSort(found.begin(), found.end(), compareTextureMetaFilePathsAssending);
    int idx = 0;
    DENG2_FOR_EACH(QList<TextureMetaFile *>, i, found)
    {
        Con_Printf(" %*i: ", numFoundDigits, idx++);
        printTextureSummary(**i, printSchemeName);
    }

    return found.count();
}

static void printTextures(de::Uri const &search, int flags = DEFAULT_PRINTTEXTUREFLAGS)
{
    Textures &textures = *App_Textures();

    int printTotal = 0;

    // Collate and print results from all schemes?
    if(search.scheme().isEmpty() && !search.path().isEmpty())
    {
        printTotal = printTextures2(0/*any scheme*/, search.path(), flags & ~PTF_TRANSFORM_PATH_NO_SCHEME);
        Con_PrintRuler();
    }
    // Print results within only the one scheme?
    else if(textures.knownScheme(search.scheme()))
    {
        printTotal = printTextures2(&textures.scheme(search.scheme()), search.path(), flags | PTF_TRANSFORM_PATH_NO_SCHEME);
        Con_PrintRuler();
    }
    else
    {
        // Collect and sort results in each scheme separately.
        Textures::Schemes const &schemes = textures.allSchemes();
        DENG2_FOR_EACH_CONST(Textures::Schemes, i, schemes)
        {
            int numPrinted = printTextures2(*i, search.path(), flags | PTF_TRANSFORM_PATH_NO_SCHEME);
            if(numPrinted)
            {
                Con_PrintRuler();
                printTotal += numPrinted;
            }
        }
    }
    Con_Printf("Found %i %s.\n", printTotal, printTotal == 1? "Texture" : "Textures");
}

} // namespace de

/**
 * Arguments are assumed to be in a human-friendly, non-encoded representation.
 *
 * Supported forms (where <> denote keyword component names):
 * <pre>
 *     [0: <scheme>:<path>]
 *     [0: <scheme>]             - if @a matchSchemeOnly
 *     [0: <path>]
 *     [0: <scheme>, 1: <path>]
 * </pre>
 */
static de::Uri composeSearchUri(char **argv, int argc, bool matchSchemeOnly = true)
{
    if(argv)
    {
        // [0: <scheme>:<path>] or [0: <scheme>] or [0: <path>].
        if(argc == 1)
        {
            // Try to extract the scheme and encode the rest of the path.
            de::String rawUri = de::String(argv[0]);
            int pos = rawUri.indexOf(':');
            if(pos >= 0)
            {
                de::String scheme = rawUri.left(pos);
                rawUri.remove(0, pos + 1);
                return de::Uri(scheme, QString(QByteArray(rawUri.toUtf8()).toPercentEncoding()));
            }

            // Just a scheme name?
            if(matchSchemeOnly && App_Textures()->knownScheme(rawUri))
            {
                return de::Uri().setScheme(rawUri);
            }

            // Just a path.
            return de::Uri(de::Path(QString(QByteArray(rawUri.toUtf8()).toPercentEncoding())));
        }
        // [0: <scheme>, 1: <path>]
        if(argc == 2)
        {
            // Assign the scheme and encode the path.
            return de::Uri(argv[0], QString(QByteArray(argv[1]).toPercentEncoding()));
        }
    }
    return de::Uri();
}

static de::Textures* textures;

de::Textures* App_Textures()
{
    if(!textures) throw de::Error("App_Textures", "Textures collection not yet initialized");
    return textures;
}

void Textures_Init(void)
{
    DENG_ASSERT(!textures);
    textures = new de::Textures();
}

void Textures_Shutdown(void)
{
    if(!textures) return;
    delete textures; textures = 0;
}

/// @note Part of the Doomsday public API.
int Textures_UniqueId2(Uri const *_uri, boolean quiet)
{
    LOG_AS("Textures_UniqueId");
    if(!_uri) return -1;
    de::Uri const &uri = reinterpret_cast<de::Uri const &>(*_uri);

    de::Textures &textures = *App_Textures();
    if(de::TextureMetaFile *metafile = textures.find(uri))
    {
        return metafile->uniqueId();
    }

    if(!quiet)
    {
        LOG_WARNING("Unknown texture %s.") << uri;
    }
    return -1;
}

/// @note Part of the Doomsday public API.
int Textures_UniqueId(Uri const *uri)
{
    return Textures_UniqueId2(uri, false);
}

D_CMD(ListTextures)
{
    DENG2_UNUSED(src);

    de::Textures &textures = *App_Textures();

    if(!textures.count())
    {
        Con_Message("There are currently no textures defined/loaded.\n");
        return true;
    }

    de::Uri search = composeSearchUri(&argv[1], argc - 1);
    if(!search.scheme().isEmpty() && !textures.knownScheme(search.scheme()))
    {
        Con_Printf("Unknown scheme '%s'.\n", search.schemeCStr());
        return false;
    }

    de::printTextures(search);
    return true;
}

D_CMD(InspectTexture)
{
    DENG2_UNUSED(src);

    de::Textures &textures = *App_Textures();

    if(!textures.count())
    {
        Con_Message("There are currently no textures defined/loaded.\n");
        return true;
    }

    de::Uri search = composeSearchUri(&argv[1], argc - 1, false /*don't match schemes*/);
    if(!search.scheme().isEmpty() && !textures.knownScheme(search.scheme()))
    {
        Con_Printf("Unknown scheme '%s'.\n", search.schemeCStr());
        return false;
    }

    if(de::TextureMetaFile *metafile = textures.find(search))
    if(de::Texture* tex = metafile->texture())
    {
        de::printTextureInfo(*tex);
        return true;
    }

    Con_Printf("Unknown texture \"%s\".\n", search.asText().toUtf8().constData());
    return false;
}

#if _DEBUG
D_CMD(PrintTextureStats)
{
    DENG2_UNUSED(src); DENG2_UNUSED(argc); DENG2_UNUSED(argv);

    de::Textures &textures = *App_Textures();

    if(!textures.count())
    {
        Con_Message("There are currently no textures defined/loaded.\n");
        return true;
    }

    Con_FPrintf(CPF_YELLOW, "Texture Statistics:\n");
    de::Textures::Schemes const &schemes = textures.allSchemes();
    DENG2_FOR_EACH_CONST(de::Textures::Schemes, i, schemes)
    {
        de::Textures::Scheme &scheme = **i;
        de::Textures::Scheme::Index const &index = scheme.index();

        uint const count = index.count();
        Con_Printf("Scheme: %s (%u %s)\n", scheme.name().toUtf8().constData(), count, count == 1? "texture" : "textures");
        index.debugPrintHashDistribution();
        index.debugPrint();
    }
    return true;
}
#endif
