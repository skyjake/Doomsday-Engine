//------------------------------------------------------------------------
// GLBSP.H : Interface to Node Builder
//------------------------------------------------------------------------
//
//  GL-Friendly Node Builder (C) 2000-2002 Andrew Apted
//
//  Based on `BSP 2.3' by Colin Reed, Lee Killough and others.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#ifndef __GLBSP_GLBSP_H__
#define __GLBSP_GLBSP_H__


#define GLBSP_VER  "1.96"


// certain GCC attributes can be useful
#ifndef GCCATTR
#define GCCATTR(attr)  /* nothing */
#endif


#ifdef __cplusplus
extern "C" {
#endif // __cplusplus


/* ----- basic types --------------------------- */

typedef char  sint8_g;
typedef short sint16_g;
typedef int   sint32_g;
   
typedef unsigned char  uint8_g;
typedef unsigned short uint16_g;
typedef unsigned int   uint32_g;

typedef double float_g;
typedef double angle_g;  // degrees, 0 is E, 90 is N

// boolean type
typedef int boolean_g;

#ifndef FALSE
#define FALSE  0
#endif
#ifndef TRUE
#define TRUE   1
#endif


/* ----- complex types --------------------------- */

// Node Build Information Structure 
//
// Memory note: when changing the string values here (and in
// nodebuildcomms_t) they should be freed using GlbspFree() and
// allocated with GlbspStrDup().  The application has the final
// responsibility to free the strings in here.
// 
typedef struct nodebuildinfo_s
{
  const char *input_file;
  const char *output_file;

  int factor;

  boolean_g no_reject;
  boolean_g no_progress;
  boolean_g mini_warnings;
  boolean_g force_hexen;
  boolean_g pack_sides;
  boolean_g v1_vert;

  boolean_g load_all;
  boolean_g no_gl;
  boolean_g no_normal;
  boolean_g force_normal;
  boolean_g gwa_mode;  
  boolean_g keep_sect;
  boolean_g no_prune;

  int block_limit;

  // private stuff -- values computed in GlbspParseArgs or
  // GlbspCheckInfo that need to be passed to GlbspBuildNodes.

  boolean_g missing_output;
  boolean_g same_filenames;
}
nodebuildinfo_t;

// This is for two-way communication (esp. with the GUI).
// Should be flagged `volatile' since multiple threads (real or
// otherwise, e.g. signals) may read or change the values.
//
typedef struct nodebuildcomms_s
{
  // if the node builder failed, this will contain the error
  const char *message;

  // the GUI can set this to tell the node builder to stop
  boolean_g cancelled;
}
nodebuildcomms_t;


// Display Prototypes
typedef enum
{
  DIS_INVALID,        // Nonsense value is always useful
  DIS_BUILDPROGRESS,  // Build Box, has 2 bars
  DIS_FILEPROGRESS,   // File Box, has 1 bar
  NUMOFGUITYPES
}
displaytype_e;

// Callback functions
typedef struct nodebuildfuncs_s
{
  // Fatal errors are called as a last resort when something serious
  // goes wrong, e.g. out of memory.  This routine should show the
  // error to the user and abort the program.
  // 
  void (* fatal_error)(const char *str, ...);

  // The print_msg routine is used to display the various messages
  // that occur, e.g. "Building GL nodes on MAP01" and that kind of
  // thing.
  // 
  void (* print_msg)(const char *str, ...);

  // This routine is called frequently whilst building the nodes, and
  // can be used to keep a GUI responsive to user input.  Many
  // toolkits have a "do iteration" or "check events" type of function
  // that this can call.  Avoid anything that sleeps though, or it'll
  // slow down the build process unnecessarily.
  //
  void (* ticker)(void);

  // These display routines is used for tasks that can show a progress
  // bar, namely: building nodes, loading the wad, and saving the wad.
  // The command line version could show a percentage value, or even
  // draw a bar using characters.
 
  // Display_open is called at the beginning, and `type' holds the
  // type of progress (and determines how many bars to display).
  // Returns TRUE if all went well, or FALSE if it failed (in which
  // case the other routines should do nothing when called).
  // 
  boolean_g (* display_open)(displaytype_e type);

  // For GUI versions this can be used to set the title of the
  // progress window.  OK to ignore it (e.g. command line version).
  //
  void (* display_setTitle)(const char *str);

  // The next three routines control the appearance of each progress
  // bar.  Display_setBarText is called to change the message above
  // the bar.  Display_setBarLimit sets the integer limit of the
  // progress (the target value), and display_setBar sets the current
  // value (which will count up from 0 to the limit, inclusive).
  // 
  void (* display_setBar)(int barnum, int count);
  void (* display_setBarLimit)(int barnum, int limit);
  void (* display_setBarText)(int barnum, const char *str);

  // The display_close routine is called when the task is finished,
  // and should remove the progress indicator/window from the screen.
  //
  void (* display_close)(void);
}
nodebuildfuncs_t;

// Default build info and comms
extern const nodebuildinfo_t default_buildinfo;
extern const nodebuildcomms_t default_buildcomms;


/* -------- engine prototypes ----------------------- */

typedef enum
{
  // everything went peachy keen
  GLBSP_E_OK = 0,

  // an unknown error occurred (this is the catch-all value)
  GLBSP_E_Unknown,

  // the arguments were bad/inconsistent.
  GLBSP_E_BadArgs,

  // the info was bad/inconsistent, but has been fixed
  GLBSP_E_BadInfoFixed,

  // file errors
  GLBSP_E_ReadError,
  GLBSP_E_WriteError,

  // building was cancelled
  GLBSP_E_Cancelled
}
glbsp_ret_e;

// parses the arguments, modifying the `info' structure accordingly.
// Returns GLBSP_E_OK if all went well, otherwise another error code.
// Upon error, comms->message may be set to an string describing the
// error.  Typical errors are unrecognised options and invalid option
// values.  Calling this routine is not compulsory.  Note that the set
// of arguments does not include the program's name.
//
glbsp_ret_e GlbspParseArgs(nodebuildinfo_t *info,
    volatile nodebuildcomms_t *comms,
    const char ** argv, int argc);

// checks the node building parameters in `info'.  If they are valid,
// returns GLBSP_E_OK, otherwise an error code is returned.  This
// routine should always be called shortly before GlbspBuildNodes().
// Note that when `output_file' is NULL, this routine will silently
// update it to the correct GWA filename (and set the gwa_mode flag).  
//
// If the GLBSP_E_BadInfoFixed error code is returned, the parameter
// causing the problem has been changed.  You could either treat it as
// a fatal error, or alternatively keep calling the routine in a loop
// until something different than GLBSP_E_BadInfoFixed is returned,
// showing the user the returned messages (e.g. as warnings or in
// pop-up dialog boxes).
//
glbsp_ret_e GlbspCheckInfo(nodebuildinfo_t *info,
    volatile nodebuildcomms_t *comms);

// main routine, this will build the nodes (GL and/or normal) for the
// given input wad file out to the given output file.  Returns
// GLBSP_E_OK if everything went well, otherwise another error code.
// Typical errors are fubar parameters (like input_file == NULL),
// problems reading/writing files, or cancellation by another thread
// (esp. the GUI) using the comms->cancelled flag.  Upon errors, the
// comms->message field usually contains a string describing it.
//
glbsp_ret_e GlbspBuildNodes(const nodebuildinfo_t *info,
    const nodebuildfuncs_t *funcs, 
    volatile nodebuildcomms_t *comms);

// string memory routines.  These should be used for all strings
// shared between the main glBSP code and the UI code (including code
// using glBSP as a plug-in).  They accept NULL pointers.
//
const char *GlbspStrDup(const char *str);
void GlbspFree(const char *str);


#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* __GLBSP_GLBSP_H__ */
