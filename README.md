# The Doomsday Engine Project Repository

This is the source code for Doomsday Engine: a portable, enhanced source port of id Software's Doom I/II and Raven Software's Heretic and Hexen. The sources are under the GNU General Public license (see doomsday/engine/doc/LICENSE).

For compilation instructions and other details, see the documentation wiki: http://dengine.net/dew/

## Branches

The following branches are currently active in the repository.

- **master**: Main code base. This is where releases are made from on a bi-weekly basis. Bug fixing is done in this branch, while larger development efforts occur in separate work branches.
- **ringzero+master**: Merge of the master branch and DaniJ's ringzero branch. Scheduled to merge with master before the stable 1.9.7.
- **legacy**: Old stable code base. Currently at the 1.8.6 release.

Other branches (not very active):

- **dsfmod**: Development branch for the FMOD Ex audio plugin.
- **remove-sdlnet**: On-going work to remove SDL_net and the rest of SDL in favor of Qt. However, currently waiting for the stable 1.9.7 release, after which this work will be the basis for a new branch for full removal of SDL. This branch also introduces a revised and trimmed version of libdeng2 (based on the hawthorn branch).
- **replacing-innosetup-with-wixtoolset**: Reimplementing the Windows installer with WiX Toolset.

Other notable branches:

- **hawthorn**: Old branch where skyjake was attempting to revise the Doomsday architecture for version 2.0. Some of the code in this branch is being used in newer branches (remove-sdlnet).