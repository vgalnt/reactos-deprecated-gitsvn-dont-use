/*
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         PCI IDE bus driver extension
 * FILE:            drivers/storage/pciidex/fdo.c
 * PURPOSE:         IRP_MJ_PNP operations for FDOs
 * PROGRAMMERS:     Hervé Poussineau (hpoussin@reactos.org)
 */

#include "pciidex.h"

//#define NDEBUG
#include <debug.h>

#include <initguid.h>
#include <wdmguid.h>

static NTSTATUS
GetBusInterface(
	IN PFDO_DEVICE_EXTENSION DeviceExtension)
{
	PBUS_INTERFACE_STANDARD BusInterface = NULL;
	KEVENT Event;
	IO_STATUS_BLOCK IoStatus;
	PIRP Irp;
	PIO_STACK_LOCATION Stack;
	NTSTATUS Status = STATUS_UNSUCCESSFUL;

	if (DeviceExtension->BusInterface)
	{
		DPRINT("We already have the bus interface\n");
		goto cleanup;
	}

	BusInterface = ExAllocatePool(PagedPool, sizeof(BUS_INTERFACE_STANDARD));
	if (!BusInterface)
	{
		DPRINT("ExAllocatePool() failed\n");
		Status = STATUS_INSUFFICIENT_RESOURCES;
		goto cleanup;
	}

	KeInitializeEvent(&Event, SynchronizationEvent, FALSE);
	Irp = IoBuildSynchronousFsdRequest(
		IRP_MJ_PNP,
		DeviceExtension->LowerDevice,
		NULL,
		0,
		NULL,
		&Event,
		&IoStatus);
	if (!Irp)
	{
		DPRINT("IoBuildSynchronousFsdRequest() failed\n");
		Status = STATUS_INSUFFICIENT_RESOURCES;
		goto cleanup;
	}

	Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
	Irp->IoStatus.Information = 0;

	Stack = IoGetNextIrpStackLocation(Irp);
	Stack->MajorFunction = IRP_MJ_PNP;
	Stack->MinorFunction = IRP_MN_QUERY_INTERFACE;
	Stack->Parameters.QueryInterface.InterfaceType = (LPGUID)&GUID_BUS_INTERFACE_STANDARD;
	Stack->Parameters.QueryInterface.Version = 1;
	Stack->Parameters.QueryInterface.Size = sizeof(BUS_INTERFACE_STANDARD);
	Stack->Parameters.QueryInterface.Interface = (PINTERFACE)BusInterface;
	Stack->Parameters.QueryInterface.InterfaceSpecificData = NULL;

	Status = IoCallDriver(DeviceExtension->LowerDevice, Irp);
	if (Status == STATUS_PENDING)
	{
		KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
		Status = IoStatus.Status;
	}
	if (!NT_SUCCESS(Status))
		goto cleanup;

	DeviceExtension->BusInterface = BusInterface;
	BusInterface = NULL;
	Status = STATUS_SUCCESS;

cleanup:
	if (BusInterface) ExFreePool(BusInterface);
	return Status;
}

static NTSTATUS
ReleaseBusInterface(
	IN PFDO_DEVICE_EXTENSION DeviceExtension)
{
	NTSTATUS Status = STATUS_UNSUCCESSFUL;

	if (DeviceExtension->BusInterface)
	{
		(*DeviceExtension->BusInterface->InterfaceDereference)(
			DeviceExtension->BusInterface->Context);
		DeviceExtension->BusInterface = NULL;
		Status = STATUS_SUCCESS;
	}

	return Status;
}

