/*
 * Copyright (c) 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is distributed under the GNU General Public License
 * version 2 (or, at your option, any later version).  Please visit
 * http://www.gnu.org/licenses/gpl.html for details.
 *
 * GL commands generation is from id software's qdata (with some
 * slight modifications).  All models are handled internally as DMDs.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#ifdef _WIN32
#include <conio.h>
#endif

#include "md2tool.h"

// MACROS ------------------------------------------------------------------

#ifdef _WIN32
#  define stricmp     _stricmp
#  define cprintf     _cprintf
#elif defined __GNUC__
#  include <strings.h>
#  define stricmp     strcasecmp
#  define cprintf     printf
#endif

// TYPES -------------------------------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void ModelSetSaveFormat(model_t *mo, int magic);

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     commands[16384];
int     numcommands;
int     numglverts;
int     used[MAX_TRIANGLES];
int     strip_xyz[128];
int     strip_st[128];
int     strip_tris[128];
int     stripcount;
dtriangle_t triangles[MAX_TRIANGLES];

float avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

int     myargc, argpos;
char    **myargv;
int     savelod = 0;
int     num_optimization_passes = 1;
float   error_factor = 1;

// CODE --------------------------------------------------------------------

//===========================================================================
// PackVector
//  Converts a float vector to yaw9/pitch7.
//===========================================================================
unsigned short PackVector(float vec[3])
{
    float yaw = 0, pitch = 0, len;
    int iyaw, ipitch;

    // First check for special cases (up/down).
    if (vec[VX] == 0 && vec[VY] == 0)
    {
        if (vec[VZ] == 0) return 0; // This is not a good vector.
        // Up or down...
        pitch = PI/2;
        if (vec[VZ] < 0) pitch = -pitch;
    }
    else
    {
        // First determine yaw (XY plane).
        yaw = (float) atan2(vec[VY], vec[VX]);
        len = (float) sqrt(vec[VX]*vec[VX] + vec[VY]*vec[VY]);
        pitch = (float) atan2(vec[VZ], len);
    }
    // Now we have yaw and pitch angles, encode them into a word.
    // (packed: pppppppy yyyyyyyy)
    iyaw = (int) (yaw/PI*256); // Convert 2*PI => 512
    while (iyaw > 511) iyaw -= 512;
    while (iyaw < 0) iyaw += 512;

    ipitch = (int) ((pitch/(PI/2) + 1)*64);
    if (ipitch > 127) ipitch = 127;
    if (ipitch < 0) ipitch = 0;

/*  //----DEBUG----
    printf("PackVector: (%f,%f,%f) => (%i,%i)\n",
        vec[0], vec[1], vec[2], iyaw, ipitch);
    //----DEBUG----*/

    return iyaw | (ipitch << 9);
}

//===========================================================================
// UnpackVector
//  Packed: pppppppy yyyyyyyy. Yaw is on the XY plane.
//===========================================================================
void UnpackVector(unsigned short packed, float vec[3])
{
    float yaw = (packed & 511)/512.0f * 2*PI;
    float pitch = ((packed >> 9)/127.0f - 0.5f) * PI;
    float cosp = (float) cos(pitch);
    vec[VX] = (float) cos(yaw) * cosp;
    vec[VY] = (float) sin(yaw) * cosp;
    vec[VZ] = (float) sin(pitch);

/*  //----DEBUG----
    printf("UnpackVector: (%i,%i) => (%f,%f,%f)\n",
        packed&511, packed>>9, vec[0], vec[1], vec[2]);
    //----DEBUG----*/
}

//===========================================================================
// GetNormalIndex
//===========================================================================
int GetNormalIndex(float vec[3])
{
    float dot, maxprod;
    int j, n, idx;

    // Find closest match in the normals list.
    for (j = 0; j < NUMVERTEXNORMALS; j++)
    {
        // Dot product.
        for (dot = 0, n = 0; n < 3; n++)
            dot += avertexnormals[j][n] * vec[n];
        if (!j || dot > maxprod)
        {
            maxprod = dot;
            idx = j;
        }
    }
    return idx;
}

//===========================================================================
// CheckOption
//  Checks the command line for the given option, and returns its index
//  number if it exists. Zero is returned if the option is not found.
//===========================================================================
int CheckOption(char *opt)
{
    int i;

    for (i = 1; i < myargc; i++)
        if (!stricmp(myargv[i], opt))
            return argpos = i;
    return 0;
}

//===========================================================================
// NextOption
//  Returns the next option the command line, or NULL if there are no more
//  options.
//===========================================================================
const char *NextOption(void)
{
    if (++argpos >= myargc) return NULL;
    return myargv[argpos];
}

//===========================================================================
// SkipWhite
//===========================================================================
const char *SkipWhite(const char *str)
{
    while (isspace(*str) && *str) str++;
    return str;
}

//===========================================================================
// PrintBanner
//===========================================================================
void PrintBanner(void)
{
    printf("\n### md2tool v"MD2TOOL_VERSION" by Jaakko Keränen "
           "<jaakko.keranen@iki.fi> ###\n\n");
}

//===========================================================================
// PrintUsage
//===========================================================================
void PrintUsage(void)
{
    printf("Usage: md2tool [-flip] [-renorm] [-dsk] [-s <skinfile>]\n");
    printf("       [-skin <num> <skinfile>] [-del <num>]\n");
    printf("       [-delframes to|from|<num> <num>] [-delskin <num>]\n");
    printf("       [-skinsize <width> <height>] [-gl] [-info] [-create <framelistfile>]\n");
    printf("       [-md2] [-dmd] [-savelod <num>] [-lod] [-ef <num>] [-op <num>] [-tcmap]\n");
    printf("       [-mg] [-fn <filename>] [-weld] [-weldtc] model[.md2|.dmd] ...\n\n");
    printf("-create     Create an empty model based on a frame list (each line specifies\n");
    printf("            a frame name, empty lines are skipped, comments begin with ; ).\n");
    printf("-del        Delete one frame.\n");
    printf("-delframes  Delete a range of frames.\n");
    printf("-delskin    Delete one skin.\n");
    printf("-dmd        Save model as DMD.\n");
    printf("-dsk        Set skin zero to the default skin name (model name + PCX).\n");
    printf("-ef         Set error factor for mesh optimization (default: 1.0).\n");
    printf("-flip       Flip triangles. Automatically builds GL commands.\n");
    printf("-fn         Change the name of the model file.\n");
    printf("-gl         Build GL commands.\n");
    printf("-info       Display model information.\n");
    printf("-lod        Generate detail levels (automatically saved as DMD).\n");
    printf("-md2        Save model as MD2 (the default).\n");
    printf("-mg         Display triangle groups in the texture coordinate map.\n");
    printf("-op         Set the number of mesh optimization passes.\n");
    printf("-renorm     Calculate vertex normals.\n");
    printf("-s          Set skin zero.\n");
    printf("-savelod    The level to save when saving as MD2 (default: 0).\n");
    printf("-skin       Set the specified skin.\n");
    printf("-skinsize   Set skin dimensions.\n");
    printf("-tcmap      Display texture coordinate map when optimizing.\n");
    printf("-weld       Weld vertices (only for models with one frame).\n");
    printf("-weldtc     Weld texture coordinates (removes all duplicates).\n");
}

//===========================================================================
// DoError
//  Fatal error messages cause the execution of the program to be aborted.
//===========================================================================
void DoError(int code)
{
    printf("\nERROR: ");
    switch (code)
    {
    case MTERR_INVALID_OPTION:
        printf("Invalid usage of a command line option.\n");
        PrintUsage();
        exit(1);

    case MTERR_INVALID_SKIN_NUMBER:
        printf("Invalid skin number.\n");
        exit(2);

    case MTERR_INVALID_FRAME_NUMBER:
        printf("Invalid frame number.\n");
        exit(3);

    case MTERR_BAD_MAGIC:
        printf("The file doesn't appear to be a valid MD2/DMD model.\n");
        break;

    case MTERR_NO_FILES:
        printf("No model files specified.\n");
        break;

    case MTERR_LISTFILE_NA:
        printf("The specified list file doesn't exist.\n");
        break;

    case MTERR_READ_FAILED:
        printf("Failed reading from file.\n");
        exit(4);
    }
}

//===========================================================================
// Load
//  Allocates and loads in the given data.
//===========================================================================
void *Load(FILE *file, int offset, int len)
{    
    if (len > 0)
    {
        void *ptr = malloc(len);
        fseek(file, offset, SEEK_SET);
        if (fread(ptr, len, 1, file) == 0)
        {
            DoError(MTERR_READ_FAILED);
        }
        return ptr;
    }
    return NULL;
}

//===========================================================================
// ModelNew
//  Create an empty MD2 model.
//===========================================================================
void ModelNew(model_t *mo, const char *filename)
{
    dmd_header_t *hd = &mo->header;
    dmd_info_t *inf = &mo->info;

    printf("Creating new model \"%s\"...\n", filename);

    memset(mo, 0, sizeof(*mo));
    strcpy(mo->fileName, filename);
    mo->modified = true;
    // Setup the header.
    hd->magic = MD2_MAGIC;
    hd->version = 8;
    inf->skinWidth = 1;
    inf->skinHeight = 1;
    inf->frameSize = MD2_FRAMESIZE(0);
}

