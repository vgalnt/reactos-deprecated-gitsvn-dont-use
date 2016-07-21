
include_directories(${REACTOS_SOURCE_DIR}/drivers/storage/class)

list(APPEND SOURCE
    cdrom.c
    data.c
    ioctl.c
    mmc.c
    sec.c
    cdrom.h)

add_library(cdrom SHARED ${SOURCE} scsicdrm.rc)
target_link_libraries(cdrom libcntpr wdmguid)

if((ARCH STREQUAL "i386") AND (NOT MSVC))
    # FIXME: http://www.cmake.org/Bug/view.php?id=12998
    #add_target_compile_flags(cdrom "-Wno-format -Wno-unused-variable -Wno-pointer-sign")
    set_source_files_properties(${SOURCE} PROPERTIES COMPILE_FLAGS "-Wno-format -Wno-unused-variable -Wno-pointer-sign")
endif()

set_module_type(cdrom kernelmodedriver)
add_importlibs(cdrom classpnp ntoskrnl hal)
add_pch(cdrom cdrom.h SOURCE)
add_cd_file(TARGET cdrom DESTINATION reactos/system32/drivers NO_CAB FOR all)
add_registry_inf(cdrom_reg.inf)
