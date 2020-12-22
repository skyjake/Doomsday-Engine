# Test for Doomsday SDK and libappfw

This test application is a minimal GUI application based on *libappfw* and the rest of the Doomsday SDK. It shows a simple UI with a single label widget.

The application supports VR modes. A virtual mouse cursor is displayed if the native mouse cursor cannot be used to interact with window contents.

Command line options:

- **--ovr** Use the Oculus Rift VR mode.

## Instructions

Before compiling you must have the Doomsday SDK installed somewhere. The qmake variable `DE_SDK_DIR` must point to the SDK folder.
