/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * gl_png.c: PNG Images
 *
 * Portable Network Graphics high-level handling.
 * You'll need libpng and zlib.
 */

// HEADER FILES ------------------------------------------------------------

#include <png.h>
#include <setjmp.h>

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void PNGAPI user_error_fn(png_structp png_ptr, png_const_charp error_msg)
{
    Con_Message("PNG-Error: %s\n", error_msg);
}

void PNGAPI user_warning_fn(png_structp png_ptr, png_const_charp warning_msg)
{
    VERBOSE(Con_Message("PNG-Warning: %s\n", warning_msg));
}

/*
 * libpng calls this to read from files.
 */
void PNGAPI my_read_data(png_structp read_ptr, png_bytep data,
                         png_size_t length)
{
    F_Read(data, length, png_get_io_ptr(read_ptr));
}

/*
 * Reads the given PNG image and returns a pointer to a planar RGB or
 * RGBA buffer. Width and height are set, and pixelSize is either 3 (RGB)
 * or 4 (RGBA). The caller must free the allocated buffer with Z_Free.
 * width, height and pixelSize can't be NULL. Handles 1-4 channels.
 */
unsigned char *PNG_Load(const char *fileName, int *width, int *height,
                        int *pixelSize)
{
    DFILE  *file;
    png_structp png_ptr = 0;
    png_infop png_info = 0, end_info = 0;
    png_bytep *rows, pixel;
    unsigned char *retbuf = 0;  // The return buffer.
    int     i, k, off;

    if((file = F_Open(fileName, "rb")) == NULL)
        return NULL;

    // Init libpng.
    png_ptr =
        png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, user_error_fn,
                               user_warning_fn);
    if(!png_ptr)
        goto pngstop;

    png_info = png_create_info_struct(png_ptr);
    if(!png_info)
        goto pngstop;

    end_info = png_create_info_struct(png_ptr);
    if(!end_info)
        goto pngstop;

    if(setjmp(png_jmpbuf(png_ptr)))
        goto pngstop;
    //png_init_io(png_ptr, file);
    png_set_read_fn(png_ptr, file, my_read_data);

    png_read_png(png_ptr, png_info, PNG_TRANSFORM_IDENTITY, NULL);

    // Check if it can be used.
    if(png_info->bit_depth != 8)
    {
        Con_Message("PNG_Load: Bit depth must be 8.\n");
        goto pngstop;
    }
    if(!png_info->width || !png_info->height)
    {
        Con_Message("PNG_Load: Bad file? Size is zero.\n");
        goto pngstop;
    }

    // Information about the image.
    *width = png_info->width;
    *height = png_info->height;
    *pixelSize = png_info->channels;

    // Paletted images have three color components per pixel.
    if(*pixelSize == 1)
        *pixelSize = 3;
    if(*pixelSize == 2)
        *pixelSize = 4;         // With alpha channel.

    // OK, let's copy it into Doomsday's buffer.
    // FIXME: Why not load directly into it?
    retbuf = M_Malloc(4 * png_info->width * png_info->height);
    rows = png_get_rows(png_ptr, png_info);
    for(i = 0; i < *height; i++)
    {
        if(png_info->channels >= 3)
        {
            memcpy(retbuf + i * (*pixelSize) * png_info->width, rows[i],
                   (*pixelSize) * png_info->width);
        }
        else                    // Paletted image.
        {
            for(k = 0; k < *width; k++)
            {
                pixel = retbuf + ((*pixelSize) * (i * png_info->width + k));
                off = k * png_info->channels;
                pixel[0] = png_info->palette[rows[i][off]].red;
                pixel[1] = png_info->palette[rows[i][off]].green;
                pixel[2] = png_info->palette[rows[i][off]].blue;
                if(png_info->channels == 2) // Alpha data.
                    pixel[3] = rows[i][off + 1];
            }
        }
    }

    // Shutdown.
  pngstop:
    png_destroy_read_struct(&png_ptr, &png_info, &end_info);
    F_Close(file);
    return retbuf;
}
