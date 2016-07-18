
list(APPEND SOURCE
    atax.c
    ataxinit.c
    ataxfdo.c
    atax.h)

add_library(atax SHARED ${SOURCE} atax.rc)
set_module_type(atax kernelmodedriver)
add_importlibs(atax ntoskrnl hal)
add_cd_file(TARGET atax DESTINATION reactos/system32/drivers NO_CAB FOR all)
add_registry_inf(atax_reg.inf)
