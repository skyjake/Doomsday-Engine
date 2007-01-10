/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/*
 * texture.cpp: Texture Management
 */

// HEADER FILES ------------------------------------------------------------

#include "drd3d.h"

// MACROS ------------------------------------------------------------------

#define IDX_TO_NAME(i)      ((i) + 1)
#define NAME_TO_IDX(n)      ((n) - 1)

#define STSF_MIN_FILTER     0x01    // Includes mipfilter.
#define STSF_MAG_FILTER     0x02
#define STSF_ADDRESS_U      0x04
#define STSF_ADDRESS_V      0x08
#define STSF_ANISOTROPY     0x10
#define STSF_ALL            0x1f

#define CUR_STAGE           ( unitToStage[currentUnit] )

// TYPES -------------------------------------------------------------------

typedef unsigned short  ushort;

typedef struct tex_s {
    IDirect3DTexture9 *ptr;
    int     width;
    int     height;
    D3DTEXTUREFILTERTYPE minFilter, mipFilter, magFilter;
    D3DTEXTUREADDRESS addressModeU, addressModeV;
} tex_t;

#pragma pack(1)
typedef struct mybmp_header_s {
    char    identifier[2];
    uint    fileSize;
    uint    reserved;
    uint    bitmapDataOffset;
    uint    bitmapHeaderSize;
    uint    width;
    int     height;
    short   planes;
    short   bitsPerPixel;
    uint    compression;
    uint    bitmapDataSize;     // Rounded to dword.
    int     hResolution;
    int     vResolution;
    uint    colors;
    uint    importantColors;
} mybmp_header_t;
#pragma pack()

#pragma pack(1)     // Confirm that the structures are formed correctly.
typedef struct      // Targa image descriptor byte; a bit field
{
    byte    attributeBits   : 4;    // Attribute bits associated with each pixel.
    byte    reserved        : 1;    // A reserved bit; must be 0.
    byte    screenOrigin    : 1;    // Location of screen origin; must be 0.
    byte    dataInterleave  : 2;    // TGA_INTERLEAVE_*
} TARGA_IMAGE_DESCRIPTOR;

typedef struct
{
    byte    idFieldSize;            // Identification field size in bytes.
    byte    colorMapType;           // Type of the color map.
    byte    imageType;              // Image type code.
    // Color map specification.
    ushort  colorMapOrigin;         // Index of first color map entry.
    ushort  colorMapLength;         // Number of color map entries.
    byte    colorMapEntrySize;      // Number of bits in a color map entry (16/24/32).
    // Image specification.
    ushort  xOrigin;                // X coordinate of lower left corner.
    ushort  yOrigin;                // Y coordinate of lower left corner.
    ushort  imageWidth;             // Width of the image in pixels.
    ushort  imageHeight;            // Height of the image in pixels.
    byte    imagePixelSize;         // Number of bits in a pixel (16/24/32).
    TARGA_IMAGE_DESCRIPTOR imageDescriptor; // A bit field.
} TARGA_HEADER;
#pragma pack()      // Back to the default value.

typedef struct palentry_s { byte color[4]; } palentry_t;

// FUNCTION PROTOTYPES -----------------------------------------------------

int Bind(int stage, DGLuint texture);
void Unbind(int stage);
void SetTexStates(int stage, tex_t *tex, int flags);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

tex_t       *texData;
int         numTexData;
DGLuint     boundTexName[MAX_TEX_STAGES];
int         currentUnit;
int         unitToStage[MAX_TEX_STAGES];
float       grayMipmapFactor = 1;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean      useAnisotropic = false;
static palentry_t   texturePalette[256];
static boolean      texActive;

// CODE --------------------------------------------------------------------

/**
 * Works within the given data, reducing the size of the picture to half
 * its original. Width and height must be powers of two.
 */
