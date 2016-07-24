#ifndef _AHCIX_PCH_
#define _AHCIX_PCH_

#include <ntifs.h>
#include <scsi.h>
#include <ide.h>
#include <initguid.h>
#include <wdmguid.h>
#include <..\ahci.h>


typedef struct _COMMON_DEVICE_EXTENSION {
  BOOLEAN IsFDO;
} COMMON_DEVICE_EXTENSION, *PCOMMON_DEVICE_EXTENSION;

typedef struct _FDO_CONTROLLER_EXTENSION {

  COMMON_DEVICE_EXTENSION  Common;

  PDEVICE_OBJECT           LowerDevice;
  PBUS_INTERFACE_STANDARD  BusInterface;
  PCI_COMMON_CONFIG        PciConfig;


} FDO_CONTROLLER_EXTENSION, *PFDO_CONTROLLER_EXTENSION;

typedef struct _PDO_CHANNEL_EXTENSION
{
  COMMON_DEVICE_EXTENSION Common;

  PDEVICE_OBJECT  ControllerFdo;


} PDO_CHANNEL_EXTENSION, *PPDO_CHANNEL_EXTENSION;

//---------------------------------------------------
/* ahcix.c */

/* ahcixfdo.c */
NTSTATUS
AhciXFdoPnpDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp);

/* ahcixpdo.c */
NTSTATUS
AhciXPdoPnpDispatch(
    IN PDEVICE_OBJECT ChannelPdo,
    IN PIRP Irp);


#endif /* _AHCIX_PCH_ */
