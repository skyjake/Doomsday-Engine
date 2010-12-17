/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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
 * File (Input) Stream Abstraction Layer.
 *
 * File input. Can read from real files or WAD lumps. Note that
 * reading from WAD lumps means that a copy is taken of the lump when
 * the corresponding 'file' is opened. With big files this uses
 * considerable memory and time.
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

#include "de_platform.h"
#include "de_base.h"
#include "de_console.h"
#include "de_system.h"

#include "m_args.h"
#include "m_misc.h" // \todo remove dependency

#include "sys_findfile.h"

// MACROS ------------------------------------------------------------------

#define LUMPDIRECTORY_MAXRECORDS    1024

// TYPES -------------------------------------------------------------------

typedef int lumpdirectoryid_t;

typedef struct {
    lumpname_t lumpName;
    ddstring_t path; // Full path name.
} lumppathmapping_t;

typedef lumppathmapping_t lumpdirectory_record_t;

typedef struct {
    char* source; // Full path name.
    char* target; // Full path name.
} vdmapping_t;

typedef struct {
    DFILE* file;
} filehandle_t;

#define FILEIDENTIFIERID_T_MAXLEN 16
#define FILEIDENTIFIERID_T_LASTINDEX 15
typedef byte fileidentifierid_t[FILEIDENTIFIERID_T_MAXLEN];

typedef struct fileidentifier_s {
    fileidentifierid_t hash;
} fileidentifier_t;

typedef struct {
    /// Callback to make for each processed node.
    f_forall_func_t callback;

    /// Data passed to the callback.
    void* paramaters;

    /// Current path being searched.
    const ddstring_t* searchPath;

    /// Current search pattern.
    const ddstring_t* pattern;
} findzipfileworker_paramaters_t;

