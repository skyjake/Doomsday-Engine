# Build configuration for ATL.
win32 {
    isEmpty(ATL_LIB_DIR) {
        error("dep_atl: Active Template Library (ATL) lib path not defined, check your config_user.pri (ATL_LIB_DIR)")
    }

    # Windows.
    LIBS += -L$$ATL_LIB_DIR
}
