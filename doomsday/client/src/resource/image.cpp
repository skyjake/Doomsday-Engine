/** @file image.cpp  Image objects and related routines.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <cstring>

#include "de_platform.h"
#include "de_console.h"
#include "de_filesys.h"
#include "m_misc.h"
#ifdef __CLIENT__
#  include "resource/colorpalettes.h"
#  include "resource/compositetexture.h"
#  include "resource/patch.h"
#  include "resource/pcx.h"
#  include "resource/tga.h"

#  include "gl/gl_tex.h"

#  include "render/rend_main.h" // misc global vars awaiting new home
#endif
#include <de/Log>
#ifdef __CLIENT__
#  include <de/NativePath>
#  include <QByteArray>
#  include <QImage>
#endif

#include "resource/image.h"

#ifndef DENG2_QT_4_7_OR_NEWER // older than 4.7?
#  define constBits bits
#endif

using namespace de;
using namespace res;

#ifdef __CLIENT__

struct GraphicFileType
{
    /// Symbolic name of the resource type.
    String name;

    /// Known file extension.
    String ext;

    bool (*interpretFunc)(de::FileHandle &hndl, String filePath, image_t &img);

    char const *(*getLastErrorFunc)(); ///< Can be NULL.
};

static bool interpretPcx(de::FileHandle &hndl, String /*filePath*/, image_t &img)
{
    Image_Init(&img);
    img.pixels = PCX_Load(reinterpret_cast<filehandle_s *>(&hndl),
                          &img.size.width, &img.size.height, &img.pixelSize);
    return (0 != img.pixels);
}

static bool interpretJpg(de::FileHandle &hndl, String /*filePath*/, image_t &img)
{
    return Image_LoadFromFileWithFormat(&img, "JPG", reinterpret_cast<filehandle_s *>(&hndl));
}

static bool interpretPng(de::FileHandle &hndl, String /*filePath*/, image_t &img)
{
    /*
    Image_Init(&img);
    img.pixels = PNG_Load(reinterpret_cast<filehandle_s *>(&hndl),
                          &img.size.width, &img.size.height, &img.pixelSize);
    return (0 != img.pixels);
    */
    return Image_LoadFromFileWithFormat(&img, "PNG", reinterpret_cast<filehandle_s *>(&hndl));
}

static bool interpretTga(de::FileHandle &hndl, String /*filePath*/, image_t &img)
{
    Image_Init(&img);
    img.pixels = TGA_Load(reinterpret_cast<filehandle_s *>(&hndl),
                          &img.size.width, &img.size.height, &img.pixelSize);
    return (0 != img.pixels);
}

// Graphic resource types.
static GraphicFileType const graphicTypes[] = {
    { "PNG",    "png",      interpretPng, 0 },
    { "JPG",    "jpg",      interpretJpg, 0 }, // TODO: add alternate "jpeg" extension
    { "TGA",    "tga",      interpretTga, TGA_LastError },
    { "PCX",    "pcx",      interpretPcx, PCX_LastError },
    { "",       "",         0,            0 } // Terminate.
};

static GraphicFileType const *guessGraphicFileTypeFromFileName(String fileName)
{
    // The path must have an extension for this.
    String ext = fileName.fileNameExtension();
    if(!ext.isEmpty())
    {
        for(int i = 0; !graphicTypes[i].ext.isEmpty(); ++i)
        {
            GraphicFileType const &type = graphicTypes[i];
            if(!ext.compareWithoutCase(type.ext))
            {
                return &type;
            }
        }
    }
    return 0; // Unknown.
}

static void interpretGraphic(de::FileHandle &hndl, String filePath, image_t &img)
{
    // Firstly try the interpreter for the guessed resource types.
    GraphicFileType const *rtypeGuess = guessGraphicFileTypeFromFileName(filePath);
    if(rtypeGuess)
    {
        rtypeGuess->interpretFunc(hndl, filePath, img);
    }

    // If not yet interpreted - try each recognisable format in order.
    if(!img.pixels)
    {
        // Try each recognisable format instead.
        /// @todo Order here should be determined by the resource locator.
        for(int i = 0; !graphicTypes[i].name.isEmpty(); ++i)
        {
            GraphicFileType const *graphicType = &graphicTypes[i];

            // Already tried this?
            if(graphicType == rtypeGuess) continue;

            graphicTypes[i].interpretFunc(hndl, filePath, img);
            if(img.pixels) break;
        }
    }
}