//===========================================================================
// ModelOpen
//  Open an MD2 or DMD model.
//===========================================================================
int ModelOpen(model_t *mo, const char *filename)
{
    FILE *file;
    dmd_header_t *hd;
    dmd_chunk_t chunk;
    dmd_info_t *inf;
    md2_header_t oldhd;
    md2_frame_t *oldframes;
    char fn[256];
    int i, k;
    void *ptr;

    strcpy(fn, filename);
    if ((file = fopen(fn, "rb")) == NULL)
    {
        strcat(fn, ".md2");
        if ((file = fopen(fn, "rb")) == NULL)
        {
            strcpy(fn, filename);
            strcat(fn, ".dmd");
            if ((file = fopen(fn, "rb")) == NULL)
            {
                printf("Couldn't open the model \"%s\".\n", filename);
                return false;
            }
        }
    }
    printf("Opening model \"%s\"...\n", fn);

    memset(mo, 0, sizeof(*mo));
    strcpy(mo->fileName, fn);
    if (!fread(&mo->header, sizeof(mo->header), 1, file))
    {
        DoError(MTERR_READ_FAILED);
    }
    hd = &mo->header;
    inf = &mo->info;
    if (hd->magic == DMD_MAGIC)
    {
        // Read the chunks.
        if (!fread(&chunk, sizeof(chunk), 1, file))
        {
            DoError(MTERR_READ_FAILED);
        }
        while (chunk.type != DMC_END)
        {
            switch (chunk.type)
            {
            case DMC_INFO:
                if (!fread(inf, sizeof(*inf), 1, file))
                {
                    DoError(MTERR_READ_FAILED);
                }
                break;

            default:
                // Just skip all unknown chunks.
                fseek(file, chunk.length, SEEK_CUR);
                printf("Skipping unknown chunk (type %i, length %i).\n",
                    chunk.type, chunk.length);
            }
            // Read the next chunk.
            if (!fread(&chunk, sizeof(chunk), 1, file))
            {
                DoError(MTERR_READ_FAILED);
            }
        }

        // Allocate and load in the data.
        mo->skins = Load(file, inf->offsetSkins, sizeof(dmd_skin_t)
            * inf->numSkins);
        mo->texCoords = Load(file, inf->offsetTexCoords,
            sizeof(dmd_textureCoordinate_t) * inf->numTexCoords);
        mo->frames = Load(file, inf->offsetFrames, inf->frameSize
            * inf->numFrames);
        ptr = Load(file, inf->offsetLODs, sizeof(dmd_levelOfDetail_t)
            * inf->numLODs);
        memcpy(mo->lodinfo, ptr, sizeof(dmd_levelOfDetail_t) * inf->numLODs);
        free(ptr);
        for (i = 0; i < inf->numLODs; i++)
        {
            mo->lods[i].triangles = Load(file, mo->lodinfo[i].offsetTriangles,
                sizeof(dmd_triangle_t) * mo->lodinfo[i].numTriangles);
            mo->lods[i].glCommands = Load(file, mo->lodinfo[i].offsetGlCommands,
                sizeof(int) * mo->lodinfo[i].numGlCommands);
        }
    }
    else if (hd->magic == MD2_MAGIC)
    {
        rewind(file);
        if (!fread(&oldhd, sizeof(oldhd), 1, file))
        {
            DoError(MTERR_READ_FAILED);
        }

        // Convert it to DMD data but keep it as an MD2.
        hd->magic = MD2_MAGIC;
        hd->version = 8;
        hd->flags = 0;
        inf->skinWidth = oldhd.skinWidth;
        inf->skinHeight = oldhd.skinHeight;
        inf->frameSize = DMD_FRAMESIZE(oldhd.numVertices);
        inf->numLODs = 1;
        inf->numSkins = oldhd.numSkins;
        inf->numTexCoords = oldhd.numTexCoords;
        inf->numVertices = oldhd.numVertices;
        inf->numFrames = oldhd.numFrames;
        inf->offsetSkins = oldhd.offsetSkins;
        inf->offsetTexCoords = oldhd.offsetTexCoords;
        inf->offsetFrames = oldhd.offsetFrames;
        inf->offsetLODs = oldhd.offsetEnd; // Doesn't exist.
        mo->lodinfo[0].numTriangles = oldhd.numTriangles;
        mo->lodinfo[0].numGlCommands = oldhd.numGlCommands;
        mo->lodinfo[0].offsetTriangles = oldhd.offsetTriangles;
        mo->lodinfo[0].offsetGlCommands = oldhd.offsetGlCommands;
        inf->offsetEnd = oldhd.offsetEnd;

        // Allocate and load in the data.
        mo->skins = Load(file, inf->offsetSkins, sizeof(md2_skin_t)
            * inf->numSkins);
        mo->texCoords = Load(file, inf->offsetTexCoords,
            sizeof(md2_textureCoordinate_t) * inf->numTexCoords);
        mo->lods[0].triangles = Load(file, mo->lodinfo[0].offsetTriangles,
            sizeof(md2_triangle_t) * mo->lodinfo[0].numTriangles);
        mo->lods[0].glCommands = Load(file, mo->lodinfo[0].offsetGlCommands,
            sizeof(int) * mo->lodinfo[0].numGlCommands);

        oldframes = Load(file, inf->offsetFrames, oldhd.frameSize
            * inf->numFrames);
        mo->frames = malloc(inf->frameSize * inf->numFrames);
        // Convert to DMD frames.
        for (i = 0; i < inf->numFrames; i++)
        {
            md2_frame_t *md2f = (md2_frame_t*) ((byte*)oldframes + i*oldhd.frameSize);
            dmd_frame_t *dmdf = (dmd_frame_t*) ((byte*)mo->frames + i*inf->frameSize);
            memcpy(dmdf->name, md2f->name, sizeof(md2f->name));
            memcpy(dmdf->scale, md2f->scale, sizeof(md2f->scale));
            memcpy(dmdf->translate, md2f->translate, sizeof(md2f->translate));
            for (k = 0; k < inf->numVertices; k++)
            {
                memcpy(dmdf->vertices[k].vertex,
                    md2f->vertices[k].vertex, sizeof(byte)*3);
                dmdf->vertices[k].normal = PackVector
                    (avertexnormals[md2f->vertices[k].lightNormalIndex]);
            }
        }
        free(oldframes);
    }
    else
    {
        DoError(MTERR_BAD_MAGIC);
        fclose(file);
        return false;
    }
    fclose(file);

    // Print some DMD information.
    printf("%i triangles, %i vertices, %i frames, %i skin%s (%ix%i).\n",
        mo->lodinfo[0].numTriangles, inf->numVertices, inf->numFrames,
        inf->numSkins, inf->numSkins != 1? "s" : "",
        inf->skinWidth, inf->skinHeight);
    /*for (i = 1; i < inf->numLODs; i++)
    {
        printf("  Level %i: %i triangles, %i GL commands.\n", i,
            mo->lodinfo[i].numTriangles, mo->lodinfo[i].numGlCommands);
    }*/
    return true;
}

//===========================================================================
// ModelSaveMD2
//===========================================================================
void ModelSaveMD2(model_t *mo, FILE *file)
{
    dmd_info_t *inf = &mo->info;
    md2_header_t hd;
    float vec[3];
    int i, k;
    byte n;

    // Check that the level-to-save is valid.
    if (savelod < 0 || savelod >= inf->numLODs)
    {
        printf("Invalid savelod (%i), saving level 0 instead.\n", savelod);
        savelod = 0;
    }

    hd.magic = MD2_MAGIC;
    hd.version = 8;
    hd.skinWidth = inf->skinWidth;
    hd.skinHeight = inf->skinHeight;
    hd.frameSize = MD2_FRAMESIZE(inf->numVertices);
    hd.numSkins = inf->numSkins;
    hd.numVertices = inf->numVertices;
    hd.numTexCoords = inf->numTexCoords;
    hd.numTriangles = mo->lodinfo[savelod].numTriangles;
    hd.numGlCommands = mo->lodinfo[savelod].numGlCommands;
    hd.numFrames = inf->numFrames;

    // The header will be written again after we know the locations
    // of the other data.
    fwrite(&hd, sizeof(hd), 1, file);

    // Skins.
    hd.offsetSkins = ftell(file);
    fwrite(mo->skins, sizeof(*mo->skins), hd.numSkins, file);
    // Texture coordinates.
    hd.offsetTexCoords = ftell(file);
    fwrite(mo->texCoords, sizeof(*mo->texCoords), hd.numTexCoords, file);
    // Triangles.
    hd.offsetTriangles = ftell(file);
    fwrite(mo->lods[savelod].triangles, sizeof(md2_triangle_t),
        hd.numTriangles, file);
    // Frames must be written separately (because of the normals).
    hd.offsetFrames = ftell(file);
    for (i = 0; i < hd.numFrames; i++)
    {
        dmd_frame_t *dmdf = (dmd_frame_t*) ((byte*)mo->frames
            + i*inf->frameSize);
        fwrite(dmdf->scale, sizeof(float) * 3, 1, file);
        fwrite(dmdf->translate, sizeof(float) * 3, 1, file);
        fwrite(dmdf->name, 16, 1, file);
        for (k = 0; k < inf->numVertices; k++)
        {
            fwrite(dmdf->vertices[k].vertex, 3, 1, file);
            UnpackVector(dmdf->vertices[k].normal, vec);
            n = GetNormalIndex(vec);
            fwrite(&n, 1, 1, file);
        }
    }
    // GL commands.
    hd.offsetGlCommands = ftell(file);
    fwrite(mo->lods[savelod].glCommands, sizeof(int), hd.numGlCommands, file);
    // The end.
    hd.offsetEnd = ftell(file);

    // Rewrite the header.
    rewind(file);
    fwrite(&hd, sizeof(hd), 1, file);
}

