# Doomsday Engine

This is the source code for Doomsday Engine: a portable, enhanced source port of id Software's Doom I/II and Raven Software's Heretic and Hexen. The sources are under the GNU General Public license (see doomsday/gpl-3.0.txt), with the exception of the Doomsday 2 libraries that are under the GNU Lesser General Public License (see doomsday/lgpl-3.0.txt).

For [compilation instructions](http://dengine.net/dew/index.php?title=Compilation) and other details, see the documentation wiki: http://dengine.net/dew/

## Libraries

**libcore** is the core of Doomsday 2. It is a C++ class framework containing functionality such as the file system, plugin loading, Doomsday Script, network communications, and generic data structures. Almost everything relies or will rely on this core library.

**liblegacy** is a collection of C language routines extracted from the old Doomsday 1 code base. Its purpose is to (eventually) act as a C wrapper for libcore. (Game plugins are mostly in C.)

**libgui** builds on libcore to add low-level GUI capabilities such as OpenGL graphics, fonts, images, and input devices.

**libappfw** contains the Doomsday UI framework: widgets, generic dialogs, abstract data models. libappfw is built on libgui and libcore.

**libshell** has functionality related to connecting to and controlling Doomsday servers remotely.

## External Dependencies

### Qt

Using Qt 5 is recommended. The minimum required version of Qt is 4.8. See [Supported platforms](http://dengine.net/dew/index.php?title=Supported_platforms) in the wiki for details about which version is being used on which platform.

### Open Asset Import Library

libgui requires the [Open Asset Import Library](http://assimp.sourceforge.net/lib_html/index.html) for reading 3D model and animation files.

1. Clone https://github.com/skyjake/assimp.
2. Check out the "deng-patches" branch.
3. Run [cmake](http://cmake.org) to generate appropriate project files (e.g., Visual Studio on Windows).
4. Compile the generated project.
5. Add `ASSIMP_DIR` to your *config_user.pri*.

### SDL 2

[SDL 2](http://libsdl.org) is needed for game controller input (e.g., joysticks and gamepads). Additionally, SDL2_mixer can be used for audio output (not required).

### FMOD Ex

The optional FMOD audio plugin requires the [FMOD Ex Programmer's API](http://fmod.org/).

## Branches

The following branches are currently active in the repository.

- **master**: Main code base. This is where releases are made from on a biweekly basis. Bug fixing is done in this branch, while larger development efforts occur in separate work branches.
- **stable**: Latest stable release. Patch releases can be made from this branch when necessary.
- **stable-x.y.z**: Stable release x.y.z.
- **legacy**: Old stable code base. Currently at the 1.8.6 release.