/// @return  @c true if the file name in @a path ends with the "color key" suffix.
static inline bool isColorKeyed(String path)
{
    return path.fileNameWithoutExtension().endsWith("-ck", Qt::CaseInsensitive);
}

#endif // __CLIENT__

void Image_Init(image_t *img)
{
    DENG2_ASSERT(img);
    img->size.width = 0;
    img->size.height = 0;
    img->pixelSize = 0;
    img->flags = 0;
    img->paletteId = 0;
    img->pixels = 0;
}

void Image_Destroy(image_t *img)
{
    DENG2_ASSERT(img);
    if(!img->pixels) return;

    M_Free(img->pixels);
    img->pixels = 0;
}

de::Vector2i Image_Dimensions(image_t const *img)
{
    DENG2_ASSERT(img);
    return Vector2i(img->size.width, img->size.height);
}

String Image_Description(image_t const *img)
{
    DENG2_ASSERT(img);
    Vector2i dimensions(img->size.width, img->size.height);

    return String("Dimensions:%1 Flags:%2 %3:%4")
               .arg(dimensions.asText())
               .arg(img->flags)
               .arg(0 != img->paletteId? "ColorPalette" : "PixelSize")
               .arg(0 != img->paletteId? img->paletteId : img->pixelSize);
}

void Image_ConvertToLuminance(image_t *img, boolean retainAlpha)
{
    DENG_ASSERT(img);
    LOG_AS("GL_ConvertToLuminance");

    uint8_t *alphaChannel = 0, *ptr = 0;

    // Is this suitable?
    if(0 != img->paletteId || (img->pixelSize < 3 && (img->flags & IMGF_IS_MASKED)))
    {
#if _DEBUG
        LOG_WARNING("Attempt to convert paletted/masked image. I don't know this format!");
#endif
        return;
    }

    long numPels = img->size.width * img->size.height;

    // Do we need to relocate the alpha data?
    if(retainAlpha && img->pixelSize == 4)
    {
        // Yes. Take a copy.
        alphaChannel = reinterpret_cast<uint8_t *>(M_Malloc(numPels));

        ptr = img->pixels;
        for(long p = 0; p < numPels; ++p, ptr += img->pixelSize)
        {
            alphaChannel[p] = ptr[3];
        }
    }

    // Average the RGB colors.
    ptr = img->pixels;
    for(long p = 0; p < numPels; ++p, ptr += img->pixelSize)
    {
        int min = MIN_OF(ptr[0], MIN_OF(ptr[1], ptr[2]));
        int max = MAX_OF(ptr[0], MAX_OF(ptr[1], ptr[2]));
        img->pixels[p] = (min == max? min : (min + max) / 2);
    }

    // Do we need to relocate the alpha data?
    if(alphaChannel)
    {
        std::memcpy(img->pixels + numPels, alphaChannel, numPels);
        img->pixelSize = 2;
        M_Free(alphaChannel);
        return;
    }

    img->pixelSize = 1;
}

void Image_ConvertToAlpha(image_t *img, boolean makeWhite)
{
    DENG2_ASSERT(img);

    Image_ConvertToLuminance(img, true);

    long total = img->size.width * img->size.height;
    for(long p = 0; p < total; ++p)
    {
        img->pixels[total + p] = img->pixels[p];
        if(makeWhite) img->pixels[p] = 255;
    }
    img->pixelSize = 2;
}

boolean Image_HasAlpha(image_t const *img)
{
    DENG2_ASSERT(img);
    LOG_AS("Image_HasAlpha");

    if(0 != img->paletteId || (img->flags & IMGF_IS_MASKED))
    {
#if _DEBUG
        LOG_WARNING("Attempt to determine alpha for paletted/masked image. I don't know this format!");
#endif
        return false;
    }

    if(img->pixelSize == 3)
    {
        return false;
    }

    if(img->pixelSize == 4)
    {
        long const numpels = img->size.width * img->size.height;
        uint8_t const *in = img->pixels;
        for(long i = 0; i < numpels; ++i, in += 4)
        {
            if(in[3] < 255)
            {
                return true;
            }
        }
    }
    return false;
}

