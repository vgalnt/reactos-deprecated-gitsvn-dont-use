
list(APPEND SOURCE
    ahcix.c
    ahcixfdo.c
    ahcixpdo.c
    ahcix.h)

add_library(ahcix SHARED ${SOURCE} ahcix.rc)
set_module_type(ahcix kernelmodedriver)
add_importlibs(ahcix ntoskrnl hal)
add_cd_file(TARGET ahcix DESTINATION reactos/system32/drivers NO_CAB FOR all)
add_registry_inf(ahcix_reg.inf)