//===========================================================================
// ModelSaveDMD
//===========================================================================
void ModelSaveDMD(model_t *mo, FILE *file)
{
    dmd_info_t *inf = &mo->info;
    int offsetInfo, i;
    dmd_chunk_t chunk;

    // First the header.
    mo->header.version = 1; // This is version 1.
    fwrite(&mo->header, sizeof(mo->header), 1, file);

    // Then the chunks.
    chunk.type = DMC_INFO;
    chunk.length = sizeof(*inf);
    fwrite(&chunk, sizeof(chunk), 1, file);
    offsetInfo = ftell(file);
    fwrite(inf, sizeof(*inf), 1, file);

    // That was the only chunk, write the end marker.
    chunk.type = DMC_END;
    chunk.length = 0;
    fwrite(&chunk, sizeof(chunk), 1, file);

    // Write the data.
    inf->offsetSkins = ftell(file);
    fwrite(mo->skins, sizeof(dmd_skin_t), inf->numSkins, file);
    inf->offsetTexCoords = ftell(file);
    fwrite(mo->texCoords, sizeof(dmd_textureCoordinate_t), inf->numTexCoords,
        file);
    inf->offsetFrames = ftell(file);
    fwrite(mo->frames, DMD_FRAMESIZE(inf->numVertices), inf->numFrames,
        file);
    // Write the LODs.
    for (i = 0; i < inf->numLODs; i++)
    {
        mo->lodinfo[i].offsetTriangles = ftell(file);
        fwrite(mo->lods[i].triangles, sizeof(dmd_triangle_t),
            mo->lodinfo[i].numTriangles, file);

        mo->lodinfo[i].offsetGlCommands = ftell(file);
        fwrite(mo->lods[i].glCommands, sizeof(int),
            mo->lodinfo[i].numGlCommands, file);
    }
    // And the LOD info.
    inf->offsetLODs = ftell(file);
    fwrite(mo->lodinfo, sizeof(dmd_levelOfDetail_t), inf->numLODs, file);

    inf->offsetEnd = ftell(file);

    // Rewrite the info chunk (now that the offsets are known).
    fseek(file, offsetInfo, SEEK_SET);
    fwrite(inf, sizeof(*inf), 1, file);
}

//===========================================================================
// ModelClose
//  The model is saved as MD2 or DMD depending on the magic number.
//===========================================================================
void ModelClose(model_t *mo)
{
    FILE *file;
    int i;

    printf("Closing model \"%s\"...\n", mo->fileName);

    if (mo->modified)
    {
        // Open the file for writing.
        if ((file = fopen(mo->fileName, "wb")) == NULL)
        {
            printf("Can't open \"%s\" for writing.\n", mo->fileName);
            return;
        }
        if (mo->header.magic == DMD_MAGIC)
            ModelSaveDMD(mo, file);
        else if (mo->header.magic == MD2_MAGIC)
            ModelSaveMD2(mo, file);

        fclose(file);
        mo->modified = false;
    }

    // Free the memory allocated for the model.
    free(mo->skins);
    free(mo->texCoords);
    free(mo->frames);
    for (i = 0; i < mo->info.numLODs; i++)
    {
        free(mo->lods[i].triangles);
        free(mo->lods[i].glCommands);
    }
}

//===========================================================================
// CrossProd
//  Calculates the cross product of two vectors, which are specified by
//  three points.
//===========================================================================
void CrossProd(float *v1, float *v2, float *v3, float *out)
{
    float a[3], b[3];
    int i;

    for (i = 0; i < 3; i++)
    {
        a[i] = v2[i] - v1[i];
        b[i] = v3[i] - v1[i];
    }
    out[VX] = a[VY]*b[VZ] - a[VZ]*b[VY];
    out[VY] = a[VZ]*b[VX] - a[VX]*b[VZ];
    out[VZ] = a[VX]*b[VY] - a[VY]*b[VX];
}

//===========================================================================
// Norm
//  Normalize a vector.
//===========================================================================
void Norm(float *vector)
{
    float length = (float) sqrt(vector[VX]*vector[VX]
        + vector[VY]*vector[VY]
        + vector[VZ]*vector[VZ]);

    if (length)
    {
        vector[VX] /= length;
        vector[VY] /= length;
        vector[VZ] /= length;
    }
}

//===========================================================================
// GetFrame
//===========================================================================
dmd_frame_t *GetFrame(model_t *mo, int framenum)
{
    return (dmd_frame_t*) ((byte*) mo->frames + mo->info.frameSize
        * framenum);
}

//===========================================================================
// StripLength
//===========================================================================
int StripLength(model_t *model, int lod, int starttri, int startv)
{
    int         m1, m2;
    int         st1, st2;
    int         j;
    dtriangle_t *last, *check;
    int         k;

    used[starttri] = 2;

    last = &triangles[starttri];

    strip_xyz[0] = last->index_xyz[(startv)%3];
    strip_xyz[1] = last->index_xyz[(startv+1)%3];
    strip_xyz[2] = last->index_xyz[(startv+2)%3];
    strip_st[0] = last->index_st[(startv)%3];
    strip_st[1] = last->index_st[(startv+1)%3];
    strip_st[2] = last->index_st[(startv+2)%3];

    strip_tris[0] = starttri;
    stripcount = 1;

    m1 = last->index_xyz[(startv+2)%3];
    st1 = last->index_st[(startv+2)%3];
    m2 = last->index_xyz[(startv+1)%3];
    st2 = last->index_st[(startv+1)%3];

    // look for a matching triangle
nexttri:
    for (j=starttri+1, check=&triangles[starttri+1]
        ; j < model->lodinfo[lod].numTriangles; j++, check++)
    {
        for (k=0 ; k<3 ; k++)
        {
            if (check->index_xyz[k] != m1)
                continue;
            if (check->index_st[k] != st1)
                continue;
            if (check->index_xyz[ (k+1)%3 ] != m2)
                continue;
            if (check->index_st[ (k+1)%3 ] != st2)
                continue;

            // this is the next part of the fan

            // if we can't use this triangle, this tristrip is done
            if (used[j])
                goto done;

            // the new edge
            if (stripcount & 1)
            {
                m2 = check->index_xyz[ (k+2)%3 ];
                st2 = check->index_st[ (k+2)%3 ];
            }
            else
            {
                m1 = check->index_xyz[ (k+2)%3 ];
                st1 = check->index_st[ (k+2)%3 ];
            }

            strip_xyz[stripcount+2] = check->index_xyz[ (k+2)%3 ];
            strip_st[stripcount+2] = check->index_st[ (k+2)%3 ];
            strip_tris[stripcount] = j;
            stripcount++;

            used[j] = 2;
            goto nexttri;
        }
    }
done:

    // clear the temp used flags
    for (j=starttri+1 ; j < model->lodinfo[lod].numTriangles; j++)
        if (used[j] == 2) used[j] = 0;

    return stripcount;
}

//===========================================================================
// FanLength
//===========================================================================
int FanLength(model_t *model, int lod, int starttri, int startv)
{
    int     m1, m2;
    int     st1, st2;
    int     j;
    dtriangle_t *last, *check;
    int     k;

    used[starttri] = 2;

    last = &triangles[starttri];

    strip_xyz[0] = last->index_xyz[(startv)%3];
    strip_xyz[1] = last->index_xyz[(startv+1)%3];
    strip_xyz[2] = last->index_xyz[(startv+2)%3];
    strip_st[0] = last->index_st[(startv)%3];
    strip_st[1] = last->index_st[(startv+1)%3];
    strip_st[2] = last->index_st[(startv+2)%3];

    strip_tris[0] = starttri;
    stripcount = 1;

    m1 = last->index_xyz[(startv+0)%3];
    st1 = last->index_st[(startv+0)%3];
    m2 = last->index_xyz[(startv+2)%3];
    st2 = last->index_st[(startv+2)%3];


    // look for a matching triangle
nexttri:
    for (j=starttri+1, check=&triangles[starttri+1]
        ; j < model->lodinfo[lod].numTriangles; j++, check++)
    {
        for (k=0 ; k<3 ; k++)
        {
            if (check->index_xyz[k] != m1)
                continue;
            if (check->index_st[k] != st1)
                continue;
            if (check->index_xyz[ (k+1)%3 ] != m2)
                continue;
            if (check->index_st[ (k+1)%3 ] != st2)
                continue;

            // this is the next part of the fan

            // if we can't use this triangle, this tristrip is done
            if (used[j])
                goto done;

            // the new edge
            m2 = check->index_xyz[ (k+2)%3 ];
            st2 = check->index_st[ (k+2)%3 ];

            strip_xyz[stripcount+2] = m2;
            strip_st[stripcount+2] = st2;
            strip_tris[stripcount] = j;
            stripcount++;

            used[j] = 2;
            goto nexttri;
        }
    }
done:

    // clear the temp used flags
    for (j=starttri+1 ; j < model->lodinfo[lod].numTriangles; j++)
        if (used[j] == 2) used[j] = 0;

    return stripcount;
}