void DownMip8(byte *in, byte *fadedOut, int width, int height, float fade)
{
    byte       *out = in;
    int         x, y, outW = width >> 1, outH = height >> 1;
    float       invFade;

    if(fade > 1)
        fade = 1;
    invFade = 1 - fade;

    if(width == 1 && height == 1)
    {
        // Nothing can be done.
        return;
    }

    if(!outW || !outH) // Limited, 1x2|2x1 -> 1x1 reduction?
    {
        int         outDim = width > 1? outW : outH;

        for(x = 0; x < outDim; x++, in += 2)
        {
            *out = (in[0] + in[1]) >> 1;
            *fadedOut++ = (byte) (*out * invFade + 0x80 * fade);
            out++;
        }
    }
    else // Unconstrained, 2x2 -> 1x1 reduction?
    {
        for(y = 0; y < outH; y++, in += width)
            for(x = 0; x < outW; x++, in += 2)
            {
                *out = (in[0] + in[1] + in[width] + in[width + 1]) >> 2;
                *fadedOut++ = (byte) (*out * invFade + 0x80 * fade);
                out++;
            }
    }
}

void LoadLevel(tex_t *tex, int level, int width, int height, byte *image)
{
    IDirect3DSurface9 *surface = NULL;
    RECT rect;

    rect.left = 0;
    rect.right = width;
    rect.top = 0;
    rect.bottom = height;

    tex->ptr->GetSurfaceLevel(level, &surface);
    D3DXLoadSurfaceFromMemory(
        surface,    // Destination surface
        NULL,       // Palette
        NULL,       // Destination rect
        image,      // Source data
        D3DFMT_L8,  // Source format
        width,      // Source pitch
        NULL,       // Source palette
        &rect,      // Source rectangle
        D3DX_FILTER_NONE, // Filter
        0);         // No color key
    surface->Release();
}

/**
 * NOTE: Copied from drOpenGL.
 */
void GenerateGrayMipmaps(tex_t *tex, int format, int width, int height,
                         void *data)
{
    byte       *image, *in, *out, *faded;
    int         i, numLevels, w, h, size = width * height, res;
    float       invFactor = 1 - grayMipmapFactor;

    // Buffer used for the faded texture.
    faded = (byte*) malloc(size / 4);
    image = (byte*) malloc(size);

    // Initial fading.
    if(format == DGL_LUMINANCE)
    {
        for(i = 0, in = (byte*) data, out = image; i < size; ++i)
        {
            // Result is clamped to [0,255].
            res = (int) (*in++ * grayMipmapFactor + 0x80 * invFactor);
            if(res < 0)
                res = 0;
            if(res > 255)
                res = 255;

            *out++ = res;
        }
    }
    else if(format == DGL_RGB)
    {
        for(i = 0, in = (byte*) data, out = image; i < size; ++i, in += 3)
        {
            // Result is clamped to [0,255].
            res = (int) (*in * grayMipmapFactor + 0x80 * invFactor);
            if(res < 0)
                res = 0;
            if(res > 255)
                res = 255;

            *out++ = res;
        }
    }

    // How many levels will there be?
    for(numLevels = 0, w = width, h = height; w > 1 || h > 1;
        w >>= 1, h >>= 1, ++numLevels);

    // Create the Direct3D Texture object.
    if(FAILED(hr = D3DXCreateTexture(dev, width, height, 0, 0, D3DFMT_L8,
        D3DPOOL_MANAGED, &tex->ptr)))
    {
        DXError("D3DXCreateTexture");
        tex->ptr = NULL;
#ifdef _DEBUG
        Con_Error("GenerateGrayMipmaps: Failed to create texture %i x %i.\n",
            width, height);
#endif
        free(faded);
        free(image);
        return;
    }

    // Upload the first level right away.
    LoadLevel(tex, 0, width, height, image);

    // Generate all mipmaps levels.
    for(i = 0, w = width, h = height; i < numLevels; ++i)
    {
        DownMip8(image, faded, w, h, (i * 1.75f)/numLevels);

        // Go down one level.
        if(w > 1)
            w >>= 1;
        if(h > 1)
            h >>= 1;

        LoadLevel(tex, i + 1, w, h, faded);
    }

    // Do we need to free the temp buffer?
    free(faded);
    free(image);
}

