REM Build configuration (example).
REM Modify and save as "vcconfig.bat".

SET ROOT_DIR=H:\deng\trunk\doomsday

SET BIN_DIR=Bin\Release
SET OBJ_DIR=Obj\Release

REM -=- Requirements for Doomsday.exe -=-
REM
REM ---- Platform SDK:
SET PLATFORM_INC=D:\VS.NET\Vc7\PlatformSDK\Include
SET PLATFORM_LIB=D:\VS.NET\VC7\PlatformSDK\Lib
SET LIBCI_LIB=D:\VS.NET\Vc7\Lib

REM ---- Python Interpreter
SET PYTHON_DIR=D:\Python24

REM ---- DirectX:
SET DX_INC=D:\sdk\dx8\include
SET DX_LIB=D:\sdk\dx8\lib

REM ---- Creative Labs EAX:
SET EAX_INC=D:\sdk\Creative Labs\EAX 2.0 Extensions SDK\Include
SET EAX_LIB=D:\sdk\Creative Labs\EAX 2.0 Extensions SDK\Libs

REM ---- SDL:
SET SDL_INC=D:\sdk\SDL-1.2.6\include
SET SDL_LIB=D:\sdk\SDL-1.2.6\lib

REM ---- SDL_net:
SET SDLNET_INC=D:\sdk\SDL_net-1.2.5\include
SET SDLNET_LIB=D:\sdk\SDL_net-1.2.5\lib

REM ---- SDL_mixer:
SET SDLMIXER_INC=D:\sdk\SDL_mixer-1.2.7\include
SET SDLMIXER_LIB=D:\sdk\SDL_mixer-1.2.7\lib

REM -=- Requirements for drOpenGL.dll -=-
REM
REM ---- OpenGL (GL/gl.h, GL/glext.h, GL/glu.h):
SET GL_INC=D:\VS.NET\Vc7\Include


REM -=- Requirements for dsA3D.dll -=-
REM
REM ---- Aureal A3D:
SET A3D_INC=D:\sdk\Aureal\A3D 3.0 SDK\sdk\inc
SET A3D_LIB=D:\sdk\Aureal\A3D 3.0 SDK\sdk\lib
