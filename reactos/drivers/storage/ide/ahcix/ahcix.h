#ifndef _AHCIX_PCH_
#define _AHCIX_PCH_

#include <ntifs.h>
#include <scsi.h>
#include <ide.h>
#include <initguid.h>
#include <wdmguid.h>
#include <..\ahci.h>


// Controller Mode
#define CONTROLLER_COMPATABLE_MODE  0x00
#define CONTROLLER_NATIVE_MODE      0x01
#define CONTROLLER_AHCI_MODE        0x02

typedef struct _COMMON_DEVICE_EXTENSION {
  BOOLEAN IsFDO;
} COMMON_DEVICE_EXTENSION, *PCOMMON_DEVICE_EXTENSION;

typedef struct _FDO_CONTROLLER_EXTENSION {

  COMMON_DEVICE_EXTENSION  Common;

  PDEVICE_OBJECT           LowerDevice;
  PBUS_INTERFACE_STANDARD  BusInterface;
  PCI_COMMON_CONFIG        PciConfig;
  USHORT                   VendorId;
  USHORT                   DeviceId;
  ULONG                    ControllerMode;  // 2 - AHCI, 1 - Native, 0 - Compatible
  PAHCI_MEMORY_REGISTERS   AhciRegisters;
  AHCI_INTERRUPT_RESOURCE  InterruptResource;
  ULONG                    ChannelsCount;
  PDEVICE_OBJECT           ChannelPdo[32];
  AHCI_DEVICE_TYPE         DeviceType[32];


} FDO_CONTROLLER_EXTENSION, *PFDO_CONTROLLER_EXTENSION;

typedef struct _PDO_CHANNEL_EXTENSION
{
  COMMON_DEVICE_EXTENSION Common;

  PDEVICE_OBJECT  ControllerFdo;
  AHCI_INTERFACE  AhciInterface;

} PDO_CHANNEL_EXTENSION, *PPDO_CHANNEL_EXTENSION;

//---------------------------------------------------
/* ahcix.c */
NTSTATUS
ForwardIrpAndWait(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp);

NTSTATUS
ForwardIrpAndForget(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp);

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
