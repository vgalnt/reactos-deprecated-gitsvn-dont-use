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
ASSERT(FALSE);
  return 0;
}

NTSTATUS
BusMasterStart(IN PPDO_DEVICE_EXTENSION DeviceExtension)//ChannelPdoExtension
{
  DPRINT("BusMasterEnable: ... \n");
ASSERT(FALSE);
  return 0;
}

NTSTATUS
BusMasterStop(IN PPDO_DEVICE_EXTENSION DeviceExtension)//ChannelPdoExtension
{
  DPRINT("BusMasterDisable: ... \n");
ASSERT(FALSE);
  return 0;
}

ULONG
BusMasterReadStatus(IN PPDO_DEVICE_EXTENSION DeviceExtension)//ChannelPdoExtension
{
	PFDO_DEVICE_EXTENSION  ControllerFdoExtension;
	ULONG                  BusMasterChannelBase;
	ULONG                  Status = 0;
        UCHAR                  Result;

	DPRINT("BusMasterReadStatus: DeviceExtension - %p\n", DeviceExtension);

	ControllerFdoExtension = DeviceExtension->ControllerFdo->DeviceExtension;
	BusMasterChannelBase = ControllerFdoExtension->BusMasterBase + 8 * (DeviceExtension->Channel & 1);

	Result = READ_PORT_UCHAR((PUCHAR)(BusMasterChannelBase + 2));
	DPRINT("BusMasterReadStatus: Result - %p\n", Result);

	/* Bus Master IDE Active: This bit is set when the Start bit is written to the Command  register.
	  This bit is cleared when the last transfer for a region is performed, where EOT for that region is
	  set in the region descriptor. It is also cleared when the Start bit is cleared in the Command
	  register. When this bit is read as a zero, all data transfered from the drive during the previous
	  bus master command is visible in system memory, unless the bus master command was aborted.
	*/
	if ( Result & 1 )
	  Status = 1;

	/* Error: This bit is set when the controller encounters an error in transferring data to/from
	  memory. The exact error condition is bus specific and can be determined in a bus specific
	  manner. This bit is cleared when a '1' is written to it by software.
	*/
	if ( Result & 2 )
	  Status |= 2;

	/* Interrupt:  This bit is set by the rising edge of the IDE interrupt line.  This bit is cleared when a
	  '1' is written to it by software. Software can use this bit to determine if an IDE device has
	  asserted its interrupt line. When this bit is read as a one, all data transfered from the drive is
	  visible in system memory.
	*/
	if ( Result & 4 )
	  Status |= 4;

	DPRINT("BusMasterReadStatus: return - %p\n", Status);
	return Status;
}

NTSTATUS
BusMasterComplete(IN PPDO_DEVICE_EXTENSION DeviceExtension)//ChannelPdoExtension
{
  DPRINT("BusMasterComplete: ... \n");
ASSERT(FALSE);
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
