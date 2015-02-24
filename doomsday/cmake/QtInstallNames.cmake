# Required variables:
# - DENG_SOURCE_DIR
# - CMAKE_INSTALL_NAME_TOOL
# - BINARY_FILE

include (${DENG_SOURCE_DIR}/cmake/Macros.cmake)

fix_bundled_install_names ("${BINARY_FILE}"
    QtCore.framework/Versions/5/QtCore
    QtNetwork.framework/Versions/5/QtNetwork
    QtGui.framework/Versions/5/QtGui
    QtOpenGL.framework/Versions/5/QtOpenGL
    QtWidgets.framework/Versions/5/QtWidgets
    VERBATIM
)