//===========================================================================
// OptimizeTexCoords
//  "Optimize" the texcoords. The tristrip builders only work correctly
//  if some triangles share texcoord indices, so we need to remove all
//  redundant references: the first (s,t) match for a texcoord is used
//  for all similar (u,v)s. -jk
//===========================================================================
void OptimizeTexCoords(model_t *mo, dtriangle_t *tris, int count)
{
    int i, j, k;
    short u, v;

    for (i = 0; i < count; i++)
        for (k = 0; k < 3; k++)
        {
            u = mo->texCoords[tris[i].index_st[k]].s;
            v = mo->texCoords[tris[i].index_st[k]].t;

            for (j = 0; j < mo->info.numTexCoords; j++)
            {
                if (mo->texCoords[j].s == u
                    && mo->texCoords[j].t == v)
                {
                    tris[i].index_st[k] = j;
                    break;
                }
            }
        }
}

//===========================================================================
// BuildGlCmds
//  Generate a list of trifans or strips for the model, which holds
//  for all frames. All the LODs present are processed.
//===========================================================================
void BuildGlCmds(model_t *mo)
{
    int     i, j, k, lod;
    int     numtris;
    int     startv;
    float   s, t;
    int     len, bestlen, besttype;
    int     best_xyz[1024];
    int     best_st[1024];
    int     best_tris[1024];
    int     type;
    int     numfans, numstrips, avgfan, avgstrip;

    printf("Building GL commands.\n");
    mo->modified = true;

    for (lod = 0; lod < mo->info.numLODs; lod++)
    {
        numfans = numstrips = avgfan = avgstrip = 0;
        numtris = mo->lodinfo[lod].numTriangles;

        // Init triangles.
        memcpy(triangles, mo->lods[lod].triangles,
            sizeof(dtriangle_t) * numtris);
        OptimizeTexCoords(mo, triangles, numtris);

        //
        // build tristrips
        //
        numcommands = 0;
        numglverts = 0;
        memset(used, 0, sizeof(used));
        for (i = 0; i < numtris; i++)
        {
            // pick an unused triangle and start the trifan
            if (used[i]) continue;

            bestlen = 0;
            for (type = 0; type < 2 ; type++)
            {
                for (startv =0 ; startv < 3 ; startv++)
                {
                    if (type == 1)
                        len = StripLength(mo, lod, i, startv);
                    else
                        len = FanLength(mo, lod, i, startv);
                    if (len > bestlen)
                    {
                        besttype = type;
                        bestlen = len;
                        for (j=0 ; j<bestlen+2 ; j++)
                        {
                            best_st[j] = strip_st[j];
                            best_xyz[j] = strip_xyz[j];
                        }
                        for (j=0 ; j<bestlen ; j++)
                            best_tris[j] = strip_tris[j];
                    }
                }
            }

            // mark the tris on the best strip/fan as used
            for (j=0 ; j<bestlen ; j++)
                used[best_tris[j]] = 1;

            if (besttype == 1)
            {
                numstrips++;
                avgstrip += bestlen + 2;
                commands[numcommands++] = (bestlen+2);
            }
            else
            {
                numfans++;
                avgfan += bestlen + 2;
                commands[numcommands++] = -(bestlen+2);
            }

            numglverts += bestlen+2;

            for (j=0 ; j<bestlen+2 ; j++)
            {
                // emit a vertex into the reorder buffer
                k = best_st[j];

                // emit s/t coords into the commands stream
                s = mo->texCoords[k].s; //base_st[k].s;
                t = mo->texCoords[k].t; //base_st[k].t;

                s = (s + 0.5f) / mo->info.skinWidth;
                t = (t + 0.5f) / mo->info.skinHeight;

                *(float *)&commands[numcommands++] = s;
                *(float *)&commands[numcommands++] = t;
                *(int *)&commands[numcommands++] = best_xyz[j];
            }
        }
        commands[numcommands++] = 0;        // end of list marker

        mo->lodinfo[lod].numGlCommands = numcommands;
        mo->lods[lod].glCommands = realloc(mo->lods[lod].glCommands,
            sizeof(int) * numcommands);
        memcpy(mo->lods[lod].glCommands, commands, sizeof(int) * numcommands);

        // Some statistics.
        printf("(Level %i)\n", lod);
        printf("  Number of strips: %-3i (%.3f vertices on average)\n",
            numstrips, numstrips? avgstrip/(float)numstrips : 0);
        printf("  Number of fans:   %-3i (%.3f vertices on average)\n",
            numfans, numfans? avgfan/(float)numfans : 0);
    }
}

//===========================================================================
// IsTexCoordConnected
//  Returns true if the two triangles can be connected via a chain of
//  shared texture coordinates.
//===========================================================================
int IsTexCoordConnected(model_t *mo, dtriangle_t *src, dtriangle_t *dest,
                        dtriangle_t *origtris, int orignumtris)
{
    int num = mo->info.numTexCoords;
    int result = false;
    int *tcmap = malloc(sizeof(int) * num);
    int i, j, k, didspread;
    int found;

    // The map tells us which texcoords have been checked.
    // 0 = not checked, 1 = newly spread, 2 = checked.
    memset(tcmap, 0, sizeof(*tcmap) * num);

    // Initial step: spread to the source triangle's texcoords.
    for (i = 0; i < 3; i++) tcmap[src->index_st[i]] = 1;
    didspread = true;
    while (didspread)
    {
        // Check if the newly spread-to vertices include one of the
        // target texcoords.
        for (i = 0; i < num; i++)
        {
            if (tcmap[i] != 1) continue;
            for (k = 0; k < 3; k++)
                if (dest->index_st[k] == i)
                {
                    // Hey, we reached the destination.
                    result = true;
                    goto tcdone;
                }
            // Not a match, mark it as checked.
            tcmap[i] = 2;
        }
        // Try spreading to new texcoords.
        didspread = false;
        // Spread to the coords of all triangles connected to this one.
        // Level zero triangles are used in the test.
        for (j = 0; j < orignumtris; j++)
        {
            // We'll only spread from checked texcoords.
            for (found = false, k = 0; k < 3; k++)
                if (tcmap[origtris[j].index_st[k]] > 0 /*== 2*/)
                {
                    found = true;
                    break;
                }
            if (!found) continue;
            // Spread to coords not yet spread to.
            for (k = 0; k < 3; k++)
                if (tcmap[origtris[j].index_st[k]] < 2)
                {
                    // Spread to this triangle.
                    for (k = 0; k < 3; k++)
                        if (!tcmap[origtris[j].index_st[k]])
                            tcmap[origtris[j].index_st[k]] = 1;
                    didspread = true;
                    break;
                }
        }
    }
tcdone:
    free(tcmap);
    return result;
}

#if 0
//===========================================================================
// FindGroupEdges
//===========================================================================
void FindGroupEdges(model_t *mo, dtriangle_t *tris, int numtris, int *ved)
{
    int i, j, k;
    int done, found, spread;

    // -1 means the vertex hasn't been checked yet.
    memset(ved, -1, sizeof(ved));

    done = false;
    while (!done)
    {
        done = true;
        // Find a good starting place.
        for (found = false, i = 0; i < mo->info.numTexCoords; i++)
        {
            if (ved[i] >= 0) continue; // Already checked.
            found = true;
            break; // This is good.
        }
        if (!found) break;

        // Start spreading from the starting vertex.
        ved[i] = 2;
        spread = true;
        while (spread)
        {
/*          for (spread = false, j = 0; j < numtris; j++)
            {
                // Does this triangle have an edge to spread along?
                for (k = 0; k < 3; k++)
                    if (ved[tris[j].index_st[k]] == 2
                        || ved[tris[j].index_st[(k+1)%3]] == 2)
                    {
                        // Spread along this edge.
                        if (ved[tris[j].index_st[k]] == 2)
                            ved[tris[j].index_st[(k+1)%3]] = 3;
                        else
                            ved[tris[j].index_st[k]] = 3;

                        // The ones we spread from certainly aren't edges.
                        if (ved[tris[j].index_st[k] == 2) ved[tris[j].index_st[k] = 0;
                        if (ved[tris[j].index_st[(k+1)%3] == 2) ved[tris[j].index_st[(k+1)%3] = 0;
                        spread = true;
                    }
            }
            if (spread)
            {
                for (k = 0; k < mo->info.numTexCoords; k++)
                    if (ved[k] == 3) ved[k] = 2;
            }*/

        }
    }
}
#endif

//===========================================================================
// HaveSharedTexCoord
//  Returns true if the two triangles have at least one common texcoord.
//===========================================================================
int HaveSharedTexCoord(dtriangle_t *a, dtriangle_t *b)
{
    int i, k;

    for (i = 0; i < 3; i++)
        for (k = 0; k < 3; k++)
            if (a->index_st[i] == b->index_st[k])
                return true;
    return false;
}

//===========================================================================
// TriangleNormal
//  Calculated using the normals in the first frame.
//===========================================================================
void TriangleNormal(model_t *mo, dtriangle_t *tri, float vec[3])
{
    dmd_frame_t *fr = GetFrame(mo, 0);
    dmd_vertex_t *vtx = fr->vertices;
    float pos[3][3];
    int i, j;

    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
        {
            pos[i][j] = vtx[tri->index_xyz[i]].vertex[j] * fr->scale[j] +
                fr->translate[j];
        }
    CrossProd(pos[0], pos[2], pos[1], vec);
    Norm(vec);
}

//===========================================================================
// PointOnLineSide
//===========================================================================
int PointOnLineSide(float x1, float y1, float x2, float y2, float cx, float cy)
{
    // (YA-YC)(XB-XA)-(XA-XC)(YB-YA)
    return ((y1-cy)*(x2-x1) - (x1-cx)*(y2-y1)) >= 0;
}

