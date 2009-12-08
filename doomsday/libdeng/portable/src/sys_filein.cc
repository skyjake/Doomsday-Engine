/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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

/**
 * sys_filein.c: File (Input) Stream Abstraction Layer
 *
 * File input. Can read from real files or WAD lumps. Note that
 * reading from WAD lumps means that a copy is taken of the lump when
 * the corresponding 'file' is opened. With big files this uses
 * considerable memory and time.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_platform.h"

#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

#include "doomsday.h"
#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

#include "sys_findfile.h"

// MACROS ------------------------------------------------------------------

#define MAX_LUMPDIRS    1024

// TYPES -------------------------------------------------------------------

typedef struct {
    DFILE          *file;
} filehandle_t;

typedef struct {
    char            lump[9];
    char           *path; // Full path name.
} lumpdirec_t;

typedef struct {
    char           *source; // Full path name.
    char           *target; // Full path name.
} vdmapping_t;

typedef struct {
    f_forall_func_t func;
    void           *parm;
    const char     *searchPath;
    const char     *pattern;
} zipforall_t;

typedef struct foundentry_s {
    filename_t      name;
    int             attrib;
} foundentry_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void F_ResetMapping(void);
void F_ResetDirec(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static lumpdirec_t direc[MAX_LUMPDIRS + 1];
static filehandle_t* files;
static unsigned int filesCount;

static vdmapping_t* vdMappings;
static unsigned int vdMappingsCount;
static unsigned int vdMappingsMax;

// CODE --------------------------------------------------------------------

/**
 * This is a case-insensitive test.
 * I do hope this algorithm works like it should...
 *
 * @return              @c true, if the string matches the pattern.
 */
int F_MatchName(const char *string, const char *pattern)
{
    const char     *in = string, *st = pattern;

    while(*in)
    {
        if(*st == '*')
        {
            st++;
            continue;
        }

        if(*st != '?' &&
           (tolower((unsigned char) *st) != tolower((unsigned char) *in)))
        {
            // A mismatch. Hmm. Go back to a previous *.
            while(st >= pattern && *st != '*')
                st--;

            if(st < pattern)
                return false; // No match!

            // The asterisk lets us continue.
        }

        // This character of the pattern is OK.
        st++;
        in++;
    }

    // Match is good if the end of the pattern was reached.
    while(*st == '*')
        st++; // Skip remaining asterisks.

    return *st == 0;
}

/**
 * Skips all whitespace except newlines.
 */
char *F_SkipSpace(char *ptr)
{
    while(*ptr && *ptr != '\n' && isspace(*ptr))
        ptr++;

    return ptr;
}

char *F_FindNewline(char *ptr)
{
    while(*ptr && *ptr != '\n')
        ptr++;

    return ptr;
}

int F_GetDirecIdx(char *exact_path)
{
    int                 i;
    boolean             found;

    for(i = 0, found = false; direc[i].path && !found; ++i)
        if(!stricmp(direc[i].path, exact_path))
            found = true;

    if(found)
        return i;
    else
        return -1;
}

/**
 * The path names are converted to full paths before adding to the table.
 */
void F_AddDirec(const char* lumpname, char* symbolic_path, size_t len)
{
    int                 i;
    filename_t          path;
    char*               full;
    lumpdirec_t*        ld;

    if(!lumpname[0] || !symbolic_path[0])
        return;

    for(i = 0; direc[i].path && i < MAX_LUMPDIRS; ++i);

    if(i == MAX_LUMPDIRS)
    {
        // \fixme Why no dynamic allocation?
        Con_Error("F_AddDirec: Not enough direcs (%s).\n", symbolic_path);
    }
    ld = direc + i;

    // Convert the symbolic path into a real path.
    Dir_FixSlashes(symbolic_path, len);
    if(symbolic_path[0] == DIR_SEP_CHAR)
        snprintf(path, FILENAME_T_MAXLEN, "%s%s", ddBasePath,
                 symbolic_path + 1);
    else
        snprintf(path, FILENAME_T_MAXLEN, "%s%s", ddRuntimeDir.path,
                 symbolic_path);

    // Since the basepath might be relative, let's explicitly make
    // the path absolute. _fullpath will allocate memory, which will
    // be freed in F_ShutdownDirec() (also via F_InitDirec()).
    full = _fullpath(0, path, 0);

    // If this path already exists, we'll just update the lump name.
    i = F_GetDirecIdx(full);
    if(i >= 0)
    {
        // Already exists!
        M_Free(full);
        ld = direc + i;
        full = ld->path;
    }
    ld->path = full;
    strcpy(ld->lump, lumpname);
    strupr(ld->lump);

    if(verbose)
    {
        Con_Message("F_AddDirec: %s -> %s\n", ld->lump, ld->path);
    }
}