NTSTATUS NTAPI
PciIdeXAddDevice(
	IN PDRIVER_OBJECT DriverObject,
	IN PDEVICE_OBJECT Pdo)
{
	PPCIIDEX_DRIVER_EXTENSION DriverExtension;
	PFDO_DEVICE_EXTENSION DeviceExtension;
	PDEVICE_OBJECT Fdo;
	ULONG BytesRead;
	PCI_COMMON_CONFIG PciConfig;
	NTSTATUS Status;

	DPRINT("PciIdeXAddDevice(%p %p)\n", DriverObject, Pdo);

	DriverExtension = IoGetDriverObjectExtension(DriverObject, DriverObject);
	ASSERT(DriverExtension);

	Status = IoCreateDevice(
		DriverObject,
		sizeof(FDO_DEVICE_EXTENSION) + DriverExtension->MiniControllerExtensionSize,
		NULL,
		FILE_DEVICE_BUS_EXTENDER,
		FILE_DEVICE_SECURE_OPEN,
		TRUE,
		&Fdo);
	if (!NT_SUCCESS(Status))
	{
		DPRINT("IoCreateDevice() failed with status 0x%08lx\n", Status);
		return Status;
	}

	DeviceExtension = (PFDO_DEVICE_EXTENSION)Fdo->DeviceExtension;
	RtlZeroMemory(DeviceExtension, sizeof(FDO_DEVICE_EXTENSION));

	DeviceExtension->Common.IsFDO = TRUE;

	Status = IoAttachDeviceToDeviceStackSafe(Fdo, Pdo, &DeviceExtension->LowerDevice);
	if (!NT_SUCCESS(Status))
	{
		DPRINT("IoAttachDeviceToDeviceStackSafe() failed with status 0x%08lx\n", Status);
		return Status;
	}

	Status = GetBusInterface(DeviceExtension);
	if (!NT_SUCCESS(Status))
	{
		DPRINT("GetBusInterface() failed with status 0x%08lx\n", Status);
		IoDetachDevice(DeviceExtension->LowerDevice);
		return Status;
	}

	BytesRead = (*DeviceExtension->BusInterface->GetBusData)(
		DeviceExtension->BusInterface->Context,
		PCI_WHICHSPACE_CONFIG,
		&PciConfig,
		0,
		PCI_COMMON_HDR_LENGTH);
	if (BytesRead != PCI_COMMON_HDR_LENGTH)
	{
		DPRINT("BusInterface->GetBusData() failed()\n");
		ReleaseBusInterface(DeviceExtension);
		IoDetachDevice(DeviceExtension->LowerDevice);
		return STATUS_IO_DEVICE_ERROR;
	}

	///*
	DPRINT("PciConfig.VendorID                 - %x\n", PciConfig.VendorID);
	DPRINT("PciConfig.DeviceID                 - %x\n", PciConfig.DeviceID);
	DPRINT("PciConfig.Command                  - %x\n", PciConfig.Command);
	DPRINT("PciConfig.Status                   - %x\n", PciConfig.Status);
	DPRINT("PciConfig.RevisionID               - %x\n", PciConfig.RevisionID);
	DPRINT("PciConfig.ProgIf                   - %x\n", PciConfig.ProgIf);
	DPRINT("PciConfig.SubClass                 - %x\n", PciConfig.SubClass);
	DPRINT("PciConfig.BaseClass                - %x\n", PciConfig.BaseClass);
	DPRINT("PciConfig.CacheLineSize            - %x\n", PciConfig.CacheLineSize);
	DPRINT("PciConfig.LatencyTimer             - %x\n", PciConfig.LatencyTimer);
	DPRINT("PciConfig.HeaderType               - %x\n", PciConfig.HeaderType);
	DPRINT("PciConfig.BIST                     - %x\n", PciConfig.BIST);
	DPRINT("PciConfig.u.type0.BaseAddresses[0] - %x\n", PciConfig.u.type0.BaseAddresses[0]);
	DPRINT("PciConfig.u.type0.BaseAddresses[1] - %x\n", PciConfig.u.type0.BaseAddresses[1]);
	DPRINT("PciConfig.u.type0.BaseAddresses[2] - %x\n", PciConfig.u.type0.BaseAddresses[2]);
	DPRINT("PciConfig.u.type0.BaseAddresses[3] - %x\n", PciConfig.u.type0.BaseAddresses[3]);
	DPRINT("PciConfig.u.type0.BaseAddresses[4] - %x\n", PciConfig.u.type0.BaseAddresses[4]);
	DPRINT("PciConfig.u.type0.BaseAddresses[5] - %x\n", PciConfig.u.type0.BaseAddresses[5]);
	DPRINT("PciConfig.u.type0.CIS              - %x\n", PciConfig.u.type0.CIS);
	DPRINT("PciConfig.u.type0.SubVendorID      - %x\n", PciConfig.u.type0.SubVendorID);
	DPRINT("PciConfig.u.type0.SubSystemID      - %x\n", PciConfig.u.type0.SubSystemID);
	DPRINT("PciConfig.u.type0.ROMBaseAddress   - %x\n", PciConfig.u.type0.ROMBaseAddress);
	DPRINT("PciConfig.u.type0.CapabilitiesPtr  - %x\n", PciConfig.u.type0.CapabilitiesPtr);
	DPRINT("PciConfig.u.type0.Reserved1[0]     - %x\n", PciConfig.u.type0.Reserved1[0]);
	DPRINT("PciConfig.u.type0.Reserved1[1]     - %x\n", PciConfig.u.type0.Reserved1[1]);
	DPRINT("PciConfig.u.type0.Reserved1[2]     - %x\n", PciConfig.u.type0.Reserved1[2]);
	DPRINT("PciConfig.u.type0.Reserved2        - %x\n", PciConfig.u.type0.Reserved2);
	DPRINT("PciConfig.u.type0.InterruptLine    - %x\n", PciConfig.u.type0.InterruptLine);
	DPRINT("PciConfig.u.type0.InterruptPin     - %x\n", PciConfig.u.type0.InterruptPin);
	DPRINT("PciConfig.u.type0.MinimumGrant     - %x\n", PciConfig.u.type0.MinimumGrant);
	DPRINT("PciConfig.u.type0.MaximumLatency   - %x\n", PciConfig.u.type0.MaximumLatency);
	//*/

	DeviceExtension->VendorId = PciConfig.VendorID;
	DeviceExtension->DeviceId = PciConfig.DeviceID;

	DeviceExtension->ControllerMode[PRIMARY_CHANNEL]   = 
        DeviceExtension->ControllerMode[SECONDARY_CHANNEL] = FALSE; // compatible BM PCI IDE mode 

        if ((PciConfig.BaseClass == PCI_CLASS_MASS_STORAGE_CTLR) &&
            (PciConfig.SubClass  == PCI_SUBCLASS_MSC_IDE_CTLR))
        {
		UCHAR PrimaryMode, SecondaryMode, PrimaryFixed, SecondaryFixed, ProgIf;

		ProgIf = PciConfig.ProgIf;

		if (!(ProgIf & 0x80))
		{
			DeviceExtension->BusMasterBase = 0;
			Fdo->Flags &= ~DO_DEVICE_INITIALIZING;
			return STATUS_SUCCESS;
		}

	 	if (PciConfig.u.type0.BaseAddresses[4] == 0)
	 	{
			if ((PciConfig.u.type0.BaseAddresses[0] & PCI_ADDRESS_MEMORY_TYPE_MASK) == PCI_TYPE_64BIT)
			{
				DPRINT("PciIdeXAddDevice: FIXME - found SATA DPA mode. BaseAddresses[0] - %x\n", PciConfig.u.type0.BaseAddresses[0]);
			}

			Fdo->Flags &= ~DO_DEVICE_INITIALIZING;
			return STATUS_SUCCESS;
	 	}

		PrimaryMode    = (ProgIf & 1) == 1; // Primary channel mode is PCI (native mode)
		PrimaryFixed   = (ProgIf & 2) == 0; // Primary channel mode can changed == FALSE
		SecondaryMode  = (ProgIf & 4) == 4; // Secondary channel mode is PCI (native mode)
		SecondaryFixed = (ProgIf & 8) == 0; // Secondary channel mode can changed == FALSE

	 	if (PciConfig.u.type0.BaseAddresses[4] & PCI_ADDRESS_IO_SPACE)
	 	{
	 		DPRINT("PciIdeXAddDevice: found Bus Master controller!\n");
	 		DeviceExtension->BusMasterBase = PciConfig.u.type0.BaseAddresses[4] & PCI_ADDRESS_IO_ADDRESS_MASK;
	 		DPRINT("PciIdeXAddDevice: Bus Master Registers at IO %lx\n", DeviceExtension->BusMasterBase);

			/*
			 * [..] In order for Windows XP SP1 and Windows Server 2003 to switch an ATA
			 * ATA controller from compatible mode to native mode, the following must be
			 * true:
			 *
			 * - The controller must indicate in its programming interface that both channels
			 *   can be switched to native mode. Windows XP SP1 and Windows Server 2003 do
			 *   not support switching only one IDE channel to native mode. See the PCI IDE
			 *   Controller Specification Revision 1.0 for details.
			 */
			if ((PrimaryMode != SecondaryMode) || (PrimaryFixed != SecondaryFixed))
			{
				/* does not support this configuration, fail */
				DPRINT1("PciIdeXAddDevice: unsupported IDE controller configuration for VEN_%04x&DEV_%04x!\n",
				        DeviceExtension->VendorId, DeviceExtension->DeviceId);
	 			DeviceExtension->BusMasterBase = 0;
			}
			else
			{
				if ( !(PciConfig.Command & 4) )
				{
					PciConfig.Command |= 4; //enable bus master PCI

					//write Command register
					BytesRead = (*DeviceExtension->BusInterface->SetBusData)(
							DeviceExtension->BusInterface->Context,
							PCI_WHICHSPACE_CONFIG,
							&PciConfig.Command,
							FIELD_OFFSET(PCI_COMMON_HEADER, Command),
							sizeof(PciConfig.Command));

					if (BytesRead != sizeof(PciConfig.Command))
					{
						DPRINT("PciIdeXAddDevice: BusInterface->SetBusData() failed: BytesRead - %lx\n", BytesRead);
					}
				}

				/* Check if the controller is already in native mode */
				if ((PrimaryMode) && (SecondaryMode))
				{
					if ( !PciConfig.u.type0.BaseAddresses[0] ||
					     !PciConfig.u.type0.BaseAddresses[1] ||
					     !PciConfig.u.type0.BaseAddresses[2] ||
					     !PciConfig.u.type0.BaseAddresses[3] )
					{
						Fdo->Flags &= ~DO_DEVICE_INITIALIZING;
						return STATUS_SUCCESS;
					}

					/* The controller is now in native mode */
					DeviceExtension->ControllerMode[PRIMARY_CHANNEL]   = 
				        DeviceExtension->ControllerMode[SECONDARY_CHANNEL] = TRUE; //native BM PCI IDE mode 
					DPRINT1("PciIdeXAddDevice: controller in NATIVE PCI IDE mode!\n");

					DPRINT("PciIdeXAddDevice: BaseAddresses[5] - %p\n", PciConfig.u.type0.BaseAddresses[5]);
					if (PciConfig.u.type0.BaseAddresses[5])
					{
						if (!(PciConfig.u.type0.BaseAddresses[5] & PCI_ADDRESS_IO_SPACE))
						{
							PVOID ResourceBase;
							PHYSICAL_ADDRESS StartAddress;

							DPRINT(" NOT  (PciConfig.u.type0.BaseAddresses[5] & PCI_ADDRESS_IO_SPACE)\n");
							StartAddress.LowPart = PciConfig.u.type0.BaseAddresses[5] & PCI_ADDRESS_MEMORY_ADDRESS_MASK;
							StartAddress.HighPart = 0;
							ResourceBase = MmMapIoSpace(StartAddress, 4 * sizeof(ULONG), MmNonCached);

							if ( !ResourceBase )
							{
								DPRINT1("MmMapIoSpace failed\n");
								//DeviceExtension->SataBaseAddress = 0;
								Fdo->Flags &= ~DO_DEVICE_INITIALIZING;
								return STATUS_SUCCESS;
							}

							//DeviceExtension->SataBaseAddress = (ULONG_PTR)ResourceBase;
							DPRINT("PciIdeXAddDevice: BaseAddresses[5]                 - %p\n", PciConfig.u.type0.BaseAddresses[5]);
							//DPRINT("PciIdeXAddDevice: SATA SuperSet Registers at MemIO - %p\n", DeviceExtension->SataBaseAddress);
						}
						else
						{
							DPRINT("PciConfig.u.type0.BaseAddresses[5] & PCI_ADDRESS_IO_SPACE\n");
						}
					}
				}
				else
				{
					DPRINT1("PciIdeXAddDevice: FIXME! Enable NATIVE PCI IDE mode!\n");
				}
			}
	 	}
        }

	Fdo->Flags &= ~DO_DEVICE_INITIALIZING;

	return STATUS_SUCCESS;
}

