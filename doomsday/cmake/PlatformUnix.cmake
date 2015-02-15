# Linux / BSD / other Unix
include (PlatformGenericUnix)

add_definitions (
    -DDENG_X11
    -D__USE_BSD 
    -D_GNU_SOURCE=1
)