//===========================================================================
// IsValidTexTriangle
//===========================================================================
int IsValidTexTriangle(dtriangle_t *tri)
{
    int i, j;

    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            if (i != j && tri->index_st[i] == tri->index_st[j])
            {
                // Hmm!! This is a bad texture triangle, with no area!
                return false;
            }
    return true;
}

//===========================================================================
// IsClockwiseTexTriangle
//===========================================================================
int IsClockwiseTexTriangle(model_t *mo, dtriangle_t *tri)
{
    dmd_textureCoordinate_t *tc = mo->texCoords;
    return PointOnLineSide(tc[tri->index_st[0]].s, tc[tri->index_st[0]].t,
        tc[tri->index_st[1]].s, tc[tri->index_st[1]].t,
        tc[tri->index_st[2]].s, tc[tri->index_st[2]].t);
}

//===========================================================================
// InsideTexTriangle
//===========================================================================
int InsideTexTriangle(model_t *mo, float x, float y, dtriangle_t *tri)
{
    int i;
    dmd_textureCoordinate_t *tc = mo->texCoords;
    int test = false;

    if (!IsValidTexTriangle(tri)) return false;

    // First determine is this a clockwise or a counterclockwise triangle.
    if (!IsClockwiseTexTriangle(mo, tri))
    {
        // Counterclockwise.
        test = true;
    }

    for (i = 0; i < 3; i++)
    {
        if (PointOnLineSide(
            tc[tri->index_st[i]].s,
            tc[tri->index_st[i]].t,
            tc[tri->index_st[(i+1)%3]].s,
            tc[tri->index_st[(i+1)%3]].t,
            x, y) == test) return false;
    }
    return true;
}

//===========================================================================
// GroupTriangles
//  Returns the number of groups.
//===========================================================================
int GroupTriangles(model_t *mo, optriangle_t *tris, int numtris)
{
    int high = 0;
    int start, i, j;
    char spreadto[MAX_TRIANGLES], didspread;

    (void) mo; // unused

    memset(spreadto, 0, sizeof(spreadto));

    // 1. Find an ungrouped triangle (stop if not found).
    // 2. Give it a new group number.
    // 3. Spread as much as possible.
    // 4. Continue from step 1.

    for (;;)
    {
        // Find an ungrouped triangle.
        for (start = 0; start < numtris; start++)
            if (!tris[start].group) break;
        if (start == numtris) break; // Nothing further to do.
        // Spread to this triangle.
        spreadto[start] = 2;
        tris[start].group = ++high;
        didspread = true;
        while (didspread)
        {
            for (didspread = false, i = 0; i < numtris; i++)
            {
                if (spreadto[i] != 2) continue;
                spreadto[i] = 1;
                for (j = 0; j < numtris; j++)
                {
                    if (spreadto[j]) continue;
                    if (HaveSharedTexCoord(&tris[i].tri, &tris[j].tri))
                    {
                        didspread = true;
                        spreadto[j] = 2;
                        tris[j].group = high;
                    }
                }
            }
        }
    }
    return high;
}

//===========================================================================
// DrawTexCoordMap
//  A debugging aid.
//  Draw a nice picture of the texture coordinates.
//===========================================================================
void DrawTexCoordMap(model_t *mo, optriangle_t *tris, int numtris,
                     int *vertex_on_edge)
{
    int cols = 160;
    int rows = 60;
    int i, k, t, m, c, found;
    int hitgroup;
    int withgroups = CheckOption("-mg");

    for (i = 0; i < rows; i++)
    {
        for (k = 0; k < cols; k++)
        {
            for (found = 0, t = 0; t < numtris; t++)
            {
                for (m = 0; m < 3; m++)
                {
                    c = tris[t].tri.index_st[m];
                    if ((int)(mo->texCoords[c].s/(float)mo->info.skinWidth*(cols-1)) == k
                        && (int)(mo->texCoords[c].t/(float)mo->info.skinHeight*(rows-1)) == i)
                    {
                        if (!found) found = 1;
                        if (vertex_on_edge[c]) found = 2;
                    }
                }
            }
            if (!found)
            {
                for (t = 0; t < numtris; t++)
                    if (InsideTexTriangle(mo, k/(float)(cols-1)*mo->info.skinWidth,
                        i/(float)(rows-1)*mo->info.skinHeight, &tris[t].tri))
                    {
                        found = 3;
                        hitgroup = tris[t].group;
                        break;
                    }
            }
            printf("%c", !found? ' ' : found==1? '+' : found==2? '#'
                : (withgroups? 'A'+hitgroup-1 : ':'));
        }
        printf("\n");
    }
}

//===========================================================================
// OptimizeMesh
//  Returns the number of triangles left.
//===========================================================================
#define MAX_MERGED 64
int OptimizeMesh(model_t *mo, dtriangle_t *origtris, int orignumtris,
                 int level)
{
    optriangle_t *tris;
    int numtris;
    int i, k, j, m, t, c, highgroup;
    int tc1, tc2;
    int found;
    int vused[MAX_VERTS];
    int thisvtx, testvtx;
    int numconnected, nummerged, numbest;
    int connected[MAX_MERGED];
    int convtx[MAX_VERTS], contc[MAX_VERTS], numconvtx;
    dtriangle_t merged[MAX_MERGED], best[MAX_MERGED];
    float connected_normal[3], merged_normal[3], vec[3];
    float bestdot, dot;
    float min_correlation = (float) (1 - error_factor
        * pow(0.1, MAX_LODS - level));
    int bestfound;
    int vertex_on_edge[4096];
    int numpasses = num_optimization_passes;

    if (!orignumtris) return 0;

    // Allocate the buffers we will be working with.
    tris = malloc(sizeof(*tris) * orignumtris);
beginpass:
    numtris = orignumtris;
    for (i = 0; i < numtris; i++)
    {
        memcpy(&tris[i].tri, origtris + i, sizeof(*origtris));
        tris[i].group = 0;
    }

    // First thing we need to do is divide the triangles up into groups
    // based on their texcoord connections (we can't really optimize over
    // texcoord boundaries).
    highgroup = GroupTriangles(mo, tris, numtris);
    printf("  Number of groups: %i\n", highgroup);

    // Determine which vertices are on group edges.
    memset(vertex_on_edge, 0, sizeof(vertex_on_edge));
    for (i = 0; i < numtris; i++)
    {
        if (!IsValidTexTriangle(&tris[i].tri)) continue;
        // Test each edge.
        for (k = 0; k < 3; k++)
        {
            tc1 = tris[i].tri.index_st[k];
            tc2 = tris[i].tri.index_st[(k+1) % 3];
            if (!IsClockwiseTexTriangle(mo, &tris[i].tri))
            {
                // Make it a clockwise edge.
                m = tc1;
                tc1 = tc2;
                tc2 = m;
            }
            // Is there a neighbor that...?
            // - belongs in the same group
            // - shares the edge's texcoords
            // - has its third vertex on the other side of the edge
            // If there isn't, this is a group edge.
            for (bestfound = false, j = 0; j < numtris && !bestfound; j++)
            {
                if (i == j || tris[i].group != tris[j].group) continue;
                if (!IsValidTexTriangle(&tris[j].tri)) continue;
                //clockwise = IsClockwiseTexTriangle(mo, &tris[j].tri);
                for (found = 0, m = 0; m < 3; m++)
                {
                    // Is this the edge we're looking for?
                    if ((tris[j].tri.index_st[m] == tc1 && tris[j].tri.index_st[(m+1)%3] == tc2)
                        || (tris[j].tri.index_st[m] == tc2 && tris[j].tri.index_st[(m+1)%3] == tc1))
                    {
                        // Try the third vertex.
                        if (!PointOnLineSide(mo->texCoords[tc1].s,
                            mo->texCoords[tc1].t,
                            mo->texCoords[tc2].s,
                            mo->texCoords[tc2].t,
                            mo->texCoords[tris[j].tri.index_st[(m+2)%3]].s,
                            mo->texCoords[tris[j].tri.index_st[(m+2)%3]].t))
                        {
                            bestfound = true;
                            break;
                        }
                    }
                }
            }
            if (!bestfound)
            {
                vertex_on_edge[tc1] = true;
                vertex_on_edge[tc2] = true;
            }
        }
    }

    if (level == 1 && CheckOption("-tcmap"))
    {
        DrawTexCoordMap(mo, tris, numtris, vertex_on_edge);
    }

    // The actual optimization tries to remove individual vertices
    // by merging triangles together.
    memset(vused, 0, sizeof(vused));
    for (i = 0; i < numtris; i++)
    {
        if (!tris[i].group) continue; // This triangle has been removed.
        for (k = 0; k < 3; k++)
        {
            thisvtx = tris[i].tri.index_xyz[k];

            // We can't remove vertices that are at the edge of a group!
            // (edge preservation)
            if (vertex_on_edge[tris[i].tri.index_st[k]]) continue;

            // Check if this vertex has already been processed.
            if (vused[thisvtx]) continue;
            vused[thisvtx] = true; // Now it's used.

            // The Big Question: can this vertex be removed?
            // It can if...
            // - it has suitable neighbors (same group, real XYZ connection)
            // - the merging doesn't alter the surface normals too much

            // The difficult test is to find the suitable neighbors.
            // First find all the triangles connected to this vertex.
            for (numconnected = 0, j = 0; j < numtris; j++)
            {
                if (tris[j].group != tris[i].group) continue;
                if (!HaveSharedTexCoord(&tris[i].tri, &tris[j].tri)) continue;
                for (m = 0; m < 3; m++)
                    if (tris[j].tri.index_xyz[m] == thisvtx)
                    {
                        connected[numconnected++] = j;
                        // Stop if there are too many triangles.
                        if (numconnected == MAX_MERGED) j = numtris;
                        break;
                    }
            }
            // If nothing is connected to this triangle, we can't process
            // it during this stage.
            if (!numconnected) continue;

            // Calculate the average normal vector for the connected
            // triangles.
            memset(connected_normal, 0, sizeof(connected_normal));
            for (j = 0; j < numconnected; j++)
            {
                TriangleNormal(mo, &tris[connected[j]].tri, vec);
                connected_normal[VX] += vec[VX];
                connected_normal[VY] += vec[VY];
                connected_normal[VZ] += vec[VZ];
            }
            Norm(connected_normal);
            bestdot = -1;

            // Figure out the connected vertices.
            for (numconvtx = j = 0; j < numconnected; j++)
            {
                for (t = 0; t < 3; t++)
                {
                    testvtx = tris[connected[j]].tri.index_xyz[t];
                    if (testvtx == thisvtx) continue;
                    // Check if this is already listed.
                    for (found = false, m = 0; m < j; m++)
                        if (convtx[m] == testvtx)
                        {
                            // Already listed, move on.
                            found = true;
                            break;
                        }
                    if (found) continue;
                    contc[numconvtx] = tris[connected[j]].tri.index_st[t];
                    convtx[numconvtx++] = testvtx;
                }
            }

            for (bestfound = false, j = 0; j < numconvtx; j++)
            {
                // Make a copy of the triangles.
                for (m = 0; m < numconnected; m++)
                {
                    memcpy(merged + m, &tris[connected[m]].tri,
                        sizeof(dtriangle_t));
                }
                // Try thisvtx => j.
                nummerged = numconnected;
                for (m = 0; m < nummerged; m++)
                {
                    for (t = 0; t < 3; t++)
                        if (merged[m].index_xyz[t] == thisvtx)
                        {
                            merged[m].index_xyz[t] = convtx[j];
                            merged[m].index_st[t] = contc[j];
                        }
                }
                // Remove triangles that don't have 3 different vertices.
                for (m = 0; m < nummerged; m++)
                {
                    for (found = false, t = 0; t < 3; t++)
                        for (c = 0; c < 3; c++)
                            if (c != t && merged[m].index_xyz[t]
                                == merged[m].index_xyz[c])
                            {
                                found = true;
                            }
                    if (found)
                    {
                        // This triangle doesn't exist any longer.
                        memmove(merged + m, merged + m+1,
                            sizeof(dtriangle_t) * (nummerged - m - 1));
                        nummerged--;
                        m--;
                    }
                }
                if (!nummerged) continue; // Hmm?

                //if (nummerged < numconvtx-4) continue;

                // Now we must check the validity of the 'new' triangles.
                // Calculate the average normal for the merged triangles.
                memset(merged_normal, 0, sizeof(merged_normal));
                for (m = 0; m < nummerged; m++)
                {
                    TriangleNormal(mo, merged + m, vec);
                    merged_normal[VX] += vec[VX];
                    merged_normal[VY] += vec[VY];
                    merged_normal[VZ] += vec[VZ];
                }
                Norm(merged_normal);

                // Calculate the dot product of the average normals.
                for (dot = 0, m = 0; m < 3; m++)
                    dot += merged_normal[m] * connected_normal[m];
                if (dot > bestdot && dot > min_correlation)
                {
                    bestfound = true;
                    bestdot = dot;
                    numbest = nummerged;
                    memcpy(best, merged, sizeof(dtriangle_t) * nummerged);
                }
            }
            if (!bestfound || numbest >= numconnected)
                continue; // Not much point in continuing...

            // Our best choice is now in the 'best' array.
            // Replace the actual connected triangles with it.
            for (j = 0; j < numconnected; j++)
            {
                if (j < numbest)
                {
                    memcpy(&tris[connected[j]].tri, &best[j],
                        sizeof(dtriangle_t));
                }
                else
                {
                    // This triangle is no longer needed.
                    tris[connected[j]].group = 0;
                }
            }
        }
    }

    // We've done as much as we can...
    // Remove all triangles that are in group zero.
    for (m = i = 0; i < numtris; i++)
    {
        if (!tris[i].group) continue;
        memcpy(&origtris[m++], &tris[i].tri, sizeof(tris[i].tri));
    }

    // Should we start another pass?
    if (--numpasses > 0 && m > 0)
    {
        orignumtris = m;
        goto beginpass;
    }
    free(tris);
    return m;
}