static NTSTATUS NTAPI
PciIdeXGetTransferModes(
	IN IDENTIFY_DATA IdentifyData,
	OUT PULONG BestXferMode,
	OUT PULONG CurrentXferMode)
{
	ULONG Best = PIO_MODE0;
	ULONG Current = PIO_MODE0;

	DPRINT("PciIdeXGetTransferModes(%lu, %p %p)\n",
		IdentifyData, BestXferMode, CurrentXferMode);

	/* FIXME: if current mode is a PIO mode, how to get it?
	 * At the moment, PIO_MODE0 is always returned...
	 */

	if (IdentifyData.TranslationFieldsValid & 0x2)
	{
		/* PIO modes and some DMA modes are supported */
		if (IdentifyData.AdvancedPIOModes & 0x02)
			Best = PIO_MODE4;
		else if (IdentifyData.AdvancedPIOModes & 0x01)
			Best = PIO_MODE3;
		else if (IdentifyData.PioCycleTimingMode == 0x02)
			Best = PIO_MODE2;
		else if (IdentifyData.PioCycleTimingMode == 0x01)
			Best = PIO_MODE1;
		else if (IdentifyData.PioCycleTimingMode == 0x00)
			Best = PIO_MODE0;
		else
			Best = PIO_MODE0;

		DPRINT("PciIdeXGetTransferModes: PIO Best - %x)\n", Best);

		if (IdentifyData.SingleWordDMASupport & 0x4)
			Best = SWDMA_MODE2;
		else if (IdentifyData.SingleWordDMASupport & 0x2)
			Best = SWDMA_MODE1;
		else if (IdentifyData.SingleWordDMASupport & 0x1)
			Best = SWDMA_MODE0;

		if (IdentifyData.SingleWordDMAActive & 0x4)
			Current = SWDMA_MODE2;
		else if (IdentifyData.SingleWordDMAActive & 0x2)
			Current = SWDMA_MODE1;
		else if (IdentifyData.SingleWordDMAActive & 0x1)
			Current = SWDMA_MODE0;

		DPRINT("PciIdeXGetTransferModes: SingleWordDMA Best - %x)\n", Best);

		if (IdentifyData.MultiWordDMASupport & 0x4)
			Best = MWDMA_MODE2;
		else if (IdentifyData.MultiWordDMASupport & 0x2)
			Best = MWDMA_MODE1;
		else if (IdentifyData.MultiWordDMASupport & 0x1)
			Best = MWDMA_MODE0;

		if (IdentifyData.MultiWordDMAActive & 0x4)
			Current = MWDMA_MODE2;
		else if (IdentifyData.MultiWordDMAActive & 0x2)
			Current = MWDMA_MODE1;
		else if (IdentifyData.MultiWordDMAActive & 0x1)
			Current = MWDMA_MODE0;

		DPRINT("PciIdeXGetTransferModes: MultiWordDMA Best - %x)\n", Best);
	}

	if (IdentifyData.TranslationFieldsValid & 0x4)
	{
		/* UDMA modes are supported */
		if (IdentifyData.UltraDMAActive & 0x10)
			Current = UDMA_MODE4;
		else if (IdentifyData.UltraDMAActive & 0x8)
			Current = UDMA_MODE3;
		else if (IdentifyData.UltraDMAActive & 0x4)
			Current = UDMA_MODE2;
		else if (IdentifyData.UltraDMAActive & 0x2)
			Current = UDMA_MODE1;
		else if (IdentifyData.UltraDMAActive & 0x1)
			Current = UDMA_MODE0;

		if (IdentifyData.UltraDMASupport & 0x10)
			Best = UDMA_MODE4;
		else if (IdentifyData.UltraDMASupport & 0x8)
			Best = UDMA_MODE3;
		else if (IdentifyData.UltraDMASupport & 0x4)
			Best = UDMA_MODE2;
		else if (IdentifyData.UltraDMASupport & 0x2)
			Best = UDMA_MODE1;
		else if (IdentifyData.UltraDMASupport & 0x1)
			Best = UDMA_MODE0;
	}

	DPRINT("PciIdeXGetTransferModes: Best    - %x)\n", Best);
	DPRINT("PciIdeXGetTransferModes: Current - %x)\n", Best);

	*BestXferMode = Best;
	*CurrentXferMode = Current;
	return TRUE;
}

