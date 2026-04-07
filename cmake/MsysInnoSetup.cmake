message (STATUS "Packaging with Inno Setup for MSYS")

# Check Inno Setup location from Windows registry.
execute_process (COMMAND reg query "HKLM\\SOFTWARE\\Classes\\InnoSetupScriptFile\\shell\\Compile\\command" OUTPUT_VARIABLE regVals)
string (REGEX MATCH "REG_SZ(.*)/cc" _ ${regVals})
set (regVals ${CMAKE_MATCH_1})
string (STRIP ${regVals} regVals)
if (regVals MATCHES "\"(.*)\"")
    set (regVals ${CMAKE_MATCH_1})
endif ()
execute_process (COMMAND cygpath -u ${regVals} OUTPUT_VARIABLE regVals)
get_filename_component (INNOSETUP_DIR ${regVals} DIRECTORY)
message (STATUS "Inno Setup path: ${INNOSETUP_DIR}")

set (INNOSETUP_COMMAND $ENV{INNOSETUP_DIR}/iscc.exe)

file (GLOB_RECURSE files 
    RELATIVE ${CPACK_TEMPORARY_DIRECTORY}
    ${CPACK_TEMPORARY_DIRECTORY}/*
)

set (deployScript ${CMAKE_CURRENT_LIST_DIR}/../build/scripts/deploy_msys.py)
#message (STATUS "Script: ${deployScript}")
set (outFile ${CPACK_TEMPORARY_DIRECTORY}/setup.iss)
set (depsDir ${CPACK_TEMPORARY_DIRECTORY}/deps)
file (MAKE_DIRECTORY ${depsDir})
set (binaries)

file (WRITE ${outFile} "
[Setup]
AppName=Doomsday Engine
AppVersion=${CPACK_PACKAGE_VERSION}
WizardStyle=modern
DefaultDirName={autopf}\\Doomsday Engine
DefaultGroupName=Doomsday Engine
UninstallDisplayIcon={app}\\doomsday.exe
Compression=lzma2
SolidCompression=yes
OutputBaseFilename=${CPACK_PACKAGE_FILE_NAME}
OutputDir=..\\..\\..\\..
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64

[Files]
")

macro (do_sign path)
    execute_process (COMMAND /opt/bin/signtool.exe
        sign -n $ENV{DE_SIGNTOOL_CERT}
        -t $ENV{DE_SIGNTOOL_TIMESTAMP} -fd sha256 -v ${path}
    )
endmacro (do_sign)

foreach (fn ${files})
    get_filename_component (fnDir ${fn} DIRECTORY)
    get_filename_component (fnName ${fn} NAME)
    get_filename_component (fnExt ${fn} EXT)
    string (REGEX REPLACE "[^/]+/(.*)" "\\1" fnDir ${fnDir})
    if (fnExt STREQUAL ".exe" OR fnExt STREQUAL ".dll")
        list (APPEND binaries ${depsDir}/${fnName})
        file (COPY ${CPACK_TEMPORARY_DIRECTORY}/${fn} DESTINATION ${depsDir})
        do_sign (${CPACK_TEMPORARY_DIRECTORY}/${fn})
    endif ()
    file (APPEND ${outFile} "Source: \"${fn}\"; DestDir: \"{app}/${fnDir}\"\n")
endforeach (fn)

message (STATUS "Copying dependencies")
execute_process (COMMAND python3 ${deployScript} ${depsDir})
file (REMOVE ${binaries})
file (GLOB deps ${depsDir}/*.dll)
foreach (fn ${deps})
    get_filename_component (fnName ${fn} NAME)
    file (APPEND ${outFile} "Source: \"deps/${fnName}\"; DestDir: \"{app}/bin\"\n")
    do_sign (${CPACK_TEMPORARY_DIRECTORY}/deps/${fnName})
endforeach ()

file (APPEND ${outFile} "
[Icons]
Name: \"{group}\\Doomsday Engine\"; Filename: \"{app}\\doomsday.exe\"
Name: \"{group}\\Doomsday Shell\"; Filename: \"{app}\\doomsday-shell.exe\"
")

execute_process (COMMAND "${INNOSETUP_COMMAND}" setup.iss
    WORKING_DIRECTORY ${CPACK_TEMPORARY_DIRECTORY}
)

# Sign the generated installer.
do_sign (${CPACK_TEMPORARY_DIRECTORY}/../../../../${CPACK_PACKAGE_FILE_NAME}.exe)
