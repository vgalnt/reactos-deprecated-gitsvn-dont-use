#include "pciidex.h"

//#define NDEBUG
#include <debug.h>

//#define MAX_SG_ELEMENTS 0x20 <-- in reactos\hal\halx86\generic\dma.c

NTSTATUS
BusMasterPrepare(
    IN PPDO_DEVICE_EXTENSION  DeviceExtension,
    IN PVOID                  CurrentVirtualAddress,
    IN ULONG                  Length,
    IN PMDL                   Mdl,
    IN BOOLEAN                WriteToDevice,
    IN PVOID                  AllocateAdapter,
    IN PDEVICE_OBJECT         AtaXChannelFdo)
{
  DPRINT("BusMasterPrepare: ... \n");
  return 0;
}

NTSTATUS
BusMasterStart(IN PPDO_DEVICE_EXTENSION DeviceExtension)//ChannelPdoExtension
{
  DPRINT("BusMasterEnable: ... \n");
  return 0;
}

NTSTATUS
BusMasterStop(IN PPDO_DEVICE_EXTENSION DeviceExtension)//ChannelPdoExtension
{
  DPRINT("BusMasterDisable: ... \n");
  return 0;
}

NTSTATUS
BusMasterReadStatus(IN PPDO_DEVICE_EXTENSION DeviceExtension)//ChannelPdoExtension
{
  DPRINT("BusMasterReadStatus: ... \n");
  return 0;
}

NTSTATUS
BusMasterComplete(IN PPDO_DEVICE_EXTENSION DeviceExtension)//ChannelPdoExtension
{
  DPRINT("BusMasterComplete: ... \n");
  return 0;
}

NTSTATUS
QueryBusMasterInterface(
    IN PPDO_DEVICE_EXTENSION DeviceExtension,
    IN PIO_STACK_LOCATION Stack)
{
  PBUS_MASTER_INTERFACE BusMasterInterface;
  PFDO_DEVICE_EXTENSION FdoDeviceExtension = (PFDO_DEVICE_EXTENSION)DeviceExtension->ControllerFdo->DeviceExtension;

  DPRINT("QueryBusMasterInterface: FdoDeviceExtension - %p, BusMasterBase - %p\n", FdoDeviceExtension, FdoDeviceExtension->BusMasterBase);

  BusMasterInterface = (PBUS_MASTER_INTERFACE)Stack->Parameters.QueryInterface.Interface;
  BusMasterInterface->Size = sizeof(BUS_MASTER_INTERFACE);
  DPRINT("QueryBusMasterInterface: BusMasterInterface->Size - %p \n", BusMasterInterface->Size);

  BusMasterInterface->ChannelPdoExtension = DeviceExtension;
  BusMasterInterface->BusMasterBase       = FdoDeviceExtension->BusMasterBase;
  BusMasterInterface->BusMasterPrepare    = (PBUS_MASTER_PREPARE)BusMasterPrepare;
  BusMasterInterface->BusMasterStart      = (PBUS_MASTER_START)BusMasterStart;
  BusMasterInterface->BusMasterStop       = (PBUS_MASTER_STOP)BusMasterStop;
  BusMasterInterface->BusMasterReadStatus = (PBUS_MASTER_READ_STATUS)BusMasterReadStatus;
  BusMasterInterface->BusMasterComplete   = (PBUS_MASTER_COMPLETE)BusMasterComplete;

  DPRINT("QueryBusMasterInterface: return STATUS_SUCCESS\n");
  return STATUS_SUCCESS;
}