void InitTextures(void)
{
    useAnisotropic = ArgExists("-anifilter");
    texActive = true;

    // Allocate the texture information buffer.
    numTexData = 32;
    texData = (tex_t*) calloc(numTexData * sizeof(*texData), 1);
    memset(boundTexName, 0, sizeof(boundTexName));

    StageIdentity();
    ActiveTexture(0);
}

void ShutdownTextures(void)
{
    for(int i = 0; i < numTexData; ++i)
    {
        if(!texData[i].ptr)
            continue; // Not used.

        // Delete this texture.
        DGLuint name = IDX_TO_NAME(i);
        DG_DeleteTextures(1, &name);
    }
    free(texData);
    texData = NULL;
    numTexData = 0;
    memset(boundTexName, 0, sizeof(boundTexName));
}

/**
 * Make logical texture unit indices match texture stage indices.
 */
void StageIdentity(void)
{
    for(int i = 0; i < MAX_TEX_STAGES; ++i)
        if(unitToStage[i] != i)
            SetUnitStage(i, i);
}

void SetUnitStage(int logicalUnit, int actualStage)
{
    int         tex = boundTexName[unitToStage[logicalUnit]];

    if(tex)
    {
        // Move old texture from here...
        Unbind(unitToStage[logicalUnit]);
    }
    unitToStage[logicalUnit] = actualStage;
    if(tex)
    {
        // ...to here.
        Bind(actualStage, tex);
    }
}

void ActiveTexture(int index)
{
    currentUnit = index;
}

void TextureOperatingMode(int isActive)
{
    texActive = isActive;
    if(!isActive)
    {
        dev->SetTexture(CUR_STAGE, NULL);
    }
    else
    {
        DG_Bind(boundTexName[CUR_STAGE]);
    }
}

tex_t *GetBoundTexture(int stage)
{
    if(!boundTexName[stage])
        return NULL;

    return texData + NAME_TO_IDX( boundTexName[stage] );
}

int Bind(int stage, DGLuint texture)
{
    int         idx = NAME_TO_IDX(texture);
    int         previous = boundTexName[stage];

    if(!texture)
    {
        Unbind(stage);
        return previous;
    }

    if(!dev || idx < 0 || idx >= numTexData)
        return previous;

    boundTexName[stage] = texture;
    tex_t *tex = GetBoundTexture(stage);
    dev->SetTexture(stage, tex->ptr);
    SetTexStates(stage, tex, STSF_ALL);

    return previous;
}

void Unbind(int stage)
{
    boundTexName[stage] = 0;
    if(dev)
        dev->SetTexture(stage, NULL);
}

/**
 * Sets the properties of the texture to the default settings.
 */
void InitNewTexture(tex_t *tex)
{
    tex->width = tex->height = 0;
    tex->minFilter = D3DTEXF_LINEAR;
    tex->mipFilter = D3DTEXF_POINT;
    tex->magFilter = D3DTEXF_LINEAR;
    tex->addressModeU = D3DTADDRESS_WRAP;
    tex->addressModeV = D3DTADDRESS_WRAP;
}

/**
 * Creates a new texture and binds it.
 */
DGLuint DG_NewTexture(void)
{
    int         i;

    // Try to find an unused texdata.
    for(i = 0; i < numTexData; ++i)
        if(!texData[i].ptr)
        {
            InitNewTexture(texData + i);
            DG_Bind(IDX_TO_NAME(i));

            return boundTexName[CUR_STAGE];
        }

    // Allocate memory texdata.
    i = numTexData;
    numTexData *= 2;
    texData = (tex_t*) realloc(texData, 2*i*sizeof(*texData));
    memset(texData + i, 0, i*sizeof(*texData));
    InitNewTexture(texData + i);
    DG_Bind(IDX_TO_NAME(i));

    return boundTexName[CUR_STAGE];
}

