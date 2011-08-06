/**\file sys_filein.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2010 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
#include "m_md5.h"

#include "filedirectory.h"
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
    ddstring_t* source; // Full path name.
    ddstring_t* target; // Full path name.
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

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static lumpdirectory_record_t* lumpDirectoryRecordForId(lumpdirectoryid_t id);
static void resetVDirectoryMappings(void);
static void clearLumpDirectory(void);

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

    VERBOSE( Con_Message("addLumpDirectoryMapping: \"%s\" -> %s\n", rec->lumpName,
        F_PrettyPath(Str_Text(&rec->path))) )
    }
}

static fileidentifier_t* findFileIdentifierForId(fileidentifierid_t id)
{
    uint i = 0;
    while(i < numReadFiles)
    {
        if(!memcmp(readFiles[i].hash, id, FILEIDENTIFIERID_T_LASTINDEX))
            return &readFiles[i];
        ++i;
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
            Str_Delete(vdMappings[i].source);
            Str_Delete(vdMappings[i].target);
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

void F_ResetFileIds(void)
{
    numReadFiles = 0;
}

void F_GenerateFileId(const char* str, byte identifier[16])
{
    md5_ctx_t context;
    ddstring_t absPath;

    // First normalize the name.
    Str_Init(&absPath); Str_Set(&absPath, str);
    F_MakeAbsolute(&absPath, &absPath);
    F_FixSlashes(&absPath, &absPath);

#if defined(WIN32) || defined(MACOSX)
    // This is a case insensitive operation.
    strupr(Str_Text(&absPath));
#endif

    md5_init(&context);
    md5_update(&context, (byte*) Str_Text(&absPath), (unsigned int) Str_Length(&absPath));
    md5_final(&context, identifier);

    Str_Free(&absPath);
}

boolean F_CheckFileId(const char* path)
{
    assert(path);
    {
    fileidentifierid_t id;

    if(!F_Access(path))
        return false;

    // Calculate the identifier.
    F_GenerateFileId(path, id);

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

        readFiles = realloc(readFiles, sizeof(*readFiles) * maxReadFiles);
        memset(readFiles + numReadFiles, 0, sizeof(*readFiles) * (maxReadFiles - numReadFiles));
    }

    memcpy(readFiles[numReadFiles - 1].hash, id, sizeof(id));
    return true;
    }
}

boolean F_ReleaseFileId(const char* path)
{
    fileidentifierid_t id;
    fileidentifier_t* fileIdentifier;
    // Calculate the identifier.
    F_GenerateFileId(path, id);
    if((fileIdentifier = findFileIdentifierForId(id)) != 0)
    {
        size_t index = fileIdentifier - readFiles;
        if(index < numReadFiles)
            memmove(readFiles + index, readFiles + index + 1, numReadFiles - index - 1);
        memset(readFiles + numReadFiles, 0, sizeof(*readFiles));
        --numReadFiles;
        return true;
    }
    return false;
}

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

void F_AddResourcePathMapping(const char* source, const char* destination)
{
    ddstring_t* src, *dst;
    vdmapping_t* vd;

    // Convert to absolute path names.
    src = Str_Set(Str_New(), source);
    Str_Strip(src);
    F_FixSlashes(src, src);
    if(DIR_SEP_CHAR != Str_RAt(src, 0))
        Str_AppendChar(src, DIR_SEP_CHAR);
    F_ExpandBasePath(src, src);
    F_PrependWorkPath(src, src);

    dst = Str_Set(Str_New(), destination);
    Str_Strip(dst);
    F_FixSlashes(dst, dst);
    if(DIR_SEP_CHAR != Str_RAt(dst, 0))
        Str_AppendChar(dst, DIR_SEP_CHAR);
    F_ExpandBasePath(dst, dst);
    F_PrependWorkPath(dst, dst);

    // Allocate more memory if necessary.
    if(++vdMappingsCount > vdMappingsMax)
    {
        vdMappingsMax *= 2;
        if(vdMappingsMax < vdMappingsCount)
            vdMappingsMax = 2*vdMappingsCount;

        vdMappings = (vdmapping_t*) realloc(vdMappings, sizeof(*vdMappings) * vdMappingsMax);
        if(NULL == vdMappings)
            Con_Error("F_AddResourcePathMapping: Failed on allocation of %lu bytes for mapping list.",
                (unsigned long) (sizeof(*vdMappings) * vdMappingsMax));
    }

    // Fill in the info into the array.
    vd = &vdMappings[vdMappingsCount - 1];
    vd->source = src;
    vd->target = dst;

    VERBOSE( Con_Message("Resources in \"%s\" now mapped to \"%s\"\n", Str_Text(vd->source), Str_Text(vd->target)) );
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
            F_FixSlashes(&lpm.path, &lpm.path);
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

void F_InitializeResourcePathMap(void)
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
            F_AddResourcePathMapping(Argv(i + 1), Argv(i + 2));
            i += 2;
        }
    }}
}

void F_InitDirec(void)
{
    static boolean inited = false;
    size_t bufSize = 0;
    char* buf = NULL;

    if(inited)
    {   // Free old paths, if any.
        clearLumpDirectory();
        memset(lumpDirectory, 0, sizeof(lumpDirectory));
    }

    // Add the contents of all DD_DIREC lumps.
    { lumpnum_t i;
    for(i = 0; i < W_LumpCount(); ++i)
    {
        size_t lumpLength;

        if(strnicmp(W_LumpName(i), "DD_DIREC", 8))
            continue;

        // Make a copy of it so we can ensure it ends in a null.
        lumpLength = W_LumpLength(i);
        if(bufSize < lumpLength + 1)
        {
            bufSize = lumpLength + 1;
            buf = (char*) realloc(buf, bufSize);
            if(NULL == buf)
                Con_Error("F_InitDirec: Failed on (re)allocation of %lu bytes for temporary read buffer.", (unsigned long) bufSize);
        }

        W_ReadLump(i, buf);
        buf[lumpLength] = 0;
        parseLumpDirectoryMap(buf);
    }}

    if(NULL != buf)
        free(buf);

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

DFILE* F_OpenLump(lumpnum_t lumpNum, boolean dontBuffer)
{
    DFILE* file;

    if(lumpNum < 0 || lumpNum >= W_LumpCount())
        return NULL;

    file = F_GetFreeFile();
    if(!file)
        return NULL;

    // Init and load in the lumpNum data.
    file->flags.open = true;
    file->flags.file = false;
    file->lastModified = time(NULL); // So I'm lazy...
    if(!dontBuffer)
    {
        const char* lump = W_CacheLump(lumpNum, PU_APPSTATIC);

        file->size = W_LumpLength(lumpNum);
        file->data = (void*) malloc(file->size);
        if(NULL == file->data)
            Con_Error("F_OpenLump: Failed on allocation of %lu bytes for buffered lump.",
                (unsigned long) file->size);

        memcpy(file->data, (const void*)lump, file->size);
        file->pos = (char*) file->data;
        W_CacheChangeTag(lumpNum, PU_CACHE);
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

/// @return  @c true, if the mapping matched the path.
boolean F_MapPath(ddstring_t* path, vdmapping_t* vd)
{
    assert(NULL != path && NULL != vd);
    if(!strnicmp(Str_Text(path), Str_Text(vd->target), Str_Length(vd->target)))
    {
        // Replace the beginning with the source path.
        ddstring_t temp;
        Str_Init(&temp);
        Str_Set(&temp, Str_Text(vd->source));
        Str_PartAppend(&temp, Str_Text(path), Str_Length(vd->target), Str_Length(path) - Str_Length(vd->target));
        Str_Copy(path, &temp);
        Str_Free(&temp);
        return true;
    }
    return false;
}

DFILE* F_OpenFile(const char* path, const char* mymode)
{
    DFILE* file = F_GetFreeFile();
    char mode[8];

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
        if(vdMappingsCount > 0)
        {
            ddstring_t mapped;
            Str_Init(&mapped);
            // Any applicable virtual directory mappings?
            { uint i;
            for(i = 0; i < vdMappingsCount; ++i)
            {
                Str_Set(&mapped, path);
                if(!F_MapPath(&mapped, &vdMappings[i]))
                    continue;
                // The mapping was successful.
                file->data = (void*)fopen(Str_Text(&mapped), mode);
                if(NULL != file->data)
                {
                    VERBOSE( Con_Message("F_OpenFile: \"%s\" opened as %s.\n",
                        F_PrettyPath(Str_Text(&mapped)), path) )
                    break;
                }
            }}
            Str_Free(&mapped);
        }

        if(!file->data)
        {   // Still can't find it.
            F_Release(file);
            return 0;
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

DFILE* F_Open(const char* path, const char* mode)
{
    boolean dontBuffer, reqRealFile;
    ddstring_t searchPath;
    DFILE* file = NULL;

    if(!path || !path[0])
        return NULL;

    if(NULL == mode) mode = "";

    dontBuffer  = (strchr(mode, 'x') != NULL);
    reqRealFile = (strchr(mode, 'f') != NULL);

    // Make it a full path.
    Str_Init(&searchPath); Str_Set(&searchPath, path);
    F_FixSlashes(&searchPath, &searchPath);
    F_ExpandBasePath(&searchPath, &searchPath);

    // Shall we first check the Zip directory?
    if(!reqRealFile)
    {
        zipindex_t zipIndex = Zip_Find(Str_Text(&searchPath));
        if(0 != zipIndex)
        {
            file = F_OpenZip(zipIndex, dontBuffer);
            if(NULL != file)
            {
                Str_Free(&searchPath);
                return file;
            }
        }
    }

    // For the next stage we must have an absolute path, so prepend the current
    // working directory if necessary.
    F_PrependWorkPath(&searchPath, &searchPath);

    // How about the dir/WAD redirects?
    if(!reqRealFile)
    {
        lumpdirectory_record_t* rec;
        int i = 0;
        for(i = 0; !Str_IsEmpty(&lumpDirectory[i].path); ++i)
        {
            rec = &lumpDirectory[i];
            if(!Str_CompareIgnoreCase(&rec->path, Str_Text(&searchPath)))
            {
                lumpnum_t lumpNum = W_CheckLumpNumForName(rec->lumpName);
                file = F_OpenLump(lumpNum, dontBuffer);
                break;
            }
        }
    }

    // Try to open as a real file then?
    if(NULL == file)
    {
        file = F_OpenFile(Str_Text(&searchPath), mode);
    }

    Str_Free(&searchPath);
    return file;
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

void F_Init(void)
{
    Zip_Init();
    W_Init();
}

void F_Shutdown(void)
{
    Zip_Shutdown();
}

void F_EndStartup(void)
{
    Zip_EndStartup();
    W_EndStartup();
}

int F_Reset(void)
{
    return Zip_Reset() + W_Reset();
}

typedef struct {
    /// Callback to make for each processed node.
    int (*callback) (const ddstring_t* path, pathdirectory_nodetype_t type, void* paramaters);

    /// Data passed to the callback.
    void* paramaters;

    /// Current search pattern.
    const ddstring_t* pattern;
} findzipfileworker_paramaters_t;

typedef struct foundentry_s {
    ddstring_t path;
    int attrib;
} foundentry_t;

static int C_DECL compareFoundEntryByPath(const void* a, const void* b)
{
    return Str_CompareIgnoreCase(&((const foundentry_t*)a)->path, Str_Text(&((const foundentry_t*)b)->path));
}

/// Collect a list of paths including those which have been mapped.
static foundentry_t* collectLocalPaths(const ddstring_t* searchPath, int* retCount)
{
    ddstring_t wildPath, origWildPath;
    foundentry_t* found = NULL;
    int count = 0, max = 0;
    finddata_t fd;

    Str_Init(&origWildPath);
    Str_Appendf(&origWildPath, "%s*", Str_Text(searchPath));

    Str_Init(&wildPath);
    { int i;
    for(i = -1; i < (int)vdMappingsCount; ++i)
    {
        Str_Copy(&wildPath, &origWildPath);

        // Possible mapping?
        if(i >= 0 && !F_MapPath(&wildPath, &vdMappings[i]))
            continue; // Not mapped.

        if(!myfindfirst(Str_Text(&wildPath), &fd))
        {   // First path found.           
            do
            {
                // Ignore relative directory symbolics.
                if(strcmp(fd.name, ".") && strcmp(fd.name, ".."))
                {
                    if(count >= max)
                    {
                        if(0 == max)
                            max = 16;
                        else
                            max *= 2;
                        found = realloc(found, sizeof(*found) * max);
                    }
                    Str_Init(&found[count].path);
                    Str_Set(&found[count].path, fd.name);
                    if((fd.attrib & A_SUBDIR) && DIR_SEP_CHAR != Str_RAt(&found[count].path, 0))
                        Str_AppendChar(&found[count].path, DIR_SEP_CHAR);
                    found[count].attrib = fd.attrib;
                    ++count;
                }
            } while(!myfindnext(&fd));
        }
        myfindend(&fd);
    }}

    Str_Free(&origWildPath);
    Str_Free(&wildPath);

    if(retCount)
        *retCount = count;
    if(0 != count)
        return found;
    return NULL;
}

static int iterateLocalPaths(const ddstring_t* pattern, const ddstring_t* searchPath,
    int (*callback) (const ddstring_t* path, pathdirectory_nodetype_t type, void* paramaters),
    void* paramaters)
{
    assert(pattern && searchPath && !Str_IsEmpty(searchPath) && callback);
    {
    int result = 0, count;
    foundentry_t* foundPaths = collectLocalPaths(searchPath, &count);

    if(NULL != foundPaths)
    {
        ddstring_t path, localPattern;

        // Sort all the foundPaths entries.
        qsort(foundPaths, count, sizeof(foundentry_t), compareFoundEntryByPath);

        Str_Init(&localPattern);
        Str_Appendf(&localPattern, "%s%s", Str_Text(searchPath), Str_Text(pattern));

        Str_Init(&path);
        {int i;
        for(i = 0; i < count; ++i)
        {
            // Is the caller's iteration still in progress?
            if(0 == result)
            {
                // Compose the full path to the found file/directory.
                Str_Clear(&path);
                Str_Appendf(&path, "%s%s", Str_Text(searchPath), Str_Text(&foundPaths[i].path));

                // Does this match the pattern?
                if(F_MatchName(Str_Text(&path), Str_Text(&localPattern)))
                {
                    // Pass this path to the caller.
                    result = callback(&path, (foundPaths[i].attrib & A_SUBDIR)? PT_BRANCH : PT_LEAF, paramaters);
                }
            }

            // We're done with this path.
            Str_Free(&foundPaths[i].path);
        }}
        Str_Free(&path);
        Str_Free(&localPattern);
        free(foundPaths);
    }

    return result;
    }
}

static int findZipFileWorker(const ddstring_t* zipFileName, void* paramaters)
{
    assert(zipFileName && paramaters);
    {
    findzipfileworker_paramaters_t* info = (findzipfileworker_paramaters_t*)paramaters;
    if(F_MatchName(Str_Text(zipFileName), Str_Text(info->pattern)))
    {
        return info->callback(zipFileName, PT_LEAF, info->paramaters);
    }
    return 0; // Continue search.
    }
}

int F_AllResourcePaths2(const char* rawSearchPattern,
    int (*callback) (const ddstring_t* path, pathdirectory_nodetype_t type, void* paramaters),
    void* paramaters)
{
    ddstring_t searchPattern, searchName, searchDirectory;
    int result = 0;

    // First normalize the raw search pattern into one we can process.
    Str_Init(&searchPattern); Str_Set(&searchPattern, rawSearchPattern);
    Str_Strip(&searchPattern);
    F_FixSlashes(&searchPattern, &searchPattern);
    F_ExpandBasePath(&searchPattern, &searchPattern);

    // An absolute path is required so resolve relative to the base path.
    // if not already absolute.
    F_PrependBasePath(&searchPattern, &searchPattern);

    // Check the Zip directory.
    { findzipfileworker_paramaters_t p;
    p.callback = callback;
    p.paramaters = paramaters;
    p.pattern = &searchPattern;
    result = Zip_Iterate2(findZipFileWorker, (void*)&p);
    if(0 != result)
    {   // Find didn't finish.
        goto searchEnded;
    }}

    // Check the dir/WAD direcs.
    { int i;
    for(i = 0; Str_Length(&lumpDirectory[i].path) != 0; ++i)
    {
        lumpdirectory_record_t* rec = &lumpDirectory[i];
        if(!F_MatchName(Str_Text(&rec->path), Str_Text(&searchPattern)))
            continue;
        result = callback(&rec->path, PT_LEAF, paramaters);
        if(0 != result)
            goto searchEnded;
    }}

    /**
     * Check real files on the search path.
     * Our existing normalized search pattern cannot be used as-is due to the
     * interface of the search algorithm requiring that the name and directory of
     * the pattern be specified separately.
     */

    // Extract just the name and/or extension.
    Str_Init(&searchName);
    F_FileNameAndExtension(&searchName, Str_Text(&searchPattern));

    // Extract the directory path.
    Str_Init(&searchDirectory);
    F_FileDir(&searchDirectory, &searchPattern);

    result = iterateLocalPaths(&searchName, &searchDirectory, callback, paramaters);

    Str_Free(&searchName);
    Str_Free(&searchDirectory);

searchEnded:
    Str_Free(&searchPattern);
    return result;
}

int F_AllResourcePaths(const char* searchPath,
    int (*callback) (const ddstring_t* path, pathdirectory_nodetype_t type, void* paramaters))
{
    return F_AllResourcePaths2(searchPath, callback, 0);
}