//===========================================================================
// BuildLODs
//===========================================================================
void BuildLODs(model_t *mo)
{
    int lod, numtris;

    printf("Building detail levels.\n");
    mo->modified = true;

    if (!mo->lodinfo[0].numTriangles) return; // No triangles at all?!

    // We'll try to build as many LOD levels as we can.
    for (lod = 1; lod < MAX_LODS; lod++)
    {
        printf("(Level %i)\n", lod);

        // We'll start working with level zero data.
        numtris = mo->lodinfo[0].numTriangles;
        memcpy(triangles, mo->lods[0].triangles,
            sizeof(dtriangle_t) * numtris);
        OptimizeTexCoords(mo, triangles, numtris); // Weld texcoords.

        // Take away as many triangles as is suitable for this level.
        numtris = OptimizeMesh(mo, triangles, numtris, lod);

        // Copy them over to the model's LOD data.
        mo->lods[lod].triangles = realloc(mo->lods[lod].triangles,
            sizeof(dmd_triangle_t) * numtris);
        memcpy(mo->lods[lod].triangles, triangles,
            sizeof(dmd_triangle_t) * numtris);
        mo->lodinfo[lod].numTriangles = numtris;

        // Print information about our success.
        printf("  Number of triangles: %-3i (%.2f%% decrease from level zero)\n",
            numtris, (1 - numtris/(float)mo->lodinfo[0].numTriangles)*100);

    }
    // Target reduction levels (minimum): 10%, 30%, 50%
    // See if we reached enough benefit. We can drop levels starting from
    // the last one.


    // Update the model's number of detail levels.
    mo->info.numLODs = lod;

    // Rebuild GL commands, too.
    BuildGlCmds(mo);

    // Now that we have detail information, save as DMD.
    if (mo->header.magic != DMD_MAGIC)
    {
        printf("Detail levels require DMD, changing...\n");
        ModelSetSaveFormat(mo, DMD_MAGIC);
    }
}

//===========================================================================
// ModelRenormalize
//  According to level zero.
//===========================================================================
void ModelRenormalize(model_t *mo)
{
    int tris = mo->lodinfo[0].numTriangles;
    int verts = mo->info.numVertices;
    int i, k, j, n, cnt;
    vector_t *normals, norm;
    vector_t *list;
    dmd_triangle_t *tri;
    dmd_frame_t *fr;
    //float maxprod, diff;
    //int idx;

    cprintf("Calculating new surface normals (  0%%).\b\b\b\b\b\b");
    mo->modified = true;

    // Triangle normals.
    list = malloc(sizeof(*list) * verts);
    normals = malloc(sizeof(*normals) * tris);

    // Calculate the normal for each vertex.
    for (i = 0; i < mo->info.numFrames; i++)
    {
        fr = GetFrame(mo, i);

        cprintf("%3i\b\b\b", mo->info.numFrames > 1? 100 * i
            / (mo->info.numFrames - 1) : 100);

        // Unpack vertices.
        for (k = 0; k < verts; k++)
            for (j = 0; j < 3; j++)
            {
                list[k].pos[j] = fr->vertices[k].vertex[j] * fr->scale[j] +
                    fr->translate[j];
            }

        // First calculate the normal for each triangle.
        for (k = 0, tri = mo->lods[0].triangles; k < tris; k++, tri++)
        {
            CrossProd(list[tri->vertexIndices[0]].pos,
                list[tri->vertexIndices[2]].pos,
                list[tri->vertexIndices[1]].pos, normals[k].pos);
        }

        for (k = 0; k < verts; k++)
        {
            memset(&norm, 0, sizeof(norm));
            for (j = 0, cnt = 0, tri = mo->lods[0].triangles; j < tris;
                j++, tri++)
            {
                tri = mo->lods[0].triangles + j;
                for (n = 0; n < 3; n++)
                    if (tri->vertexIndices[n] == k)
                    {
                        cnt++;
                        for (n = 0; n < 3; n++)
                            norm.pos[n] += normals[j].pos[n];
                        break;
                    }
            }
            if (!cnt) continue;
            // Calculate the average.
            for (n = 0; n < 3; n++) norm.pos[n] /= cnt;
            Norm(norm.pos);
            //fr->vertices[k].lightNormalIndex = GetNormalIndex(norm.pos);
            fr->vertices[k].normal = PackVector(norm.pos);
        }
    }
    free(list);
    free(normals);
    printf("\n");
}

//===========================================================================
// ModelFlipNormals
//  Flips all detail levels.
//===========================================================================
void ModelFlipNormals(model_t *mo)
{
    int lod, i;
    short v;
    dmd_triangle_t *tri;

    printf("Flipping all triangles.\n");
    mo->modified = true;
    for (lod = 0; lod < mo->info.numLODs; lod++)
    {
        for (i = 0, tri = mo->lods[lod].triangles;
            i < mo->lodinfo[lod].numTriangles; i++, tri++)
        {
            v = tri->vertexIndices[1];
            tri->vertexIndices[1] = tri->vertexIndices[2];
            tri->vertexIndices[2] = v;

            v = tri->textureIndices[1];
            tri->textureIndices[1] = tri->textureIndices[2];
            tri->textureIndices[2] = v;
        }
    }
    // Also automatically renormalize.
    ModelRenormalize(mo);
    BuildGlCmds(mo);
}