void SetTexStates(int stage, tex_t *tex, int flags)
{
    if(flags & STSF_MIN_FILTER)
    {
        SetSS(stage, D3DSAMP_MINFILTER, tex->minFilter);
        SetSS(stage, D3DSAMP_MIPFILTER, tex->mipFilter);
    }
    if(flags & STSF_MAG_FILTER)
    {
        SetSS(stage, D3DSAMP_MAGFILTER, tex->magFilter);
    }
    if(flags & STSF_ADDRESS_U)
    {
        SetSS(stage, D3DSAMP_ADDRESSU, tex->addressModeU);
    }
    if(flags & STSF_ADDRESS_V)
    {
        SetSS(stage, D3DSAMP_ADDRESSV, tex->addressModeV);
    }
    if(useAnisotropic && flags & STSF_ANISOTROPY)
    {
        SetSS(stage, D3DSAMP_MAXANISOTROPY, maxAniso);
    }
}

/**
 * The texture data is put into a Targa image structure, so creating
 * the texture object is easy using D3DXCreateTextureFromFileInMemory.
 */
int DG_TexImage(int format, int width, int height, int genMips, void *data)
{
    tex_t      *tex = GetBoundTexture(CUR_STAGE);
    byte       *buffer;
    TARGA_HEADER *hdr;
    byte       *ptr, *in, *alphaIn;
    int         i, x, y;
    boolean     hiBits = (wantedTexDepth != 16);

#if _DEBUG
if(!width || !height)
    Con_Error("DG_TexImage: No width or height!\n");
#endif

    if(!tex)
        return DGL_ERROR; // No texture has been bound!

    // If there is a previous texture, release it.
    if(tex->ptr)
    {
        dev->SetTexture(CUR_STAGE, NULL);
        tex->ptr->Release();
        tex->ptr = NULL;
    }

    // Update texture width and height.
    tex->width = width;
    tex->height = height;

    if(genMips == DGL_GRAY_MIPMAP)
    {
        GenerateGrayMipmaps(tex, format, width, height, data);
    }
    else
    {
        // Allocate a large enough buffer.
        buffer = (byte*) calloc(sizeof(TARGA_HEADER) + 256*4 /*Palette*/
            + width*height*4 /*Pixels*/, 1);
        hdr = (TARGA_HEADER*) buffer;

        // Fill in the Targa data.
        hdr->imageWidth = width;
        hdr->imageHeight = height;
        ptr = buffer + sizeof(*hdr);    // Writing will continue here.

        // Palette information.
        if(format == DGL_COLOR_INDEX_8
            || format == DGL_COLOR_INDEX_8_PLUS_A8)
        {
            hdr->colorMapType = 1;
            hdr->colorMapOrigin = 0;
            hdr->colorMapLength = 256;
            hdr->colorMapEntrySize = 24;

            // Write the color map (BGR).
            for(i = 0; i < 256; ++i)
            {
                *ptr++ = texturePalette[i].color[CB];
                *ptr++ = texturePalette[i].color[CG];
                *ptr++ = texturePalette[i].color[CR];
            }
        }

        if(format == DGL_COLOR_INDEX_8
            /*|| format == DGL_COLOR_INDEX_8_PLUS_A8*/)
        {
            // Color-mapped.
            hdr->imageType = 1;
            /*hdr->colorMapType = 1;
            hdr->colorMapOrigin = 0;
            hdr->colorMapLength = 256;
            hdr->colorMapEntrySize = 24;*/
            if(format == DGL_COLOR_INDEX_8)
            {
                hdr->imagePixelSize = 8;
                hdr->imageDescriptor.attributeBits = 0;
            }
            else
            {
                hdr->imagePixelSize = 16; // Contains the 8 attribute bits.
                hdr->imageDescriptor.attributeBits = 8;
            }
    /*      // Write the color map (BGR).
            for(i = 0; i < 256; i++)
            {
                *ptr++ = texturePalette[i].color[CB];
                *ptr++ = texturePalette[i].color[CG];
                *ptr++ = texturePalette[i].color[CR];
            }
    */
            // Write the image data.
            if(format == DGL_COLOR_INDEX_8)
            {
                // Copy line by line.
                for(y = height - 1; y >= 0; y--, ptr += width)
                    memcpy(ptr, (byte*)data + y*width, width);
            }
            else // DGL_COLOR_INDEX_8_PLUS_A8
            {
                // The alpha values must be interleaved.
                for(y = height - 1; y >= 0; y--)
                {
                    in = (byte*)data + y*width;
                    alphaIn = in + width*height;
                    for(x = 0; x < width; ++x)
                    {
                        *ptr++ = *in++;
                        *ptr++ = *alphaIn++;
                    }
                }
            }
        }
        else if(format == DGL_COLOR_INDEX_8_PLUS_A8)
        {
            hdr->imageType = 2;
            hdr->imagePixelSize = 32;
            hdr->imageDescriptor.attributeBits = 8;

            // The alpha values must be interleaved.
            for(y = height - 1; y >= 0; y--)
            {
                in = (byte*)data + y*width;
                alphaIn = in + width*height;
                for(x = 0; x < width; ++x)
                {
                    *ptr++ = texturePalette[*in].color[CB];
                    *ptr++ = texturePalette[*in].color[CG];
                    *ptr++ = texturePalette[*in++].color[CR];
                    *ptr++ = *alphaIn++;
                }
            }
        }
        else // Not color-mapped.
        {
            boolean hasAlpha = (format == DGL_RGBA
                || format == DGL_LUMINANCE_PLUS_A8);

            hdr->imageType = 2;
            hdr->colorMapType = 0;
            hdr->imagePixelSize = hasAlpha? 32 : 24;
            hdr->imageDescriptor.attributeBits = hasAlpha? 8 : 0;

            // Write the image data.
            switch(format)
            {
            case DGL_RGB:
                for(y = height - 1; y >= 0; y--)
                {
                    in = (byte*)data + y*width*3;
                    for(x = 0; x < width; ++x, in += 3)
                    {
                        *ptr++ = in[CB];
                        *ptr++ = in[CG];
                        *ptr++ = in[CR];
                    }
                }
                break;

            case DGL_RGBA:
                for(y = height - 1; y >= 0; y--)
                {
                    in = (byte*)data + y*width*4;
                    for(x = 0; x < width; ++x, in += 4)
                    {
                        *ptr++ = in[CB];
                        *ptr++ = in[CG];
                        *ptr++ = in[CR];
                        *ptr++ = in[CA];
                    }
                }
                break;

            case DGL_LUMINANCE:
                for(y = height - 1; y >= 0; y--)
                {
                    in = (byte*)data + y*width;
                    for(x = 0; x < width; ++x)
                    {
                        *ptr++ = *in;
                        *ptr++ = *in;
                        *ptr++ = *in++;
                    }
                }
                break;

            case DGL_LUMINANCE_PLUS_A8:
                for(y = height - 1; y >= 0; y--)
                {
                    in = (byte*)data + y*width;
                    alphaIn = in + width*height;
                    for(x = 0; x < width; ++x)
                    {
                        *ptr++ = *in;
                        *ptr++ = *in;
                        *ptr++ = *in++;
                        *ptr++ = *alphaIn++;
                    }
                }
                break;
            }
        }

        // Create the texture.
        if(FAILED(hr = D3DXCreateTextureFromFileInMemoryEx(dev,
            buffer, ptr - buffer, 0, 0, genMips? 0 : 1, 0,
              format == DGL_RGB? (hiBits? D3DFMT_R8G8B8 : D3DFMT_R5G6B5)
            : format == DGL_RGBA? (hiBits? D3DFMT_A8R8G8B8 :
                useBadAlpha? D3DFMT_A1R5G5B5 : D3DFMT_A4R4G4B4)
            : format == DGL_COLOR_INDEX_8? D3DFMT_P8
            : format == DGL_COLOR_INDEX_8_PLUS_A8? D3DFMT_A8P8
            : format == DGL_LUMINANCE? D3DFMT_L8
            : format == DGL_LUMINANCE_PLUS_A8? D3DFMT_A8L8 : D3DFMT_UNKNOWN,
            D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, 0,
            NULL, NULL, &tex->ptr)))
        {
            free(buffer);
            DXError("D3DXCreateTextureFromFileInMemoryEx");

            return DGL_ERROR;
        }
        free(buffer);
    }

    // We're done!
    dev->SetTexture(CUR_STAGE, tex->ptr);
    SetTexStates(CUR_STAGE, tex, STSF_ALL);

    return DGL_OK;
}