/**
 * The path names are converted to full paths before adding to the table.
 * Files in the source directory are mapped to the target directory.
 */
void F_AddMapping(const char* source, const char* destination)
{
    filename_t          src;
    filename_t          dst;
    vdmapping_t*        vd;

    // Convert to absolute path names.
    M_TranslatePath(src, source, FILENAME_T_MAXLEN);
    Dir_ValidDir(src, FILENAME_T_MAXLEN);
    Dir_MakeAbsolute(src, FILENAME_T_MAXLEN);

    M_TranslatePath(dst, destination, FILENAME_T_MAXLEN);
    Dir_ValidDir(dst, FILENAME_T_MAXLEN);
    Dir_MakeAbsolute(dst, FILENAME_T_MAXLEN);

    // Allocate more memory if necessary.
    if(++vdMappingsCount > vdMappingsMax)
    {
        vdMappingsMax *= 2;
        if(vdMappingsMax < vdMappingsCount)
            vdMappingsMax = 2*vdMappingsCount;

        vdMappings = (vdmapping_t*)
            M_Realloc(vdMappings, sizeof(vdmapping_t) * vdMappingsMax);
    }

    // Fill in the info into the array.
    vd = &vdMappings[vdMappingsCount - 1];
    vd->source = strdup(src);
    vd->target = strdup(dst);

    VERBOSE(Con_Message("F_AddMapping: %s mapped to %s.\n",
                        vd->source, vd->target));
}

/**
 * LUMPNAM0 \Path\In\The\Base.ext
 * LUMPNAM1 Path\In\The\RuntimeDir.ext
 *  :
 */
void F_ParseDirecData(char* buffer)
{
    char*               ptr = buffer, *end;
    char                name[9];
    filename_t          path;
    size_t              len;

    for(; *ptr;)
    {
        ptr = F_SkipSpace(ptr);
        if(!*ptr)
            break;

        if(*ptr == '\n')
        {
            ptr++; // Advance to the next line.
            continue;
        }

        // We're at the lump name.
        end = M_FindWhite(ptr);
        if(!*end)
            break;

        len = end - ptr;
        if(len > 8)
            len = 8;
        memset(name, 0, sizeof(name));
        strncpy(name, ptr, len);
        ptr = F_SkipSpace(end);
        if(!*ptr || *ptr == '\n')
            continue; // Missing filename?

        // We're at the path name.
        end = F_FindNewline(ptr);

        // Get rid of extra whitespace.
        while(end > ptr && isspace(*end))
            end--;
        end = M_FindWhite(end);

        len = end - ptr;
        if(len > FILENAME_T_MAXLEN)
            len = FILENAME_T_MAXLEN;

        memset(path, 0, sizeof(path));
        strncpy(path, ptr, len);
        F_AddDirec(name, path, len);
        ptr = end;
    }
}

void F_InitMapping(void)
{
    int                 i, argC = Argc();

    F_ResetMapping();

    // Create virtual directory mappings by processing all -vdmap
    // options.
    for(i = 0; i < argC; ++i)
    {
        if(strnicmp("-vdmap", Argv(i), 6))
            continue; // This is not the option we're looking for.

        if(i < argC - 1 && !ArgIsOption(i + 1) && !ArgIsOption(i + 2))
        {
            F_AddMapping(Argv(i + 1), Argv(i + 2));
            i += 2;
        }
    }
}

/**
 * Initialize the WAD/dir translations. Called after WADs have been read.
 */
