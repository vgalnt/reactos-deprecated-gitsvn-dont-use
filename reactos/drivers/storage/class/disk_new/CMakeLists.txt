
include_directories(..)

list(APPEND SOURCE
    data.c
    disk.c
    diskwmi.c
    enum.c
    geometry.c
    part.c
    pnp.c
    disk.h)

add_library(disk SHARED ${SOURCE} disk.rc)
target_link_libraries(disk libcntpr wdmguid)

if((ARCH STREQUAL "i386") AND (NOT MSVC))
    # FIXME: http://www.cmake.org/Bug/view.php?id=12998
    #add_target_compile_flags(disk "-Wno-format -Wno-pointer-sign")
    set_source_files_properties(${SOURCE} PROPERTIES COMPILE_FLAGS "-Wno-format -Wno-pointer-sign")
endif()

set_module_type(disk kernelmodedriver)
add_importlibs(disk classpnp ntoskrnl hal)
add_pch(disk disk.h SOURCE)
add_cd_file(TARGET disk DESTINATION reactos/system32/drivers NO_CAB FOR all)
add_registry_inf(disk_reg.inf)
