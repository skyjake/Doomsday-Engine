# Doomsday Engine

This is the source code for Doomsday Engine: a portable, enhanced source port of id Software's Doom I/II and Raven Software's Heretic and Hexen. The sources are under the GNU General Public license (see doomsday/gpl-3.0.txt), with the exception of the Doomsday 2 libraries that are under the GNU Lesser General Public License (see doomsday/lgpl-3.0.txt).

For [compilation instructions](https://manual.dengine.net/devel/compilation) and other details, see the [Doomsday Manual](https://manual.dengine.net/).

Linux 64-bit [![Linux Build Status](https://travis-ci.org/skyjake/Doomsday-Engine.svg)](https://travis-ci.org/skyjake/Doomsday-Engine) Windows 32-bit [![Windows Build Status](https://ci.appveyor.com/api/projects/status/79h7egw7q225gj2h?svg=true)](https://ci.appveyor.com/project/skyjake/doomsday-engine)

## Libraries

**libcore** is the core of Doomsday 3. It is a C++ class framework containing functionality such as the file system, plugin loading, Doomsday Script, network communications, and generic data structures. Almost everything relies or will rely on this core library.

**libgui** builds on the core library to add low-level GUI capabilities such as OpenGL graphics, fonts, images, and input devices. It also contains the Doomsday UI framework: widgets, generic dialogs, and abstract data models.

**libgloom** contains the renderer.

**libdoomsday** is an application-level library that contains shared functionality for the client, server, and extensions.

**libdoomsdaygui** extends libdoomsday with GUI-only functionality like widgets shared by the client and shell apps.

**libgamekit** contains the game extension libraries: Doom, Heretic, and Hexen.

## Dependencies

### CMake

Doomsday is compiled using [CMake](http://cmake.org/). Version 3.1 or later is required.

### SDL 2

[SDL 2](http://libsdl.org) provides window system integration (windows, graphics API initialization, input events, game controllers). Additionally, SDL2_mixer can be used for audio output.

### the_Foundation

[the_Foundation](https://git.skyjake.fi/skyjake/the_Foundation/) is a C library for low-level functionality such as multithreading and Unicode text processing. Use the `build_deps.py` script to download and compile this.

### GLM

[OpenGL Mathematics](https://glm.g-truc.net/) library. Use the `build_deps.py` script to download and compile this.

### Open Asset Import Library

libgui requires the [Open Asset Import Library](http://assimp.sourceforge.net/lib_html/index.html) for reading 3D model and animation files. Use the `build_deps.py` script to download and compile this.

### FMOD Studio

The optional FMOD audio plugin requires the [FMOD Studio Low-Level Programmer API](http://www.fmod.org/download).

### FluidSynth

FluidSynth is used for rendering MIDI music using software instruments. FluidSynth can be built as part of the main Doomsday build, in which case GLib 2 is also required as a dependency.