void F_InitDirec(void)
{
    static boolean      alreadyInited = false;
    int                 i;
    size_t              len;
    char*               buf;

    if(alreadyInited)
    {
        // Free old paths, if any.
        F_ResetDirec();
        memset(direc, 0, sizeof(direc));
    }

    // Add the contents of all DD_DIREC lumps.
    for(i = 0; i < numLumps; ++i)
    {
        if(!strnicmp(W_LumpName(i), "DD_DIREC", 8))
        {
            // Make a copy of it so we can make it end in a null.
            len = W_LumpLength(i);
            buf = (char*) M_Malloc(len + 1);
            memcpy(buf, W_CacheLumpNum(i, PU_CACHE), len);
            buf[len] = 0;

            F_ParseDirecData(buf);
            M_Free(buf);
        }
    }

    alreadyInited = true;
}

void F_ResetMapping(void)
{
    uint                i;

    if(vdMappings)
    {
        // Free the allocated memory.
        for(i = 0; i < vdMappingsCount; ++i)
        {
            M_Free(vdMappings[i].source);
            M_Free(vdMappings[i].target);
        }
        M_Free(vdMappings);
    }
    vdMappings = NULL;
    vdMappingsCount = vdMappingsMax = 0;
}

void F_ResetDirec(void)
{
    size_t              i;

    for(i = 0; direc[i].path; ++i)
        if(direc[i].path)
            M_Free(direc[i].path); // Allocated by _fullpath.
}

void F_CloseAll(void)
{
    uint                i;

    if(files)
    {
        for(i = 0; i < filesCount; ++i)
            if(files[i].file != NULL)
            {
                F_Close(files[i].file);
            }

        M_Free(files);
    }
    files = NULL;
    filesCount = 0;
}

void F_ShutdownDirec(void)
{
    F_ResetMapping();
    F_ResetDirec();
    F_CloseAll();
}

/**
 * Returns true if the file can be opened for reading.
 */
int F_Access(const char *path)
{
    // Open for reading, but don't buffer anything.
    DFILE              *file = F_Open(path, "rx");

    if(!file)
        return false;
    F_Close(file);
    return true;
}

DFILE *F_GetFreeFile(void)
{
    uint                i, oldCount;
    boolean             found;
    filehandle_t       *fhdl;

    for(i = 0, found = false, fhdl = files; i < filesCount && !found;
        fhdl++, ++i)
    {
        if(fhdl->file == NULL)
            found = true;
    }

    if(found)
    {
        fhdl->file = (DFILE*) M_Calloc(sizeof(DFILE));
        return fhdl->file;
    }

    oldCount = filesCount;

    // Allocate more memory.
    filesCount *= 2;
    if(filesCount < 16)
        filesCount = 16;

    files = (filehandle_t*) M_Realloc(files, sizeof(filehandle_t) * filesCount);

    // Clear the new handles.
    for(i = oldCount; i < filesCount; ++i)
    {
        memset(&files[i], 0, sizeof(filehandle_t));
    }

    files[oldCount].file = (DFILE*) M_Calloc(sizeof(DFILE));
    return files[oldCount].file;
}

/**
 * Frees the memory allocated to the handle.
 */
void F_Release(DFILE *file)
{
    uint                i;

    if(!file)
        return;

    if(files)
    {
        // Clear references to the handle.
        for(i = 0; i < filesCount; ++i)
            if(files[i].file == file)
                files[i].file = NULL;
    }

    // File was allocated in F_GetFreeFile.
    M_Free(file);
}

DFILE *F_OpenLump(const char *name, boolean dontBuffer)
{
    int                 num = W_CheckNumForName((char *) name);
    DFILE              *file;

    if(num < 0)
        return NULL;
    file = F_GetFreeFile();
    if(!file)
        return NULL;

    // Init and load in the lump data.
    file->flags.open = true;
    file->flags.file = false;
    file->lastModified = time(NULL); // So I'm lazy...
    if(!dontBuffer)
    {
        file->size = W_LumpLength(num);
        file->data = (char*) M_Malloc(file->size);
        file->pos = (char*) file->data;
        memcpy(file->data, W_CacheLumpNum(num, PU_CACHE), file->size);
    }

    return file;
}