uint8_t *Image_LoadFromFile(image_t *img, filehandle_s *_file)
{
#ifdef __CLIENT__
    DENG2_ASSERT(img && _file);
    de::FileHandle &file = reinterpret_cast<de::FileHandle &>(*_file);
    LOG_AS("Image_LoadFromFile");

    String filePath = file.file().composePath();

    Image_Init(img);
    interpretGraphic(file, filePath, *img);

    // Still not interpreted?
    if(!img->pixels)
    {
        LOG_DEBUG("\"%s\" unrecognized, trying fallback loader...")
            << NativePath(filePath).pretty();
        return NULL; // Not a recognised format. It may still be loadable, however.
    }

    // How about some color-keying?
    if(isColorKeyed(filePath))
    {
        uint8_t *out = ApplyColorKeying(img->pixels, img->size.width, img->size.height, img->pixelSize);
        if(out != img->pixels)
        {
            // Had to allocate a larger buffer, free the old and attach the new.
            M_Free(img->pixels);
            img->pixels = out;
        }

        // Color keying is done; now we have 4 bytes per pixel.
        img->pixelSize = 4;
    }

    // Any alpha pixels?
    if(Image_HasAlpha(img))
    {
        img->flags |= IMGF_IS_MASKED;
    }

    LOG_VERBOSE("\"%s\" (%ix%i)")
        << NativePath(filePath).pretty() << img->size.width << img->size.height;

    return img->pixels;
#else
    // Server does not load image files.
    DENG2_UNUSED2(img, _file);
    return NULL;
#endif
}

boolean Image_LoadFromFileWithFormat(image_t *img, char const *format, filehandle_s *_hndl)
{
#ifdef __CLIENT__
    /// @todo There are too many copies made here. It would be best if image_t
    /// contained an instance of QImage. -jk

    DENG2_ASSERT(img);
    DENG2_ASSERT(_hndl);
    de::FileHandle &hndl = *reinterpret_cast<de::FileHandle *>(_hndl);

    // It is assumed that file's position stays the same (could be trying multiple interpreters).
    size_t initPos = hndl.tell();

    Image_Init(img);

    // Load the file contents to a memory buffer.
    QByteArray data;
    data.resize(hndl.length() - initPos);
    hndl.read(reinterpret_cast<uint8_t*>(data.data()), data.size());

    QImage image = QImage::fromData(data, format);
    if(image.isNull())
    {
        // Back to the original file position.
        hndl.seek(initPos, SeekSet);
        return false;
    }

    //LOG_TRACE("Loading \"%s\"...") << NativePath(hndl->file().composePath()).pretty();

    // Convert paletted images to RGB.
    if(image.colorCount())
    {
        image = image.convertToFormat(QImage::Format_ARGB32);
        DENG_ASSERT(!image.colorCount());
        DENG_ASSERT(image.depth() == 32);
    }

    // Swap the red and blue channels for GL.
    image = image.rgbSwapped();

    img->size.width  = image.width();
    img->size.height = image.height();
    img->pixelSize   = image.depth() / 8;

    LOG_TRACE("Image_Load: size %i x %i depth %i alpha %b bytes %i")
        << img->size.width << img->size.height << img->pixelSize
        << image.hasAlphaChannel() << image.byteCount();

    img->pixels = reinterpret_cast<uint8_t *>(M_MemDup(image.constBits(), image.byteCount()));

    // Back to the original file position.
    hndl.seek(initPos, SeekSet);
    return true;
#else
    // Server does not load image files.
    DENG2_UNUSED3(img, format, _hndl);
    return false;
#endif
}

boolean Image_Save(image_t const *img, char const *filePath)
{
#ifdef __CLIENT__
    DENG2_ASSERT(img);

    // Compose the full path.
    String fullPath = String(filePath);
    if(fullPath.isEmpty())
    {
        static int n = 0;
        fullPath = String("image%1x%2-%3").arg(img->size.width).arg(img->size.height).arg(n++, 3);
    }

    if(fullPath.fileNameExtension().isEmpty())
    {
        fullPath += ".png";
    }

    // Swap red and blue channels then save.
    QImage image = QImage(img->pixels, img->size.width, img->size.height, QImage::Format_ARGB32);
    image = image.rgbSwapped();

    return CPP_BOOL(image.save(NativePath(fullPath)));
#else
    // Server does not save images.
    DENG2_UNUSED2(img, filePath);
    return false;
#endif
}