//===========================================================================
// ReplaceVertex
//===========================================================================
void ReplaceVertex(model_t *mo, int to, int from)
{
    int lod, j, c;

    mo->modified = true;

    // Change all references.
    for (lod = 0; lod < mo->info.numLODs; lod++)
        for (j = 0; j < mo->lodinfo[lod].numTriangles; j++)
            for (c = 0; c < 3; c++)
                if (mo->lods[lod].triangles[j].vertexIndices[c] == from)
                    mo->lods[lod].triangles[j].vertexIndices[c] = to;
}

//===========================================================================
// ModelWeldVertices
//===========================================================================
void ModelWeldVertices(model_t *mo)
{
    int k;
    int i, j;

    printf("Welding vertices...\n");

    if (mo->info.numFrames > 1)
    {
        // Welding is a bit more problematic in the case of multiple
        // frames, because a vertex can't be removed unless it's a
        // duplicate in all frames.

        printf("Model has multiple frames: welding not supported.\n");
        return;
    }

    for (k = 0; k < mo->info.numFrames; ++k)
    {
        dmd_frame_t *frame = &mo->frames[k];

        for (i = 0; i < mo->info.numVertices; ++i)
        {
            for (j = i + 1; j < mo->info.numVertices; ++j)
            {
                dmd_vertex_t *a = &frame->vertices[i];
                dmd_vertex_t *b = &frame->vertices[j];

                if (a->vertex[0] == b->vertex[0] &&
                   a->vertex[1] == b->vertex[1] &&
                   a->vertex[2] == b->vertex[2])
                {
                    printf("Duplicate found: %i and %i.\n", i, j);

                    // Remove the latter one.
                    ReplaceVertex(mo, j, i);

/*                    if (j < mo->info.numVertices - 1)
                    {
                        memmove(&frame->vertices[j], &frame->vertices[j + 1],
                                sizeof(dmd_vertex_t) *
                                (mo->info.numVertices - j - 1));
                                }*/

                    /*--mo->info.numVertices;*/
                }
            }
        }
    }
    // Also automatically renormalize and update GL commands.
    ModelRenormalize(mo);
    BuildGlCmds(mo);
}

//===========================================================================
// MoveTexCoord
//===========================================================================
void MoveTexCoord(model_t *mo, int to, int from)
{
    int lod, j, c;

    mo->modified = true;
    memcpy(mo->texCoords + to, mo->texCoords + from,
        sizeof(dmd_textureCoordinate_t));

    // Change all references.
    for (lod = 0; lod < mo->info.numLODs; lod++)
        for (j = 0; j < mo->lodinfo[lod].numTriangles; j++)
            for (c = 0; c < 3; c++)
                if (mo->lods[lod].triangles[j].textureIndices[c] == from)
                    mo->lods[lod].triangles[j].textureIndices[c] = to;
}

//===========================================================================
// ModelWeldTexCoords
//===========================================================================
void ModelWeldTexCoords(model_t *mo)
{
    int oldnum = mo->info.numTexCoords;
    int i, k, high = 0;
    char refd[4096];
    int numtris = mo->lodinfo[0].numTriangles;
    int numcoords = mo->info.numTexCoords;
    int num_unrefd;
    dtriangle_t *tris = (dtriangle_t*) mo->lods[0].triangles;

    printf("Welding texture coordinates: ");
    OptimizeTexCoords(mo, tris, numtris);

    // First remove all unused texcoords.
    memset(refd, 0, sizeof(refd));
    for (i = 0; i < numtris; i++)
        for (k = 0; k < 3; k++)
            refd[tris[i].index_st[k]] = true;

    // Count how many unreferenced texcoords there are.
    for (num_unrefd = 0, i = 0; i < numcoords; i++)
        if (!refd[i]) num_unrefd++;

    if (num_unrefd)
    {
        printf("%i unused, ", num_unrefd);
        // Let's get rid of them.
        for (i = 0; i < numcoords; i++)
        {
            if (refd[i]) continue;
            // This is a free spot, move a texcoord down here.
            // Find the last used coord.
            for (k = numcoords - 1; k > i; k--) if (refd[k]) break;
            if (k == i) break; // Nothing more can be done.
            refd[i] = true;
            refd[k] = false;
            MoveTexCoord(mo, i, k);
        }
    }

    // Now let's find the highest index in use.
    for (i = 0; i < numcoords; i++)
        if (refd[i]) high = i;
    // Now we know what is the number of the last used index.
    mo->info.numTexCoords = high + 1;
    i = oldnum - mo->info.numTexCoords;
    if (!i)
        printf("no duplicates.\n");
    else
    {
        printf("%i removed.\n", i);
        mo->modified = true;
    }
}

//===========================================================================
// ReadText
//===========================================================================
void ReadText(FILE *file, char *buf, int size)
{
    int i;

    memset(buf, 0, size);
    if (fgets(buf, size - 1, file))
    {
        i = strlen(buf) - 1;
        if (buf[i] == '\n') buf[i] = 0;
    }
}

//===========================================================================
// ModelNewFrame
//===========================================================================
void ModelNewFrame(model_t *mo, const char *name)
{
    int idx = mo->info.numFrames;
    dmd_frame_t *fr;

    mo->modified = true;
    mo->frames = realloc(mo->frames, mo->info.frameSize *
        ++mo->info.numFrames);
    fr = GetFrame(mo, idx);
    memset(fr, 0, mo->info.frameSize);
    strncpy(fr->name, name, 15);
    fr->scale[VX] = fr->scale[VY] = fr->scale[VZ] = 1;
}

//===========================================================================
// ModelCreateFrames
//===========================================================================
void ModelCreateFrames(model_t *mo, const char *listfile)
{
    int cnt = 0;
    FILE *file;
    char line[100];
    const char *ptr;

    printf("Creating new frames according to \"%s\"...\n", listfile);
    mo->modified = true;
    if ((file = fopen(listfile, "rt")) == NULL)
    {
        DoError(MTERR_LISTFILE_NA);
        return;
    }
    for (;;)
    {
        ReadText(file, line, 100);
        ptr = SkipWhite(line);
        if (*ptr && *ptr != ';') // Not an empty line?
        {
            // Create a new frame.
            ModelNewFrame(mo, ptr);
            cnt++;
        }
        if (feof(file)) break;
    }
    fclose(file);

    printf("%i frames were created.\n", cnt);
}

//===========================================================================
// ModelDelFrames
//===========================================================================
void ModelDelFrames(model_t *mo, int from, int to)
{
    int num = mo->info.numFrames;

    mo->modified = true;
    if (from < 0 || from >= num || from > to
        || to < 0 || to >= num)
    {
        DoError(MTERR_INVALID_FRAME_NUMBER);
        return;
    }
    if (from != to)
        printf("Deleting frames from %i to %i.\n", from, to);
    else
        printf("Deleting frame %i.\n", from);

    memmove(GetFrame(mo, from), GetFrame(mo, to + 1),
        mo->info.frameSize * (num - to - 1));
    num -= to - from + 1;
    mo->frames = realloc(mo->frames, mo->info.frameSize * num);
    mo->info.numFrames = num;
}

//===========================================================================
// ModelSetSkin
//===========================================================================
void ModelSetSkin(model_t *mo, int idx, const char *skinfile)
{
    if (idx < 0) DoError(MTERR_INVALID_SKIN_NUMBER);
    printf("Setting skin %i to \"%s\".\n", idx, skinfile);
    mo->modified = true;

    // Are there enough skins allocated?
    if (idx >= mo->info.numSkins)
    {
        // Allocate more skins.
        mo->skins = realloc(mo->skins, sizeof(*mo->skins) * (idx + 1));
        // Clear the new skin names.
        memset(&mo->skins[mo->info.numSkins], 0,
            sizeof(*mo->skins) * (idx + 1 - mo->info.numSkins));
        mo->info.numSkins = idx + 1;
    }
    strcpy(mo->skins[idx].name, skinfile);
}

//===========================================================================
// ModelSetDefaultSkin
//===========================================================================
void ModelSetDefaultSkin(model_t *mo, int idx)
{
    char buf[256], *ptr;

    if (idx < 0) DoError(MTERR_INVALID_SKIN_NUMBER);

    strcpy(buf, mo->fileName);
    ptr = strrchr(buf, '\\');
    if (!ptr) ptr = strrchr(buf, '/');
    if (ptr) memmove(ptr + 1, buf, strlen(ptr));
    ptr = strrchr(buf, '.');
    if (ptr) strcpy(ptr, ".pcx"); else strcat(buf, ".pcx");
    buf[63] = 0; // Can't be longer.
    ModelSetSkin(mo, 0, buf);
}

//===========================================================================
// ModelSetSkinSize
//===========================================================================
void ModelSetSkinSize(model_t *mo, int width, int height)
{
    printf("Setting skin size to %i x %i.\n", width, height);
    mo->info.skinWidth = width;
    mo->info.skinHeight = height;
    mo->modified = true;
}

//===========================================================================
// ModelDelSkin
//===========================================================================
void ModelDelSkin(model_t *mo, int idx)
{
    if (idx < 0 || idx >= mo->info.numSkins)
    {
        DoError(MTERR_INVALID_SKIN_NUMBER);
        return;
    }
    printf("Deleting skin %i (\"%s\").\n", idx, mo->skins[idx].name);

    if (idx < mo->info.numSkins - 1)
    {
        memmove(&mo->skins[idx], &mo->skins[idx + 1],
            sizeof(*mo->skins) * (mo->info.numSkins - idx - 1));
    }
    mo->info.numSkins--;
    mo->skins = realloc(mo->skins, sizeof(*mo->skins)
        * mo->info.numSkins);
    mo->modified = true;
}