typedef struct foundentry_s {
    filename_t name;
    int attrib;
} foundentry_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void clearLumpDirectory(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static lumpdirectory_record_t* lumpDirectoryRecordForId(lumpdirectoryid_t id);

static void resetVDirectoryMappings(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static lumpdirectory_record_t lumpDirectory[LUMPDIRECTORY_MAXRECORDS + 1];

static filehandle_t* files;
static uint filesCount;

static uint numReadFiles = 0;
static uint maxReadFiles = 0;
static fileidentifier_t* readFiles = 0;

static vdmapping_t* vdMappings;
static uint vdMappingsCount;
static uint vdMappingsMax;

// CODE --------------------------------------------------------------------

static __inline void initLumpPathMapping(lumppathmapping_t* lpm)
{
    assert(lpm);
    Str_Init(&lpm->path);
    memset(lpm->lumpName, 0, sizeof(lpm->lumpName));
}

static __inline void clearLumpPathMapping(lumppathmapping_t* lpm)
{
    Str_Free(&lpm->path);
    memset(lpm->lumpName, 0, sizeof(lpm->lumpName));
}

static lumpdirectoryid_t getUnusedLumpDirectoryId(void)
{
    lumpdirectoryid_t id;
    // \fixme Why no dynamic allocation?
    for(id = 0; Str_Length(&lumpDirectory[id].path) != 0 && id < LUMPDIRECTORY_MAXRECORDS; ++id);
    if(id == LUMPDIRECTORY_MAXRECORDS)
    {
        Con_Error("getUnusedLumpDirectoryId: Not enough records.\n");
    }
    return id;
}

static int toLumpDirectoryId(const char* path)
{
    if(path && path[0])
    {
        int i;
        for(i = 0; Str_Length(&lumpDirectory[i].path) != 0; ++i)
        {
            lumpdirectory_record_t* rec = &lumpDirectory[i];
            if(!Str_CompareIgnoreCase(&lumpDirectory[i].path, path))
                return i;
        }
    }
    return -1;
}

static lumpdirectory_record_t* newLumpDirectoryRecord(lumpdirectoryid_t id)
{
    return lumpDirectoryRecordForId(id);
}

static lumpdirectory_record_t* lumpDirectoryRecordForId(lumpdirectoryid_t id)
{
    if(id >= 0 && id < LUMPDIRECTORY_MAXRECORDS)
        return &lumpDirectory[id];
    return 0;
}

static void clearLumpDirectory(void)
{
    int i;
    for(i = 0; Str_Length(&lumpDirectory[i].path) != 0; ++i)
    {
        lumpdirectory_record_t* rec = &lumpDirectory[i];
        Str_Free(&rec->path);
    }
}

/**
 * The path names are converted to full paths before adding to the table.
 */
static void addLumpDirectoryMapping(const char* lumpName, const ddstring_t* symbolicPath)
{
    assert(lumpName && symbolicPath);
    {
    size_t symbolicPathLength = Str_Length(symbolicPath);
    lumpdirectory_record_t* rec;
    ddstring_t fullPath, path;

    if(!lumpName[0] || Str_Length(symbolicPath) == 0)
        return;

    // Convert the symbolic path into a real path.
    Str_Init(&path);
    F_ResolveSymbolicPath(&path, symbolicPath);

    // Since the path might be relative, let's explicitly make the path absolute.
    { char* full;
    Str_Init(&fullPath);
    Str_Set(&fullPath, full = _fullpath(0, Str_Text(&path), 0)); free(full);
    }

    // If this path already exists, we'll just update the lump name.
    rec = lumpDirectoryRecordForId(toLumpDirectoryId(Str_Text(&fullPath)));
    if(!rec)
    {   // Acquire a new record.
        rec = newLumpDirectoryRecord(getUnusedLumpDirectoryId());
        assert(rec);
        Str_Copy(&rec->path, &fullPath);
    }
    memcpy(rec->lumpName, lumpName, sizeof(rec->lumpName));
    rec->lumpName[LUMPNAME_T_LASTINDEX] = '\0';

    Str_Free(&fullPath);
    Str_Free(&path);

    VERBOSE( Con_Message("addLumpDirectoryMapping: \"%s\" -> %s\n", rec->lumpName, Str_Text(F_PrettyPath(&rec->path)) ));
    }
}

static fileidentifier_t* findFileIdentifierForId(fileidentifierid_t id)
{
    uint i = 0;
    while(i < numReadFiles)
    {
        if(!memcmp(readFiles[i].hash, id, 16))
            return &readFiles[i];
        i++;
    }
    return 0;
}

static void resetVDirectoryMappings(void)
{
    if(vdMappings)
    {
        // Free the allocated memory.
        uint i;
        for(i = 0; i < vdMappingsCount; ++i)
        {
            free(vdMappings[i].source);
            free(vdMappings[i].target);
        }
        free(vdMappings); vdMappings = 0;
    }
    vdMappingsCount = vdMappingsMax = 0;
}

static filehandle_t* findUsedFileHandle(void)
{
    uint i;
    for(i = 0; i < filesCount; ++i)
    {
        filehandle_t* fhdl = &files[i];
        if(!fhdl->file)
            return fhdl;
    }
    return 0;
}

static filehandle_t* getFileHandle(void)
{
    filehandle_t* fhdl;
    if(!(fhdl = findUsedFileHandle()))
    {
        // Allocate more memory.
        uint firstNewFile = filesCount;

        filesCount *= 2;
        if(filesCount < 16)
            filesCount = 16;

        files = realloc(files, sizeof(filehandle_t) * filesCount);
        // Clear the new handles.
        { uint i;
        for(i = firstNewFile; i < filesCount; ++i)
        {
            memset(&files[i], 0, sizeof(filehandle_t));
        }}
        fhdl = &files[firstNewFile];
    }
    return fhdl;
}

/**
 * Resets the array of known file IDs. The next time F_CheckFileId() is
 * called on a file, it passes.
 */
void F_ResetFileIDs(void)
{
    numReadFiles = 0;
}

/**
 * Maintains a list of identifiers already seen.
 *
 * @return              @c true, if the given file can be read, or
 *                      @c false, if it has already been read.
 */
boolean F_CheckFileId(const char* path)
{
    assert(path);
    {
    fileidentifierid_t id;

    if(!F_Access(path))
        return false;

    // Calculate the identifier.
    Dir_FileID(path, id);

    if(findFileIdentifierForId(id))
        return false;

    // Allocate a new entry.
    numReadFiles++;
    if(numReadFiles > maxReadFiles)
    {
        if(!maxReadFiles)
            maxReadFiles = 16;
        else
            maxReadFiles *= 2;

        readFiles = realloc(readFiles, sizeof(readFiles[0]) * maxReadFiles);
    }

    memcpy(readFiles[numReadFiles - 1].hash, id, 16);
    return true;
    }
}

/**
 * This is a case-insensitive test.
 * I do hope this algorithm works like it should...
 *
 * @return              @c true, if the string matches the pattern.
 */
int F_MatchName(const char* string, const char* pattern)
{
    const char* in = string, *st = pattern;

    while(*in)
    {
        if(*st == '*')
        {
            st++;
            continue;
        }

        if(*st != '?' && (tolower((unsigned char) *st) != tolower((unsigned char) *in)))
        {
            // A mismatch. Hmm. Go back to a previous '*'.
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

void F_AddMapping(const char* source, const char* destination)
{
    filename_t src, dst;
    vdmapping_t* vd;

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

        vdMappings = realloc(vdMappings, sizeof(vdmapping_t) * vdMappingsMax);
    }

    // Fill in the info into the array.
    vd = &vdMappings[vdMappingsCount - 1];
    vd->source = strdup(src);
    vd->target = strdup(dst);

    VERBOSE( Con_Message("F_AddMapping: %s mapped to %s.\n", vd->source, vd->target) );
}

/// Skip all whitespace except newlines.
static __inline const char* skipSpace(const char* ptr)
{
    assert(ptr);
    while(*ptr && *ptr != '\n' && isspace(*ptr))
        ptr++;
    return ptr;
}

static boolean parseLumpPathMapping(lumppathmapping_t* lpm, const char* buffer)
{
    const char* ptr = buffer, *end;
    size_t len;

    // Find the start of the lump name.
    ptr = skipSpace(ptr);
    if(!*ptr || *ptr == '\n')
    {   // Just whitespace??
        return false;
    }

    // Find the end of the lump name.
    end = M_FindWhite((char*)ptr);
    if(!*end || *end == '\n')
    {
        return false;
    }

    len = end - ptr;
    if(len > 8)
    {   // Invalid lump name.
        return false;
    }

    clearLumpPathMapping(lpm);
    strncpy(lpm->lumpName, ptr, len);

    // Find the start of the file path.
    ptr = skipSpace(end);
    if(!*ptr || *ptr == '\n')
    {   // Missing file path.
        return false;
    }

    // We're at the file path.
    Str_Set(&lpm->path, ptr);
    // Get rid of any extra whitespace on the end.
    Str_StripRight(&lpm->path);
    return true;
}

/**
 * LUMPNAM0 \Path\In\The\Base.ext
 * LUMPNAM1 Path\In\The\RuntimeDir.ext
 *  :
 */
static boolean parseLumpDirectoryMap(const char* buffer)
{
    assert(buffer);
    {
    boolean successful = false;
    lumppathmapping_t lpm;
    ddstring_t line;
    const char* ch;

    initLumpPathMapping(&lpm);

    Str_Init(&line);
    ch = buffer;
    do
    {
        ch = Str_GetLine(&line, ch);
        if(!parseLumpPathMapping(&lpm, Str_Text(&line)))
        {   // Failure parsing the mapping.
            // Ignore errors in individual mappings and continue parsing.
            //goto parseEnded;
        }
        else
        {
            strupr(lpm.lumpName);
            Dir_FixSlashes(Str_Text(&lpm.path), Str_Length(&lpm.path));
            addLumpDirectoryMapping(lpm.lumpName, &lpm.path);
        }
    } while(*ch);

    // Success.
    successful = true;

//parseEnded:
    clearLumpPathMapping(&lpm);
    Str_Free(&line);
    return successful;
    }
}

void F_InitMapping(void)
{
    int argC = Argc();

    resetVDirectoryMappings();

    // Create virtual directory mappings by processing all -vdmap options.
    { int i;
    for(i = 0; i < argC; ++i)
    {
        if(strnicmp("-vdmap", Argv(i), 6))
            continue; // This is not the option we're looking for.

        if(i < argC - 1 && !ArgIsOption(i + 1) && !ArgIsOption(i + 2))
        {
            F_AddMapping(Argv(i + 1), Argv(i + 2));
            i += 2;
        }
    }}
}

void F_InitDirec(void)
{
    static boolean inited = false;

    if(inited)
    {   // Free old paths, if any.
        clearLumpDirectory();
        memset(lumpDirectory, 0, sizeof(lumpDirectory));
    }

    // Add the contents of all DD_DIREC lumps.
    { lumpnum_t i;
    for(i = 0; i < W_NumLumps(); ++i)
    {
        if(strnicmp(W_LumpName(i), "DD_DIREC", 8))
            continue;

        // Make a copy of it so we can ensure it ends in a null.
        { size_t lumpLength = W_LumpLength(i);
        char* buf = malloc(lumpLength+1);
        W_ReadLump(i, buf);
        buf[lumpLength] = 0;
        parseLumpDirectoryMap(buf);
        free(buf);
        }
    }}

    inited = true;
}

void F_CloseAll(void)
{
    if(files)
    {
        uint i;
        for(i = 0; i < filesCount; ++i)
        {
            if(files[i].file)
                F_Close(files[i].file);
        }
        free(files); files = 0;
    }
    filesCount = 0;
}

void F_ShutdownDirec(void)
{
    resetVDirectoryMappings();
    clearLumpDirectory();
    F_CloseAll();
}

/**
 * Returns true if the file can be opened for reading.
 */
int F_Access(const char* path)
{
    // Open for reading, but don't buffer anything.
    DFILE* file;
    if((file = F_Open(path, "rx")))
    {
        F_Close(file);
        return true;
    }
    return false;
}

DFILE* F_GetFreeFile(void)
{
    filehandle_t* fhdl = getFileHandle();
    fhdl->file = calloc(1, sizeof(DFILE));
    return fhdl->file;
}

/**
 * Frees the memory allocated to the handle.
 */
void F_Release(DFILE* file)
{
    if(!file)
        return;

    if(files)
    {   // Clear references to the handle.
        uint i;
        for(i = 0; i < filesCount; ++i)
        {
            if(files[i].file == file)
                files[i].file = NULL;
        }
    }

    // File was allocated in F_GetFreeFile.
    free(file);
}

DFILE* F_OpenLump(const char* name, boolean dontBuffer)
{
    int num = W_CheckNumForName((char *) name);
    DFILE* file;

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
        file->pos = file->data = malloc(file->size);
        memcpy(file->data, W_CacheLumpNum(num, PU_CACHE), file->size);
    }

    return file;
}

/**
 * This only works on real files.
 */
static unsigned int F_GetLastModified(const char* path)
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
    size_t targetLen = strlen(vd->target);

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
    DFILE* file = F_GetFreeFile();
    char mode[8];
    filename_t mapped;

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
        { uint i;
        for(i = 0; i < vdMappingsCount; ++i)
        {
            if(!F_MapPath(mapped, path, &vdMappings[i], FILENAME_T_MAXLEN))
                continue;

            // The mapping was successful.
            if((file->data = fopen(mapped, mode)) != NULL)
            {
                // Success!
                VERBOSE( Con_Message("F_OpenFile: %s opened as %s.\n", mapped, path) );
                break;
            }
        }}

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
    DFILE* file = F_GetFreeFile();

    if(!file)
        return NULL;

    // Init and load in the lump data.
    file->flags.open = true;
    file->flags.file = false;
    file->lastModified = Zip_GetLastModified(zipIndex);
    if(!dontBuffer)
    {
        file->size = Zip_GetSize(zipIndex);
        file->pos = file->data = malloc(file->size);
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
    filename_t trans, full;
    boolean dontBuffer;

    if(!mode)
        mode = "";
    dontBuffer = (strchr(mode, 'x') != NULL);

    // Make it a full path.
    M_TranslatePath(trans, path, FILENAME_T_MAXLEN);
    _fullpath(full, trans, 255);

    if(!strchr(mode, 'f')) // Doesn't need to be a real file?
    {
        // First check the Zip directory.
        { zipindex_t foundZip;
        if((foundZip = Zip_Find(full)) != 0)
            return F_OpenZip(foundZip, dontBuffer);
        }

        // Check through the dir/WAD direcs.
        { int i;
        for(i = 0; Str_Length(&lumpDirectory[i].path) != 0; ++i)
        {
            lumpdirectory_record_t* rec = &lumpDirectory[i];
            if(!Str_CompareIgnoreCase(&rec->path, full))
                return F_OpenLump(rec->lumpName, dontBuffer);
        }}
    }

    if(strchr(mode, 'w'))
        return NULL; // Must be in a WAD...

    // Try to open as a real file, then.
    return F_OpenFile(full, mode);
}

void F_Close(DFILE* file)
{
    if(!file)
        return; // Wha?

    if(!file->flags.open)
        return;

    if(file->flags.file)
    {
        fclose(file->data);
    }
    else
    {   // Free the stored data.
        if(file->data)
            free(file->data);
    }
    memset(file, 0, sizeof(*file));

    F_Release(file);
}

/**
 * @return              The number of bytes read (up to 'count').
 */
size_t F_Read(void* dest, size_t count, DFILE* file)
{
    size_t bytesleft;

    if(!file->flags.open)
        return 0;

    if(file->flags.file)
    {   // Normal file.
        count = fread(dest, 1, count, file->data);
        if(feof((FILE *) file->data))
            file->flags.eof = true;
        return count;
    }

    // Is there enough room in the file?
    bytesleft = file->size - (file->pos - (char*) file->data);
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

unsigned char F_GetC(DFILE* file)
{
    unsigned char ch = 0;

    if(!file->flags.open)
        return 0;
    F_Read(&ch, 1, file);
    return ch;
}

size_t F_Tell(DFILE* file)
{
    if(!file->flags.open)
        return 0;
    if(file->flags.file)
        return (size_t) ftell(file->data);
    return file->pos - (char *) file->data;
}

/**
 * @return              The current position in the file, before the move,
 *                      as an offset from the beginning of the file.
 */
size_t F_Seek(DFILE* file, size_t offset, int whence)
{
    size_t oldpos = F_Tell(file);

    if(!file->flags.open)
        return 0;

    file->flags.eof = false;
    if(file->flags.file)
    {
        fseek(file->data, (long) offset, whence);
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

void F_Rewind(DFILE* file)
{
    F_Seek(file, 0, SEEK_SET);
}

/**
 * \note Stream position is not affected.
 *
 * @return              The length of the file, in bytes.
 */
size_t F_Length(DFILE* file)
{
    size_t length, currentPosition;

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
    DFILE* file = F_Open(fileName, "rx");
    unsigned int modified = 0;

    if(!file)
        return 0;

    modified = file->lastModified;
    F_Close(file);
    return modified;
}

int C_DECL F_CompareFoundEntryByName(const void* a, const void* b)
{
    return stricmp(((const foundentry_t*)a)->name, ((const foundentry_t*)b)->name);
}

/**
 * Descends into 'physical' subdirectories.
 */
static int forAllDescend(const ddstring_t* pattern, const ddstring_t* path,
    f_forall_func_t callback, void* paramaters)
{
    assert(pattern && path && callback);
    {
    int count, max, result = 0;
    foundentry_t* found;
    filename_t localPattern;
    ddstring_t filename;
    finddata_t fd;

    dd_snprintf(localPattern, FILENAME_T_MAXLEN, "%s%s", Str_Text(path), Str_Text(pattern));

    // Collect a list of file paths including those which have been mapped.
    count = 0;
    max = 16;
    found = malloc(max * sizeof(*found));

    { int i;
    for(i = -1; i < (int)vdMappingsCount; ++i)
    {
        filename_t spec;

        dd_snprintf(spec, FILENAME_T_MAXLEN, "%s*", Str_Text(path));

        if(i >= 0)
        {
            filename_t mapped;
            // Possible virtual mapping.
            if(!F_MapPath(mapped, spec, &vdMappings[i], FILENAME_T_MAXLEN))
            {   // Not mapped.
                continue;
            }
            strncpy(spec, mapped, FILENAME_T_MAXLEN);
        }

        if(!myfindfirst(spec, &fd))
        {
            // The first file found!
            do
            {
                // Ignore relative directory symbolics.
                if(strcmp(fd.name, ".") && strcmp(fd.name, ".."))
                {
                    if(count >= max)
                    {
                        max *= 2;
                        found = realloc(found, sizeof(*found) * max);
                    }
                    memset(&found[count], 0, sizeof(*found));
                    strncpy(found[count].name, fd.name, FILENAME_T_MAXLEN);
                    found[count].attrib = fd.attrib;
                    count++;
                }
            } while(!myfindnext(&fd));
        }
        myfindend(&fd);
    }}

    // Sort all the found entries.
    qsort(found, count, sizeof(foundentry_t), F_CompareFoundEntryByName);

    Str_Init(&filename);
    { int i;
    for(i = 0; i < count; ++i)
    {
        // Compile the full pathname of the found file.
        Str_Clear(&filename);
        Str_Appendf(&filename, "%s%s", Str_Text(path), found[i].name);

        // Descend recursively into subdirectories.
        if(found[i].attrib & A_SUBDIR)
        {
            Str_AppendChar(&filename, DIR_SEP_CHAR);
            if((result = forAllDescend(pattern, &filename, callback, paramaters)) != 0)
            {
                break;
            }
        }
        else
        {
            // Does this match the pattern?
            if(F_MatchName(Str_Text(&filename), localPattern))
            {
                // If the callback returns false, stop immediately.
                if((result = callback(&filename, FT_NORMAL, paramaters)) != 0)
                {
                    break;
                }
            }
        }
    }}

    // Free the memory allocate for the list of found entries.
    free(found);
    Str_Free(&filename);
    return result;
    }
}

int F_FindZipFileWorker(const ddstring_t* zipFileName, void* paramaters)
{
    assert(zipFileName && paramaters);
    {
    findzipfileworker_paramaters_t* info = (findzipfileworker_paramaters_t*)paramaters;
    if(F_MatchName(Str_Text(zipFileName), Str_Text(info->pattern)))
    {
        return info->callback(zipFileName, FT_NORMAL, info->paramaters);
    }
    return 0; // Continue search.
    }
}

int F_ForAll2(const ddstring_t* fileSpec, f_forall_func_t callback, void* paramaters)
{
    directory2_t specdir;
    ddstring_t temp, filename;
    int result = 0;

    memset(&specdir, 0, sizeof(specdir));
    Str_Init(&specdir.path);
    
    Str_Init(&temp);
    F_ExpandBasePath(&temp, fileSpec);
    F_FileDir(&temp, &specdir);

    { filename_t fn;
    _fullpath(fn, Str_Text(&temp), FILENAME_T_MAXLEN);
    Str_Init(&filename); Str_Set(&filename, fn);
    }

    // First the Zip directory.
    { findzipfileworker_paramaters_t p;
    p.callback = callback;
    p.paramaters = paramaters;
    p.searchPath = &specdir.path;
    p.pattern = &filename;
    if((result = Zip_Iterate2(F_FindZipFileWorker, (void*)&p)) != 0)
    {   // Find didn't finish.
        goto searchEnded;
    }}

    // Check through the dir/WAD direcs.
    { int i;
    for(i = 0; Str_Length(&lumpDirectory[i].path) != 0; ++i)
    {
        lumpdirectory_record_t* rec = &lumpDirectory[i];
        if(!F_MatchName(Str_Text(&rec->path), Str_Text(&filename)))
            continue;
        if((result = callback(&rec->path, FT_NORMAL, paramaters)) != 0)
            goto searchEnded;
    }}

    { char ext[100];
    _splitpath(Str_Text(&temp), 0, 0, Str_Text(&filename), ext);
    Str_Append(&filename, ext);
    }

    // Finally, check real files on the search path.
    result = forAllDescend(&filename, &specdir.path, callback, paramaters);

searchEnded:
    Str_Free(&specdir.path);
    Str_Free(&temp);
    Str_Free(&filename);
    return result;
}

int F_ForAll(const ddstring_t* fileSpec, f_forall_func_t callback)
{
    return F_ForAll2(fileSpec, callback, 0);
}