void DG_DeleteTextures(int num, DGLuint *names)
{
    for(int i = 0; i < num; ++i)
    {
        // Unbind a deleted texture.
        for(int k = 0; k < MAX_TEX_STAGES; ++k)
        {
            if(boundTexName[k] == names[i]) Unbind(k);
        }

        // Check that it's a valid name.
        int idx = NAME_TO_IDX(names[i]);
        if(idx < 0 || idx >= numTexData)
            continue;

        // Clear all data.
        tex_t *tex = &texData[idx];
        if(tex->ptr)
            tex->ptr->Release();
        memset(tex, 0, sizeof(*tex));
    }
}

void DG_TexParameter(int pname, int param)
{
    tex_t *tex = GetBoundTexture(CUR_STAGE);

    if(!tex)
        return;

    switch(pname)
    {
    case DGL_MIN_FILTER:
        tex->minFilter = (param == DGL_NEAREST
            || param == DGL_NEAREST_MIPMAP_NEAREST
            || param == DGL_NEAREST_MIPMAP_LINEAR? D3DTEXF_POINT
            : useAnisotropic? D3DTEXF_ANISOTROPIC : D3DTEXF_LINEAR);
        tex->mipFilter = (param == DGL_NEAREST
            || param == DGL_LINEAR? D3DTEXF_NONE
            : param == DGL_NEAREST_MIPMAP_NEAREST
            || param == DGL_LINEAR_MIPMAP_NEAREST? D3DTEXF_POINT
            : D3DTEXF_LINEAR);
        SetTexStates(CUR_STAGE, tex, STSF_MIN_FILTER);
        break;

    case DGL_MAG_FILTER:
        tex->magFilter = (param == DGL_NEAREST
            || param == DGL_NEAREST_MIPMAP_NEAREST
            || param == DGL_NEAREST_MIPMAP_LINEAR? D3DTEXF_POINT
            : D3DTEXF_LINEAR);
        SetTexStates(CUR_STAGE, tex, STSF_MAG_FILTER);
        break;

    case DGL_WRAP_S:
        tex->addressModeU = (param == DGL_CLAMP? D3DTADDRESS_CLAMP
            : D3DTADDRESS_WRAP);
        SetTexStates(CUR_STAGE, tex, STSF_ADDRESS_U);
        break;

    case DGL_WRAP_T:
        tex->addressModeV = (param == DGL_CLAMP? D3DTADDRESS_CLAMP
            : D3DTADDRESS_WRAP);
        SetTexStates(CUR_STAGE, tex, STSF_ADDRESS_V);
        break;
    }
}

/**
 * NOTE: Currently not needed by the engine.
 */
void DG_GetTexParameterv(int level, int pname, int *v)
{
}

void DG_Palette(int format, void *data)
{
    byte       *ptr = (byte*) data;
    int         size = (format == DGL_RGBA? 4 : 3);

    for(int i = 0; i < 256; ++i, ptr += size)
    {
        texturePalette[i].color[CR] = ptr[CR];
        texturePalette[i].color[CG] = ptr[CG];
        texturePalette[i].color[CB] = ptr[CB];
        texturePalette[i].color[CA] = (format == DGL_RGBA? ptr[CA] : 255);
    }
}

byte *GetPaletteColor(int index)
{
    if(index < 0 || index > 255)
        return NULL;

    return texturePalette[index].color;
}

/**
 * @return          The name of the texture that got replaced by the call.
 */
int DG_Bind(DGLuint texture)
{
    return Bind(CUR_STAGE, texture);
}
