#ifndef _AHCIX_PCH_
#define _AHCIX_PCH_

#include <ntifs.h>
#include <scsi.h>
#include <ide.h>
#include <..\ahci.h>


typedef struct _COMMON_DEVICE_EXTENSION {
  BOOLEAN IsFDO;
} COMMON_DEVICE_EXTENSION, *PCOMMON_DEVICE_EXTENSION;


#endif /* _AHCIX_PCH_ */