/**
 * This only works on real files.
 */
static unsigned int F_GetLastModified(const char *path)
{
#ifdef UNIX
    struct stat s;
    stat(path, &s);
    return s.st_mtime;
#endif

#ifdef WIN32
    struct _stat s;
    _stat(path, &s);
    return s.st_mtime;
#endif
}

/**
 * @return              @c true, if the mapping matched the path.
 */
boolean F_MapPath(char* mapped, const char* path, vdmapping_t* vd, size_t len)
{
    size_t              targetLen = strlen(vd->target);

    if(!strnicmp(path, vd->target, targetLen))
    {
        // Replace the beginning with the source path.
        strncpy(mapped, vd->source, len);
        strncat(mapped, path + targetLen, len);
        return true;
    }

    return false;
}

DFILE* F_OpenFile(const char* path, const char* mymode)
{
    DFILE*              file = F_GetFreeFile();
    char                mode[8];
    uint                i;
    filename_t          mapped;

    if(!file)
        return NULL;

    strcpy(mode, "r"); // Open for reading.
    if(strchr(mymode, 't'))
        strcat(mode, "t");
    if(strchr(mymode, 'b'))
        strcat(mode, "b");

    // Try opening as a real file.
    file->data = fopen(path, mode);
    if(!file->data)
    {
        // Any applicable virtual directory mappings?
        for(i = 0; i < vdMappingsCount; ++i)
        {
            if(F_MapPath(mapped, path, &vdMappings[i], FILENAME_T_MAXLEN))
            {
                // The mapping was successful.
                if((file->data = fopen(mapped, mode)) != NULL)
                {
                    // Success!
                    VERBOSE(Con_Message("F_OpenFile: %s opened as %s.\n",
                                        mapped, path));
                    break;
                }
            }
        }

        if(!file->data)
        {
            // Still can't find it.
            F_Release(file);
            return NULL; // Can't find the file.
        }
    }

    file->flags.open = true;
    file->flags.file = true;
    file->lastModified = F_GetLastModified(path);
    return file;
}

/**
 * Zip data is buffered like lump data.
 */
DFILE* F_OpenZip(zipindex_t zipIndex, boolean dontBuffer)
{
    DFILE*              file = F_GetFreeFile();

    if(!file)
        return NULL;

    // Init and load in the lump data.
    file->flags.open = true;
    file->flags.file = false;
    file->lastModified = Zip_GetLastModified(zipIndex);
    if(!dontBuffer)
    {
        file->size = Zip_GetSize(zipIndex);
        file->data = (char*) M_Malloc(file->size);
        file->pos = (char*) file->data;
        Zip_Read(zipIndex, file->data);
    }

    return file;
}

/**
 * Opens the given file (will be translated) or lump for reading.
 * "t" = text mode (with real files, lumps are always binary)
 * "b" = binary
 * "f" = must be a real file
 * "w" = file must be in a WAD
 * "x" = just test for access (don't buffer anything)
 */
DFILE* F_Open(const char* path, const char* mode)
{
    int                 i;
    filename_t          trans, full;
    boolean             dontBuffer;

    if(!mode)
        mode = "";
    dontBuffer = (strchr(mode, 'x') != NULL);

    // Make it a full path.
    M_TranslatePath(trans, path, FILENAME_T_MAXLEN);
    _fullpath(full, trans, 255);

    // Lumpdirecs take precedence.
    if(!strchr(mode, 'f')) // Doesn't need to be a real file?
    {
        // First check the Zip directory.
        zipindex_t          foundZip = Zip_Find(full);

        if(foundZip)
            return F_OpenZip(foundZip, dontBuffer);

        for(i = 0; direc[i].path; ++i)
            if(!strnicmp(full, direc[i].path, FILENAME_T_MAXLEN))
                return F_OpenLump(direc[i].lump, dontBuffer);
    }

    if(strchr(mode, 'w'))
        return NULL; // Must be in a WAD...

    // Try to open as a real file, then.
    return F_OpenFile(full, mode);
}