static NTSTATUS
PciIdeXFdoStartDevice(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp)
{
	PPCIIDEX_DRIVER_EXTENSION DriverExtension;
	PFDO_DEVICE_EXTENSION DeviceExtension;
	PCM_RESOURCE_LIST ResourceList;
	NTSTATUS Status;

	DPRINT("PciIdeXStartDevice(%p %p)\n", DeviceObject, Irp);

	DriverExtension = IoGetDriverObjectExtension(DeviceObject->DriverObject, DeviceObject->DriverObject);
	ASSERT(DriverExtension);
	DeviceExtension = (PFDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	ASSERT(DeviceExtension);
	ASSERT(DeviceExtension->Common.IsFDO);

	DeviceExtension->Properties.Size = sizeof(IDE_CONTROLLER_PROPERTIES);
	DeviceExtension->Properties.ExtensionSize = DriverExtension->MiniControllerExtensionSize;
	Status = DriverExtension->HwGetControllerProperties(
		DeviceExtension->MiniControllerExtension,
		&DeviceExtension->Properties);
	if (!NT_SUCCESS(Status))
		return Status;

	DriverExtension->HwUdmaModesSupported = DeviceExtension->Properties.PciIdeUdmaModesSupported;
	if (!DriverExtension->HwUdmaModesSupported)
		/* This method is optional, so provide our own one */
		DriverExtension->HwUdmaModesSupported = PciIdeXGetTransferModes;

	/* Get bus master port base, if any */
	ResourceList = IoGetCurrentIrpStackLocation(Irp)->Parameters.StartDevice.AllocatedResources;
	if (ResourceList
		&& ResourceList->Count == 1
		&& ResourceList->List[0].PartialResourceList.Count == 1
		&& ResourceList->List[0].PartialResourceList.Version == 1
		&& ResourceList->List[0].PartialResourceList.Revision == 1
		&& ResourceList->List[0].PartialResourceList.PartialDescriptors[0].Type == CmResourceTypePort
		&& ResourceList->List[0].PartialResourceList.PartialDescriptors[0].u.Port.Length == 16)
	{
		DeviceExtension->BusMasterPortBase = ResourceList->List[0].PartialResourceList.PartialDescriptors[0].u.Port.Start;
	}

	DPRINT("PciIdeXStartDevice return STATUS_SUCCESS\n");
	return STATUS_SUCCESS;
}

static NTSTATUS
PciIdeXFdoQueryBusRelations(
	IN PDEVICE_OBJECT DeviceObject,
	OUT PDEVICE_RELATIONS* pDeviceRelations)
{
	PFDO_DEVICE_EXTENSION DeviceExtension;
	PDEVICE_RELATIONS DeviceRelations = NULL;
	PDEVICE_OBJECT Pdo;
	PPDO_DEVICE_EXTENSION PdoDeviceExtension;
	ULONG i, j;
	ULONG PDOs = 0;
	IDE_CHANNEL_STATE ChannelState;
	NTSTATUS Status;

	DeviceExtension = (PFDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	ASSERT(DeviceExtension);
	ASSERT(DeviceExtension->Common.IsFDO);

	for (i = 0; i < MAX_IDE_CHANNEL; i++)
	{
		if (DeviceExtension->Pdo[i])
		{
			PDOs++;
			continue;
		}
		ChannelState = DeviceExtension->Properties.PciIdeChannelEnabled(
			DeviceExtension->MiniControllerExtension, i);
		if (ChannelState == ChannelDisabled)
		{
			DPRINT("Channel %lu is disabled\n", i);
			continue;
		}

		/* Need to create a PDO */
		Status = IoCreateDevice(
			DeviceObject->DriverObject,
			sizeof(PDO_DEVICE_EXTENSION),
			NULL,
			FILE_DEVICE_CONTROLLER,
			FILE_AUTOGENERATED_DEVICE_NAME,
			FALSE,
			&Pdo);
		if (!NT_SUCCESS(Status))
			/* FIXME: handle error */
			continue;

		PdoDeviceExtension = (PPDO_DEVICE_EXTENSION)Pdo->DeviceExtension;
		RtlZeroMemory(PdoDeviceExtension, sizeof(PDO_DEVICE_EXTENSION));
		PdoDeviceExtension->Common.IsFDO = FALSE;
		PdoDeviceExtension->Channel = i;
		PdoDeviceExtension->ControllerFdo = DeviceObject;
		PdoDeviceExtension->SelfDevice = Pdo;
		Pdo->Flags |= DO_BUS_ENUMERATED_DEVICE;
		Pdo->Flags &= ~DO_DEVICE_INITIALIZING;

		DeviceExtension->Pdo[i] = Pdo;
		PDOs++;
	}

	if (PDOs == 0)
	{
		DeviceRelations = (PDEVICE_RELATIONS)ExAllocatePool(
			PagedPool,
			sizeof(DEVICE_RELATIONS));
	}
	else
	{
		DeviceRelations = (PDEVICE_RELATIONS)ExAllocatePool(
			PagedPool,
			sizeof(DEVICE_RELATIONS) + sizeof(PDEVICE_OBJECT) * (PDOs - 1));
	}
	if (!DeviceRelations)
		return STATUS_INSUFFICIENT_RESOURCES;

	DeviceRelations->Count = PDOs;
	for (i = 0, j = 0; i < MAX_IDE_CHANNEL; i++)
	{
		if (DeviceExtension->Pdo[i])
		{
			ObReferenceObject(DeviceExtension->Pdo[i]);
			DeviceRelations->Objects[j++] = DeviceExtension->Pdo[i];
		}
	}

	*pDeviceRelations = DeviceRelations;
	return STATUS_SUCCESS;
}

NTSTATUS NTAPI
PciIdeXFdoPnpDispatch(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp)
{
	ULONG MinorFunction;
	PIO_STACK_LOCATION Stack;
	ULONG_PTR Information = Irp->IoStatus.Information;
	NTSTATUS Status;

	Stack = IoGetCurrentIrpStackLocation(Irp);
	MinorFunction = Stack->MinorFunction;

	switch (MinorFunction)
	{
		case IRP_MN_START_DEVICE: /* 0x00 */
		{
			DPRINT("IRP_MJ_PNP / IRP_MN_START_DEVICE\n");
			/* Call lower driver */
			Status = ForwardIrpAndWait(DeviceObject, Irp);
			if (NT_SUCCESS(Status))
				Status = PciIdeXFdoStartDevice(DeviceObject, Irp);
			break;
		}
                case IRP_MN_QUERY_REMOVE_DEVICE: /* 0x01 */
                {
                        DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_REMOVE_DEVICE\n");
                        Status = STATUS_UNSUCCESSFUL;
                        break;
                }
		case IRP_MN_QUERY_DEVICE_RELATIONS: /* 0x07 */
		{
			switch (Stack->Parameters.QueryDeviceRelations.Type)
			{
				case BusRelations:
				{
					PDEVICE_RELATIONS DeviceRelations = NULL;
					DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_DEVICE_RELATIONS / BusRelations\n");
					Status = PciIdeXFdoQueryBusRelations(DeviceObject, &DeviceRelations);
					Information = (ULONG_PTR)DeviceRelations;
					break;
				}
				default:
				{
					DPRINT1("IRP_MJ_PNP / IRP_MN_QUERY_DEVICE_RELATIONS / Unknown type 0x%lx\n",
						Stack->Parameters.QueryDeviceRelations.Type);
					Status = STATUS_NOT_IMPLEMENTED;
					break;
				}
			}
			break;
		}
                case IRP_MN_QUERY_PNP_DEVICE_STATE: /* 0x14 */
                {
                        DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_PNP_DEVICE_STATE\n");
                        Information |= PNP_DEVICE_NOT_DISABLEABLE;
                        Status = STATUS_SUCCESS;
                        break;
                }
		case IRP_MN_FILTER_RESOURCE_REQUIREMENTS: /* 0x0d */
		{
			DPRINT("IRP_MJ_PNP / IRP_MN_FILTER_RESOURCE_REQUIREMENTS\n");
			return ForwardIrpAndForget(DeviceObject, Irp);
		}
		default:
		{
			DPRINT1("IRP_MJ_PNP / Unknown minor function 0x%lx\n", MinorFunction);
			return ForwardIrpAndForget(DeviceObject, Irp);
		}
	}

	Irp->IoStatus.Information = Information;
	Irp->IoStatus.Status = Status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return Status;
}