//===========================================================================
// ModelSetFileName
//===========================================================================
void ModelSetFileName(model_t *mo, const char *fn)
{
    strcpy(mo->fileName, fn);
    printf("Filename changed to: \"%s\"\n", mo->fileName);
    mo->modified = true;
}

//===========================================================================
// ModelSetSaveFormat
//===========================================================================
void ModelSetSaveFormat(model_t *mo, int magic)
{
    char *newext, *ptr;

    mo->modified = true;
    mo->header.magic = magic;
    if (magic == MD2_MAGIC && mo->info.numLODs > 1)
    {
        printf("Saving as MD2, all levels except level %i will be discarded.\n",
            savelod);
    }
    // Change the extension of the file name.
    newext = (magic == MD2_MAGIC? ".md2" : ".dmd");
    ptr = strrchr(mo->fileName, '.');
    if (!ptr) // What?
        strcat(mo->fileName, newext);
    else
        strcpy(ptr, newext);

    printf("Filename set to: \"%s\"\n", mo->fileName);
}

//===========================================================================
// ModelPrintInfo
//===========================================================================
void ModelPrintInfo(model_t *mo)
{
    int dmd = (mo->header.magic == DMD_MAGIC);
    dmd_info_t *inf = &mo->info;
    int i, k, frame_index, num_cols, per_col;

    printf("--- Information about %s:\n", mo->fileName);
    printf("Format: %s\n", !dmd? "MD2 (Quake II)"
        : "DMD (Detailed/Doomsday Model)");
    printf("Version: %i\n", mo->header.version);
    printf("%i vertices, %i texcoords, %i frames, %i level%s.\n",
        inf->numVertices, inf->numTexCoords, inf->numFrames,
        inf->numLODs, inf->numLODs != 1? "s" : "");
    for (i = 0; i < inf->numLODs; i++)
    {
        printf("Level %i: %i triangles, %i GL commands", i,
            mo->lodinfo[i].numTriangles, mo->lodinfo[i].numGlCommands);
        if (i && mo->lodinfo[0].numTriangles)
        {
            printf(" (%.2f%% reduction).\n",
                100 - mo->lodinfo[i].numTriangles
                / (float) mo->lodinfo[0].numTriangles*100);
        }
        else
        {
            printf(".\n");
        }
    }
    printf("Frames are %i bytes long.\n", dmd? DMD_FRAMESIZE(inf->numVertices)
        : MD2_FRAMESIZE(inf->numVertices));
    printf("Offsets in file: skin=%i txc=%i fr=%i",
        inf->offsetSkins, inf->offsetTexCoords, inf->offsetFrames);
    if (dmd)
    {
        printf(" lodinfo=%i", inf->offsetLODs);
    }
    printf(" end=%i\n", inf->offsetEnd);
    for (i = 0; i < inf->numLODs; i++)
    {
        printf("Level %i offsets: tri=%i gl=%i\n", i,
            mo->lodinfo[i].offsetTriangles,
            mo->lodinfo[i].offsetGlCommands);
    }
    // Print the frame list in three columns.
    printf("Frame list:\n");
    num_cols = 3;
    per_col = (inf->numFrames+2)/num_cols;
    for (i = 0; i < per_col; i++)
    {
        for (k = 0; k < num_cols; k++)
        {
            frame_index = i + k*per_col;
            if (frame_index >= inf->numFrames) break;
            printf(" %3i: %-16s", frame_index,
                GetFrame(mo, frame_index)->name);
        }
        printf("\n");
    }
    printf("%i skin%s of size %ix%i:\n", inf->numSkins,
        inf->numSkins != 1? "s" : "", inf->skinWidth, inf->skinHeight);
    for (i = 0; i < inf->numSkins; i++)
        printf("  %i: %s\n", i, mo->skins[i].name);
}

//===========================================================================
// main
//===========================================================================
int main(int argc, char **argv)
{
    int         i, from, to, skin_num;
    const char  *skin_file, *ptr, *opt;
    model_t     model;
    boolean     nofiles = true;

    PrintBanner();

    // Init cmdline handling.
    myargc = argc;
    myargv = argv;
    argpos = 0;

    // What are we supposed to do?
    if (argc == 1)
    {
        PrintUsage();
        return 0;
    }

    // Scan for all file names.
    for (i = 1; i < argc; i++)
    {
        if (!stricmp(argv[i], "-skin")
            || !stricmp(argv[i], "-skinsize")
            || !stricmp(argv[i], "-delframes"))
        {
            i += 2;
            continue;
        }
        if (!stricmp(argv[i], "-delskin")
            || !stricmp(argv[i], "-del")
            || !stricmp(argv[i], "-create")
            || !stricmp(argv[i], "-s")
            || !stricmp(argv[i], "-savelod")
            || !stricmp(argv[i], "-ef")
            || !stricmp(argv[i], "-op")
            || !stricmp(argv[i], "-fn"))

        {
            i++;
            continue;
        }
        if (argv[i][0] != '-') // Not an option?
        {
            // Process this.
            if (CheckOption("-create"))
            {
                if (!(ptr = NextOption())) DoError(MTERR_INVALID_OPTION);
                ModelNew(&model, argv[i]);
                ModelCreateFrames(&model, ptr);
            }
            else if (!ModelOpen(&model, argv[i])) continue; // No success.
            nofiles = false;
            if (CheckOption("-del"))
            {
                if (!(ptr = NextOption())) DoError(MTERR_INVALID_OPTION);
                from = to = strtoul(ptr, 0, 0);
                ModelDelFrames(&model, from, to);
            }
            if (CheckOption("-delframes"))
            {
                if (!(ptr = NextOption())) DoError(MTERR_INVALID_OPTION);
                if (!(opt = NextOption())) DoError(MTERR_INVALID_OPTION);
                if (!stricmp(ptr, "from"))
                {
                    from = strtoul(opt, 0, 0);
                    to = model.info.numFrames - 1;
                }
                else if (!stricmp(ptr, "to"))
                {
                    from = 0;
                    to = strtoul(opt, 0, 0);
                }
                else
                {
                    from = strtoul(ptr, 0, 0);
                    to = strtoul(opt, 0, 0);
                }
                ModelDelFrames(&model, from, to);
            }
            if (CheckOption("-weld")) ModelWeldVertices(&model);
            if (CheckOption("-weldtc")) ModelWeldTexCoords(&model);
            if (CheckOption("-flip")) ModelFlipNormals(&model);
            if (CheckOption("-renorm")) ModelRenormalize(&model);
            if (CheckOption("-skin"))
            {
                if (!(ptr = NextOption())) DoError(MTERR_INVALID_OPTION);
                skin_num = strtoul(ptr, 0, 0);
                if (!(skin_file = NextOption())) DoError(MTERR_INVALID_OPTION);
                ModelSetSkin(&model, skin_num, skin_file);
            }
            if (CheckOption("-s"))
            {
                if (!(ptr = NextOption())) DoError(MTERR_INVALID_OPTION);
                ModelSetSkin(&model, 0, ptr);
            }
            if (CheckOption("-dsk"))
            {
                ModelSetDefaultSkin(&model, 0);
            }
            if (CheckOption("-skinsize"))
            {
                if (!(ptr = NextOption())) DoError(MTERR_INVALID_OPTION);
                if (!(opt = NextOption())) DoError(MTERR_INVALID_OPTION);
                ModelSetSkinSize(&model, strtoul(ptr, 0, 0),
                    strtoul(opt, 0, 0));
            }
            if (CheckOption("-delskin"))
            {
                if (!(ptr = NextOption())) DoError(MTERR_INVALID_OPTION);
                skin_num = strtoul(ptr, 0, 0);
                ModelDelSkin(&model, skin_num);
            }
            if (CheckOption("-ef"))
            {
                if (!(ptr = NextOption())) DoError(MTERR_INVALID_OPTION);
                error_factor = (float) strtod(ptr, 0);
                printf("Using optimization error factor %.3f.\n", error_factor);
            }
            if (CheckOption("-op"))
            {
                if (!(ptr = NextOption())) DoError(MTERR_INVALID_OPTION);
                num_optimization_passes = strtoul(ptr, 0, 0);
                printf("Using %i mesh optimization passes.\n",
                    num_optimization_passes);
            }
            if (CheckOption("-fn"))
            {
                if (!(ptr = NextOption())) DoError(MTERR_INVALID_OPTION);
                ModelSetFileName(&model, ptr);
            }
            if (CheckOption("-lod")) BuildLODs(&model);
            if (CheckOption("-gl")) BuildGlCmds(&model);
            if (CheckOption("-info")) ModelPrintInfo(&model);
            if (CheckOption("-savelod"))
            {
                if (!(ptr = NextOption())) DoError(MTERR_INVALID_OPTION);
                savelod = strtoul(ptr, 0, 0);
            }
            if (CheckOption("-dmd")) ModelSetSaveFormat(&model, DMD_MAGIC);
            if (CheckOption("-md2")) ModelSetSaveFormat(&model, MD2_MAGIC);
            // We're done, close the model.
            ModelClose(&model);
        }
    }
    if (nofiles) DoError(MTERR_NO_FILES);
    return 0;
}