void F_Close(DFILE *file)
{
    if(!file)
        return; // Wha?

    if(!file->flags.open)
        return;

    if(file->flags.file)
    {
        fclose((FILE*)file->data);
    }
    else
    {
        // Free the stored data.
        if(file->data)
            M_Free(file->data);
    }
    memset(file, 0, sizeof(*file));

    F_Release(file);
}

/**
 * @return              The number of bytes read (up to 'count').
 */
size_t F_Read(void *dest, size_t count, DFILE *file)
{
    size_t              bytesleft;

    if(!file->flags.open)
        return 0;

    if(file->flags.file)
    {   // Normal file.
        count = fread(dest, 1, count, (FILE*)file->data);
        if(feof((FILE *) file->data))
            file->flags.eof = true;
        return count;
    }

    // Is there enough room in the file?
    bytesleft = file->size - (file->pos - (char *) file->data);
    if(count > bytesleft)
    {
        count = bytesleft;
        file->flags.eof = true;
    }

    if(count)
    {
        memcpy(dest, file->pos, count);
        file->pos += count;
    }

    return count;
}

unsigned char F_GetC(DFILE *file)
{
    unsigned char       ch = 0;

    if(!file->flags.open)
        return 0;
    F_Read(&ch, 1, file);
    return ch;
}

size_t F_Tell(DFILE *file)
{
    if(!file->flags.open)
        return 0;
    if(file->flags.file)
        return (size_t) ftell((FILE*)file->data);
    return file->pos - (char *) file->data;
}

/**
 * @return              The current position in the file, before the move,
 *                      as an offset from the beginning of the file.
 */
size_t F_Seek(DFILE *file, size_t offset, int whence)
{
    size_t              oldpos = F_Tell(file);

    if(!file->flags.open)
        return 0;

    file->flags.eof = false;
    if(file->flags.file)
    {
        fseek((FILE*)file->data, (long) offset, whence);
    }
    else
    {
        if(whence == SEEK_SET)
            file->pos = (char *) file->data + offset;
        else if(whence == SEEK_END)
            file->pos = (char *) file->data + (file->size + offset);
        else if(whence == SEEK_CUR)
            file->pos += offset;
    }

    return oldpos;
}

void F_Rewind(DFILE *file)
{
    F_Seek(file, 0, SEEK_SET);
}

/**
 * \note Stream position is not affected.
 *
 * @return              The length of the file, in bytes.
 */
size_t F_Length(DFILE *file)
{
    size_t              length, currentPosition;

    if(!file)
        return 0;

    currentPosition = F_Seek(file, 0, SEEK_END);
    length = F_Tell(file);
    F_Seek(file, currentPosition, SEEK_SET);
    return length;
}

/**
 * @return              The time when the file was last modified, as seconds
 *                      since the Epoch else zero if the file is not found.
 */
unsigned int F_LastModified(const char* fileName)
{
    // Try to open the file, but don't buffer any contents.
    DFILE*              file = F_Open(fileName, "rx");
    unsigned int        modified = 0;

    if(!file)
        return 0;

    modified = file->lastModified;
    F_Close(file);
    return modified;
}

/**
 * @return              The number of times the char appears in the path.
 */
size_t F_CountPathChars(const char* path, char ch)
{
    size_t              count;

    for(count = 0; *path; path++)
        if(*path == ch)
            count++;

    return count;
}

/**
 * @return              @c true, to indicate to stop searching, when forall_func
 *                      returns @c false.
 */
int F_ZipFinderForAll(const char* zipFileName, void* parm)
{
    zipforall_t*        info = (zipforall_t*) parm;

    if(F_MatchName(zipFileName, info->pattern))
    {
        if(!info->func(zipFileName, FT_NORMAL, info->parm))
        {
            return true; // Stop searching.
        }
    }

    // Continue searching.
    return false;
}

static int C_DECL F_EntrySorter(const void* a, const void* b)
{
    return stricmp(((const foundentry_t*)a)->name,
                   ((const foundentry_t*)b)->name);
}

/**
 * Descends into 'physical' subdirectories.
 */