#ifdef __CLIENT__
uint8_t *GL_LoadImage(image_t &image, String nativePath)
{
    try
    {
        // Relative paths are relative to the native working directory.
        String path = (NativePath::workPath() / NativePath(nativePath).expand()).withSeparators('/');

        de::FileHandle &hndl = App_FileSystem().openFile(path, "rb");
        uint8_t *pixels = Image_LoadFromFile(&image, reinterpret_cast<filehandle_s *>(&hndl));

        App_FileSystem().releaseFile(hndl.file());
        delete &hndl;

        return pixels;
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore error.

    return 0; // Not loaded.
}

Source GL_LoadExtImage(image_t &image, char const *_searchPath, gfxmode_t mode)
{
    DENG_ASSERT(_searchPath);

    try
    {
        String foundPath = App_FileSystem().findPath(de::Uri(RC_GRAPHIC, _searchPath),
                                                     RLF_DEFAULT, App_ResourceClass(RC_GRAPHIC));
        // Ensure the found path is absolute.
        foundPath = App_BasePath() / foundPath;

        if(GL_LoadImage(image, foundPath))
        {
            // Force it to grayscale?
            if(mode == LGM_GRAYSCALE_ALPHA || mode == LGM_WHITE_ALPHA)
            {
                Image_ConvertToAlpha(&image, mode == LGM_WHITE_ALPHA);
            }
            else if(mode == LGM_GRAYSCALE)
            {
                Image_ConvertToLuminance(&image, true);
            }

            return External;
        }
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore this error.

    return None;
}

static boolean palettedIsMasked(uint8_t const *pixels, int width, int height)
{
    DENG2_ASSERT(pixels != 0);
    // Jump to the start of the alpha data.
    pixels += width * height;
    for(int i = 0; i < width * height; ++i)
    {
        if(255 != pixels[i])
        {
            return true;
        }
    }
    return false;
}

static Source loadExternalTexture(image_t &image, String encodedSearchPath,
    String optionalSuffix = "")
{
    // First look for a version with an optional suffix.
    try
    {
        String foundPath = App_FileSystem().findPath(de::Uri(encodedSearchPath + optionalSuffix, RC_GRAPHIC),
                                                     RLF_DEFAULT, App_ResourceClass(RC_GRAPHIC));
        // Ensure the found path is absolute.
        foundPath = App_BasePath() / foundPath;

        return GL_LoadImage(image, foundPath)? External : None;
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore this error.

    // Try again without the suffix?
    if(!optionalSuffix.empty())
    {
        try
        {
            String foundPath = App_FileSystem().findPath(de::Uri(encodedSearchPath, RC_GRAPHIC),
                                                         RLF_DEFAULT, App_ResourceClass(RC_GRAPHIC));
            // Ensure the found path is absolute.
            foundPath = App_BasePath() / foundPath;

            return GL_LoadImage(image, foundPath)? External : None;
        }
        catch(FS1::NotFoundError const&)
        {} // Ignore this error.
    }

    return None;
}

/**
 * Draw the component image @a src into the composite @a dst.
 *
 * @param dst               The composite buffer (drawn to).
 * @param dstDimensions     Pixel dimensions of @a dst.
 * @param src               The component image to be composited (read from).
 * @param srcDimensions     Pixel dimensions of @a src.
 * @param origin            Coordinates (topleft) in @a dst to draw @a src.
 *
 * @todo Optimize: Should be redesigned to composite whole rows -ds
 */
static void compositePaletted(dbyte *dst, Vector2i const &dstDimensions,
    IByteArray const &src, Vector2i const &srcDimensions, Vector2i const &origin)
{
    if(dstDimensions.x == 0 && dstDimensions.y == 0) return;
    if(srcDimensions.x == 0 && srcDimensions.y == 0) return;

    int const    srcW = srcDimensions.x;
    int const    srcH = srcDimensions.y;
    size_t const srcPels = srcW * srcH;

    int const    dstW = dstDimensions.x;
    int const    dstH = dstDimensions.y;
    size_t const dstPels = dstW * dstH;

    int dstX, dstY;

    for(int srcY = 0; srcY < srcH; ++srcY)
    for(int srcX = 0; srcX < srcW; ++srcX)
    {
        dstX = origin.x + srcX;
        dstY = origin.y + srcY;
        if(dstX < 0 || dstX >= dstW) continue;
        if(dstY < 0 || dstY >= dstH) continue;

        dbyte srcAlpha;
        src.get(srcY * srcW + srcX + srcPels, &srcAlpha, 1);
        if(srcAlpha)
        {
            src.get(srcY * srcW + srcX, &dst[dstY * dstW + dstX], 1);
            dst[dstY * dstW + dstX + dstPels] = srcAlpha;
        }
    }
}

static Block loadAndTranslatePatch(IByteArray const &data, int tclass = 0, int tmap = 0)
{
    if(dbyte const *xlatTable = R_TranslationTable(tclass, tmap))
    {
        return Patch::load(data, ByteRefArray(xlatTable, 256), Patch::ClipToLogicalDimensions);
    }
    else
    {
        return Patch::load(data, Patch::ClipToLogicalDimensions);
    }
}

static Source loadPatch(image_t &image, de::FileHandle &hndl, int tclass = 0,
    int tmap = 0, int border = 0)
{
    LOG_AS("image_t::loadPatchLump");

    if(Image_LoadFromFile(&image, reinterpret_cast<filehandle_s *>(&hndl)))
    {
        return External;
    }

    de::File1 &file = hndl.file();
    ByteRefArray fileData = ByteRefArray(file.cache(), file.size());

    // A DOOM patch?
    if(Patch::recognize(fileData))
    {
        try
        {
            Block patchImg = loadAndTranslatePatch(fileData, tclass, tmap);
            Patch::Metadata info = Patch::loadMetadata(fileData);

            Image_Init(&image);
            image.size.width  = info.logicalDimensions.x + border*2;
            image.size.height = info.logicalDimensions.y + border*2;
            image.pixelSize   = 1;
            image.paletteId   = App_ResourceSystem().defaultColorPalette();

            image.pixels = (uint8_t*) M_Calloc(2 * image.size.width * image.size.height);

            compositePaletted(image.pixels, Vector2i(image.size.width, image.size.height),
                              patchImg, info.logicalDimensions, Vector2i(border, border));

            if(palettedIsMasked(image.pixels, image.size.width, image.size.height))
            {
                image.flags |= IMGF_IS_MASKED;
            }

            return Original;
        }
        catch(IByteArray::OffsetError const &)
        {
            LOG_WARNING("File \"%s:%s\" does not appear to be a valid Patch.")
                << NativePath(file.container().composePath()).pretty()
                << NativePath(file.composePath()).pretty();
        }
    }

    file.unlock();
    return None;
}

static Source loadPatchComposite(image_t &image, Texture const &tex,
    bool maskZero = false, bool useZeroOriginIfOneComponent = false)
{
    LOG_AS("image_t::loadPatchComposite");

    Image_Init(&image);
    image.pixelSize = 1;
    image.size.width  = tex.width();
    image.size.height = tex.height();
    image.paletteId   = App_ResourceSystem().defaultColorPalette();

    image.pixels = (uint8_t *) M_Calloc(2 * image.size.width * image.size.height);

    CompositeTexture const &texDef = *reinterpret_cast<CompositeTexture *>(tex.userDataPointer());
    DENG2_FOR_EACH_CONST(CompositeTexture::Components, i, texDef.components())
    {
        de::File1 &file = App_FileSystem().nameIndex().lump(i->lumpNum());
        ByteRefArray fileData = ByteRefArray(file.cache(), file.size());

        // A DOOM patch?
        if(Patch::recognize(fileData))
        {
            try
            {
                Patch::Flags loadFlags;
                if(maskZero) loadFlags |= Patch::MaskZero;
                Block patchImg = Patch::load(fileData, loadFlags);
                Patch::Metadata info = Patch::loadMetadata(fileData);

                Vector2i origin = i->origin();
                if(useZeroOriginIfOneComponent && texDef.componentCount() == 1)
                    origin = Vector2i(0, 0);

                // Draw the patch in the buffer.
                compositePaletted(image.pixels, Vector2i(image.size.width, image.size.height),
                                  patchImg, info.dimensions, origin);
            }
            catch(IByteArray::OffsetError const &)
            {} // Ignore this error.
        }

        file.unlock();
    }

    if(maskZero || palettedIsMasked(image.pixels, image.size.width, image.size.height))
    {
        image.flags |= IMGF_IS_MASKED;
    }

    // For debug:
    // GL_DumpImage(&image, Str_Text(GL_ComposeCacheNameForTexture(tex)));

    return Original;
}

static Source loadFlat(image_t &image, de::FileHandle &hndl)
{
    if(Image_LoadFromFile(&image, reinterpret_cast<filehandle_s *>(&hndl)))
    {
        return External;
    }

    // A DOOM flat.
#define FLAT_WIDTH          64
#define FLAT_HEIGHT         64

    Image_Init(&image);

    /// @todo not all flats are 64x64!
    image.size.width  = FLAT_WIDTH;
    image.size.height = FLAT_HEIGHT;
    image.pixelSize = 1;
    image.paletteId = App_ResourceSystem().defaultColorPalette();

    de::File1 &file   = hndl.file();
    size_t fileLength = hndl.length();

    size_t bufSize = MAX_OF(fileLength, (size_t) image.size.width * image.size.height);
    image.pixels = (uint8_t *) M_Malloc(bufSize);

    if(fileLength < bufSize)
    {
        std::memset(image.pixels, 0, bufSize);
    }

    // Load the raw image data.
    file.read(image.pixels, 0, fileLength);
    return Original;

#undef FLAT_HEIGHT
#undef FLAT_WIDTH
}

static Source loadDetail(image_t &image, de::FileHandle &hndl)
{
    if(Image_LoadFromFile(&image, reinterpret_cast<filehandle_s *>(&hndl)))
    {
        return Original;
    }

    // It must be an old-fashioned "raw" image.
    Image_Init(&image);

    // How big is it?
    de::File1 &file = hndl.file();
    size_t fileLength = hndl.length();
    switch(fileLength)
    {
    case 256 * 256: image.size.width = image.size.height = 256; break;
    case 128 * 128: image.size.width = image.size.height = 128; break;
    case  64 *  64: image.size.width = image.size.height =  64; break;
    default:
        throw Error("image_t::loadDetail", "Must be 256x256, 128x128 or 64x64.");
    }

    image.pixelSize = 1;
    size_t bufSize = (size_t) image.size.width * image.size.height;
    image.pixels = (uint8_t *) M_Malloc(bufSize);

    if(fileLength < bufSize)
    {
        std::memset(image.pixels, 0, bufSize);
    }

    // Load the raw image data.
    file.read(image.pixels, fileLength);
    return Original;
}

Source GL_LoadSourceImage(image_t &image, Texture const &tex,
    TextureVariantSpec const &spec)
{
    Source source = None;
    variantspecification_t const &vspec = spec.variant;
    if(!tex.manifest().schemeName().compareWithoutCase("Textures"))
    {
        // Attempt to load an external replacement for this composite texture?
        if(!noHighResTex && (loadExtAlways || highResWithPWAD || !tex.isFlagged(Texture::Custom)))
        {
            // First try the textures scheme.
            de::Uri uri = tex.manifest().composeUri();
            source = loadExternalTexture(image, uri.compose(), "-ck");
        }

        if(source == None)
        {
            if(TC_SKYSPHERE_DIFFUSE != vspec.context)
            {
                source = loadPatchComposite(image, tex);
            }
            else
            {
                bool const zeroMask = (vspec.flags & TSF_ZEROMASK) != 0;
                bool const useZeroOriginIfOneComponent = true;
                source = loadPatchComposite(image, tex, zeroMask, useZeroOriginIfOneComponent);
            }
        }
    }
    else if(!tex.manifest().schemeName().compareWithoutCase("Flats"))
    {
        // Attempt to load an external replacement for this flat?
        if(!noHighResTex && (loadExtAlways || highResWithPWAD || !tex.isFlagged(Texture::Custom)))
        {
            // First try the flats scheme.
            de::Uri uri = tex.manifest().composeUri();
            source = loadExternalTexture(image, uri.compose(), "-ck");

            if(source == None)
            {
                // How about the old-fashioned "flat-name" in the textures scheme?
                source = loadExternalTexture(image, "Textures:flat-" + uri.path().toStringRef(), "-ck");
            }
        }

        if(source == None)
        {
            if(tex.manifest().hasResourceUri())
            {
                de::Uri resourceUri = tex.manifest().resourceUri();
                if(!resourceUri.scheme().compareWithoutCase("LumpIndex"))
                {
                    try
                    {
                        lumpnum_t lumpNum = resourceUri.path().toString().toInt();
                        de::File1 &lump = App_FileSystem().nameIndex().lump(lumpNum);
                        de::FileHandle &hndl = App_FileSystem().openLump(lump);

                        source = loadFlat(image, hndl);

                        App_FileSystem().releaseFile(hndl.file());
                        delete &hndl;
                    }
                    catch(LumpIndex::NotFoundError const&)
                    {} // Ignore this error.
                }
            }
        }
    }
    else if(!tex.manifest().schemeName().compareWithoutCase("Patches"))
    {
        int tclass = 0, tmap = 0;
        if(vspec.flags & TSF_HAS_COLORPALETTE_XLAT)
        {
            tclass = vspec.tClass;
            tmap   = vspec.tMap;
        }

        // Attempt to load an external replacement for this patch?
        if(!noHighResTex && (loadExtAlways || highResWithPWAD || !tex.isFlagged(Texture::Custom)))
        {
            de::Uri uri = tex.manifest().composeUri();
            source = loadExternalTexture(image, uri.compose(), "-ck");
        }

        if(source == None)
        {
            if(tex.manifest().hasResourceUri())
            {
                de::Uri resourceUri = tex.manifest().resourceUri();
                if(!resourceUri.scheme().compareWithoutCase("LumpIndex"))
                {
                    try
                    {
                        lumpnum_t lumpNum = resourceUri.path().toString().toInt();
                        de::File1 &lump = App_FileSystem().nameIndex().lump(lumpNum);
                        de::FileHandle &hndl = App_FileSystem().openLump(lump);

                        source = loadPatch(image, hndl, tclass, tmap, vspec.border);

                        App_FileSystem().releaseFile(hndl.file());
                        delete &hndl;
                    }
                    catch(LumpIndex::NotFoundError const&)
                    {} // Ignore this error.
                }
            }
        }
    }
    else if(!tex.manifest().schemeName().compareWithoutCase("Sprites"))
    {
        int tclass = 0, tmap = 0;
        if(vspec.flags & TSF_HAS_COLORPALETTE_XLAT)
        {
            tclass = vspec.tClass;
            tmap   = vspec.tMap;
        }

        // Attempt to load an external replacement for this sprite?
        if(!noHighResPatches)
        {
            de::Uri uri = tex.manifest().composeUri();

            // Prefer psprite or translated versions if available.
            if(TC_PSPRITE_DIFFUSE == vspec.context)
            {
                source = loadExternalTexture(image, "Patches:" + uri.path() + "-hud", "-ck");
            }
            else if(tclass || tmap)
            {
                source = loadExternalTexture(image, "Patches:" + uri.path() + String("-table%1%2").arg(tclass).arg(tmap), "-ck");
            }

            if(!source)
            {
                source = loadExternalTexture(image, "Patches:" + uri.path(), "-ck");
            }
        }

        if(source == None)
        {
            if(tex.manifest().hasResourceUri())
            {
                de::Uri resourceUri = tex.manifest().resourceUri();
                if(!resourceUri.scheme().compareWithoutCase("LumpIndex"))
                {
                    try
                    {
                        lumpnum_t lumpNum = resourceUri.path().toString().toInt();
                        de::File1 &lump = App_FileSystem().nameIndex().lump(lumpNum);
                        de::FileHandle &hndl = App_FileSystem().openLump(lump);

                        source = loadPatch(image, hndl, tclass, tmap, vspec.border);

                        App_FileSystem().releaseFile(hndl.file());
                        delete &hndl;
                    }
                    catch(LumpIndex::NotFoundError const&)
                    {} // Ignore this error.
                }
            }
        }
    }
    else if(!tex.manifest().schemeName().compareWithoutCase("Details"))
    {
        if(tex.manifest().hasResourceUri())
        {
            de::Uri resourceUri = tex.manifest().resourceUri();
            if(resourceUri.scheme().compareWithoutCase("Lumps"))
            {
                source = loadExternalTexture(image, resourceUri.compose());
            }
            else
            {
                lumpnum_t lumpNum = App_FileSystem().lumpNumForName(resourceUri.path());
                try
                {
                    de::File1 &lump = App_FileSystem().nameIndex().lump(lumpNum);
                    de::FileHandle &hndl = App_FileSystem().openLump(lump);

                    source = loadDetail(image, hndl);

                    App_FileSystem().releaseFile(hndl.file());
                    delete &hndl;
                }
                catch(LumpIndex::NotFoundError const&)
                {} // Ignore this error.
            }
        }
    }
    else
    {
        if(tex.manifest().hasResourceUri())
        {
            de::Uri resourceUri = tex.manifest().resourceUri();
            source = loadExternalTexture(image, resourceUri.compose());
        }
    }
    return source;
}

#endif // __CLIENT__
