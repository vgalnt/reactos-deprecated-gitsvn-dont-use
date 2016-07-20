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
	NTSTATUS Status;
	PDMA_ADAPTER DmaAdapter = DeviceExtension->DmaAdapter;

	DeviceExtension->WriteToDevice = WriteToDevice;

	DPRINT("BusMasterPrepare: DeviceExtension       - %p\n", DeviceExtension);
	DPRINT("BusMasterPrepare: CurrentVirtualAddress - %p\n", CurrentVirtualAddress);
	DPRINT("BusMasterPrepare: Length                - %p\n", Length);
	DPRINT("BusMasterPrepare: Mdl                   - %p\n", Mdl);
	DPRINT("BusMasterPrepare: WriteToDevice         - %p\n", WriteToDevice);
	DPRINT("BusMasterPrepare: AllocateAdapter       - %p\n", AllocateAdapter);
	DPRINT("BusMasterPrepare: AtaXChannelFdo        - %p\n", AtaXChannelFdo);

	DeviceExtension->AllocateAdapter        = AllocateAdapter;
	DeviceExtension->AllocateAdapterContext = (ULONG)AtaXChannelFdo;

	Status = DmaAdapter->DmaOperations->GetScatterGatherList(
			DmaAdapter,
			DeviceExtension->SelfDevice,
			Mdl,
			CurrentVirtualAddress,
			Length,
			(PDRIVER_LIST_CONTROL)AdapterListControl,
			DeviceExtension,
			WriteToDevice);

	DPRINT("BusMasterPrepare: return - %p\n", Status);
	return Status;
}

NTSTATUS
BusMasterStart(IN PPDO_DEVICE_EXTENSION DeviceExtension)//ChannelPdoExtension
{
	PFDO_DEVICE_EXTENSION  ControllerFdoExtension;
	ULONG  BusMasterChannelBase;

	DPRINT("BusMasterStart: ... \n");

	ControllerFdoExtension = DeviceExtension->ControllerFdo->DeviceExtension;

	BusMasterChannelBase = ControllerFdoExtension->BusMasterBase +
                               8 * (DeviceExtension->Channel & 1);

	if ( DeviceExtension->WriteToDevice )
	{
		DPRINT("BusMasterStart: Write to device (1). BusMasterChannelBase - %p\n", BusMasterChannelBase);
		//Start Bus Master. PCI bus master reads are performed.
		WRITE_PORT_UCHAR((PUCHAR)BusMasterChannelBase, 1);
	}
	else
	{
		DPRINT("BusMasterStart: Read from device (9). BusMasterChannelBase - %p\n", BusMasterChannelBase);
		//Start Bus Master. PCI bus master writes are performed.
		WRITE_PORT_UCHAR((PUCHAR)BusMasterChannelBase, 9);
	}

	DPRINT("BusMasterStart: return STATUS_SUCCESS\n");
	return STATUS_SUCCESS;
}

NTSTATUS
BusMasterStop(IN PPDO_DEVICE_EXTENSION DeviceExtension)//ChannelPdoExtension
{
	PFDO_DEVICE_EXTENSION  ControllerFdoExtension;
	ULONG  BusMasterChannelBase;


	DPRINT("BusMasterStop: DeviceExtension - %p\n", DeviceExtension);

	ControllerFdoExtension = DeviceExtension->ControllerFdo->DeviceExtension;

	BusMasterChannelBase = ControllerFdoExtension->BusMasterBase + 8 * (DeviceExtension->Channel & 1);
	DPRINT("BusMasterStop: BusMasterChannelBase - %p\n", BusMasterChannelBase);

	/*
	Start/Stop Bus Master: Writing a '1' to this bit enables bus master operation of the controller.
	Bus master operation begins when this bit is detected changing from a zero to a one. The
	controller will transfer data between the IDE device and memory only when this bit is set.
	Master operation can be halted by writing a '0' to this bit.  All state information is lost when a '0'
	is written; Master mode operation cannot be stopped and then resumed.  If this bit is reset while
	bus master operation is still active (i.e., the Bus Master IDE Active bit of the Bus Master IDE
	Status register for that IDE channel is set) and the drive has not yet finished its data transfer (The
	Interupt bit in the Bus Master IDE Status register for that IDE channel is not set), the bus master
	command is said to be aborted and data transfered from the drive may be discarded before being
	written to system memory. This bit is intended to be reset after the data transfer is completed, as
	indicated by either the  Bus Master IDE Active bit or the Interrupt bit of the Bus Master IDE
	Status register for that IDE channel being set, or both. */

	WRITE_PORT_UCHAR((PUCHAR)BusMasterChannelBase, 0);          //Stop Bus Master (Bus Master IDE Command Register)

	/*
	Interrupt:  This bit is set by the rising edge of the IDE interrupt line.  This bit is cleared when a
	'1' is written to it by software.  Software can use this bit to determine if an IDE device has
	asserted its interrupt line. When this bit is read as a one, all data transfered from the drive is
	visible in system memory. */

	WRITE_PORT_UCHAR((PUCHAR)(BusMasterChannelBase + 2), 4);    //Clear Interrupt bit (Bus Master IDE Status Register)

	DPRINT("BusMasterStop: return STATUS_SUCCESS\n");
	return STATUS_SUCCESS;
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