int F_ForAllDescend(const char* pattern, const char* path, void* parm,
                    f_forall_func_t func)
{
    int                 i, count, max;
    finddata_t          fd;
    foundentry_t*       found;
    filename_t          fn, spec, localPattern;

    snprintf(localPattern, FILENAME_T_MAXLEN, "%s%s", path, pattern);

    // Collect a list of paths.  The list contains files in all the
    // paths mapped to the current path.
    count = 0;
    max = 16;
    found = (foundentry_t*) M_Malloc(max * sizeof(*found));

    for(i = -1; i < (int)vdMappingsCount; ++i)
    {
        snprintf(spec, FILENAME_T_MAXLEN, "%s*", path);

        if(i >= 0)
        {
            filename_t          mapped;

            // Possible virtual mapping.
            if(!F_MapPath(mapped, spec, &vdMappings[i], FILENAME_T_MAXLEN))
            {
                // The mapping didn't match this one.
                continue;
            }
            strncpy(spec, mapped, FILENAME_T_MAXLEN);

            // Also map the local pattern.
            //F_MapPath(localPattern, &vdMappings[i], mapped);
            //strncpy(localPattern, mapped, FILENAME_T_MAXLEN);
        }

        if(!myfindfirst(spec, &fd))
        {
            // The first file found!
            do
            {
                // Ignore the relative directory names.
                if(strcmp(fd.name, ".") && strcmp(fd.name, ".."))
                {
                    if(count >= max)
                    {
                        max *= 2;
                        found = (foundentry_t*) M_Realloc(found, sizeof(*found) * max);
                    }
                    memset(&found[count], 0, sizeof(*found));
                    strncpy(found[count].name, fd.name, FILENAME_T_MAXLEN);
                    found[count].attrib = fd.attrib;
                    count++;
                }
            } while(!myfindnext(&fd));
        }
        myfindend(&fd);
    }

    // Sort all the found entries.
    qsort(found, count, sizeof(foundentry_t), F_EntrySorter);

    for(i = 0; i < count; ++i)
    {
        // Compile the full pathname of the found file.
        strncpy(fn, path, FILENAME_T_MAXLEN);
        strncat(fn, found[i].name, FILENAME_T_MAXLEN);

        // Descend recursively into subdirectories.
        if(found[i].attrib & A_SUBDIR)
        {
            strncat(fn, DIR_SEP_STR, FILENAME_T_MAXLEN);
            if(!F_ForAllDescend(pattern, fn, parm, func))
            {
                M_Free(found);
                return false;
            }
        }
        else
        {
            // Does this match the pattern?
            if(F_MatchName(fn, localPattern))
            {
                // If the callback returns false, stop immediately.
                if(!func(fn, FT_NORMAL, parm))
                {
                    M_Free(found);
                    return false;
                }
            }
        }
    }

    // Free the memory allocate for the list of found entries.
    M_Free(found);
    return true;
}

/**
 * Parm is passed on to the callback, which is called for each file
 * matching the filespec. Absolute path names are given to the callback.
 * Zip directory, DD_DIREC and the real files are scanned.
 */
int F_ForAll(const char* filespec, void* parm, f_forall_func_t func)
{
    int                 i;
    filename_t          fn;
    directory_t         specdir;
    zipforall_t         zipFindInfo;

    memset(&specdir, 0, sizeof(specdir));

    Dir_FileDir(filespec, &specdir);

    // First check the Zip directory.
    _fullpath(fn, filespec, FILENAME_T_MAXLEN);
    zipFindInfo.func = func;
    zipFindInfo.parm = parm;
    zipFindInfo.pattern = fn;
    zipFindInfo.searchPath = specdir.path;

    if(Zip_Iterate(F_ZipFinderForAll, &zipFindInfo))
    {
        // Find didn't finish.
        return false;
    }

    // Check through the dir/WAD direcs.
    for(i = 0; direc[i].path; ++i)
        if(F_MatchName(direc[i].path, fn))
            if(!func(direc[i].path, FT_NORMAL, parm))
                //if(!F_ForAllCaller(specdir.path, direc[i].path, func, parm))
                return false;

    Dir_FileName(fn, filespec, FILENAME_T_MAXLEN);

    if(!F_ForAllDescend(fn, specdir.path, parm, func))
        return false;

    return true;
}
