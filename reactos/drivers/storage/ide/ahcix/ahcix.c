
#include "ahcix.h"

//#define NDEBUG
#include <debug.h>


NTSTATUS
ForwardIrpAndForget(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
  PDEVICE_OBJECT  LowerDevice;

  ASSERT(((PCOMMON_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->IsFDO);
  LowerDevice = ((PFDO_CONTROLLER_EXTENSION)DeviceObject->DeviceExtension)->LowerDevice;
  ASSERT(LowerDevice);

  IoSkipCurrentIrpStackLocation(Irp);
  return IoCallDriver(LowerDevice, Irp);
}

DRIVER_DISPATCH AhciXForwardOrIgnore;
NTSTATUS NTAPI
AhciXForwardOrIgnore(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
  if ( ((PCOMMON_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->IsFDO )
  {
    return ForwardIrpAndForget(DeviceObject, Irp);
  }
  else
  {
    ULONG MajorFunction;
    NTSTATUS Status;

    MajorFunction = IoGetCurrentIrpStackLocation(Irp)->MajorFunction;

    if ( MajorFunction == IRP_MJ_CREATE  ||
         MajorFunction == IRP_MJ_CLEANUP ||
         MajorFunction == IRP_MJ_CLOSE )
    {
      Status = STATUS_SUCCESS;
    }
    else
    {
      DPRINT1("PDO stub for major function 0x%lx\n", MajorFunction);
      Status = STATUS_NOT_SUPPORTED;
    }

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = Status;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return Status;
  }
}

DRIVER_UNLOAD AhciXUnload;
VOID NTAPI 
AhciXUnload(IN PDRIVER_OBJECT DriverObject)
{
  DPRINT1("AhciX Unload ... \n");
}

DRIVER_DISPATCH AhciXPnpDispatch;
NTSTATUS NTAPI
AhciXPnpDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
  if ( ((PCOMMON_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->IsFDO )
    return AhciXFdoPnpDispatch(DeviceObject, Irp);
  else
    return AhciXPdoPnpDispatch(DeviceObject, Irp);
}

NTSTATUS
GetBusInterface(
    IN PFDO_CONTROLLER_EXTENSION ControllerFdoExtension)
{
  PBUS_INTERFACE_STANDARD  BusInterface = NULL;
  KEVENT                   Event;
  IO_STATUS_BLOCK          IoStatus;
  PIRP                     Irp;
  PIO_STACK_LOCATION       Stack;
  NTSTATUS                 Status = STATUS_UNSUCCESSFUL;

  if ( ControllerFdoExtension->BusInterface )
  {
    DPRINT1("We already have the bus interface\n");
    goto cleanup;
  }

  BusInterface = ExAllocatePool(PagedPool, sizeof(BUS_INTERFACE_STANDARD));
  if ( !BusInterface )
  {
    DPRINT1("ExAllocatePool() failed\n");
    Status = STATUS_INSUFFICIENT_RESOURCES;
    goto cleanup;
  }

  KeInitializeEvent(&Event, SynchronizationEvent, FALSE);

  Irp = IoBuildSynchronousFsdRequest(
               IRP_MJ_PNP,
               ControllerFdoExtension->LowerDevice,
               NULL,
               0,
               NULL,
               &Event,
               &IoStatus);

  if ( !Irp )
  {
    DPRINT1("IoBuildSynchronousFsdRequest() failed\n");
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

  Status = IoCallDriver(ControllerFdoExtension->LowerDevice, Irp);
  if ( Status == STATUS_PENDING )
  {
    KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
    Status = IoStatus.Status;
  }

  if ( !NT_SUCCESS(Status) )
    goto cleanup;

  ControllerFdoExtension->BusInterface = BusInterface;
  BusInterface = NULL;
  Status = STATUS_SUCCESS;

cleanup:
  if ( BusInterface )
    ExFreePool(BusInterface);

  return Status;
}

NTSTATUS
ReleaseBusInterface(
    IN PFDO_CONTROLLER_EXTENSION ControllerFdoExtension)
{
  NTSTATUS  Status = STATUS_UNSUCCESSFUL;

  if ( ControllerFdoExtension->BusInterface )
  {
    (*ControllerFdoExtension->BusInterface->InterfaceDereference)(
                ControllerFdoExtension->BusInterface->Context);
                ControllerFdoExtension->BusInterface = NULL;
                Status = STATUS_SUCCESS;
  }

  return Status;
}

NTSTATUS
AhciXFindMassStorageController(
    IN PFDO_CONTROLLER_EXTENSION ControllerFdoExtension,
    IN PPCI_COMMON_CONFIG PciConfig)
{
  ULONG  BaseAddress;

  DPRINT("AhciXFindMassStorageController(%p %p)\n", ControllerFdoExtension, PciConfig);

  DPRINT("PciConfig->VendorID                 - %x\n", PciConfig->VendorID);
  DPRINT("PciConfig->DeviceID                 - %x\n", PciConfig->DeviceID);
  DPRINT("PciConfig->Command                  - %x\n", PciConfig->Command);
  DPRINT("PciConfig->Status                   - %x\n", PciConfig->Status);
  DPRINT("PciConfig->RevisionID               - %x\n", PciConfig->RevisionID);
  DPRINT("PciConfig->ProgIf                   - %x\n", PciConfig->ProgIf);
  DPRINT("PciConfig->SubClass                 - %x\n", PciConfig->SubClass);
  DPRINT("PciConfig->BaseClass                - %x\n", PciConfig->BaseClass);
  DPRINT("PciConfig->CacheLineSize            - %x\n", PciConfig->CacheLineSize);
  DPRINT("PciConfig->LatencyTimer             - %x\n", PciConfig->LatencyTimer);
  DPRINT("PciConfig->HeaderType               - %x\n", PciConfig->HeaderType);
  DPRINT("PciConfig->BIST                     - %x\n", PciConfig->BIST);
  DPRINT("PciConfig->u.type0.BaseAddresses[0] - %x\n", PciConfig->u.type0.BaseAddresses[0]);
  DPRINT("PciConfig->u.type0.BaseAddresses[1] - %x\n", PciConfig->u.type0.BaseAddresses[1]);
  DPRINT("PciConfig->u.type0.BaseAddresses[2] - %x\n", PciConfig->u.type0.BaseAddresses[2]);
  DPRINT("PciConfig->u.type0.BaseAddresses[3] - %x\n", PciConfig->u.type0.BaseAddresses[3]);
  DPRINT("PciConfig->u.type0.BaseAddresses[4] - %x\n", PciConfig->u.type0.BaseAddresses[4]);
  DPRINT("PciConfig->u.type0.BaseAddresses[5] - %x\n", PciConfig->u.type0.BaseAddresses[5]);
  DPRINT("PciConfig->u.type0.CIS              - %x\n", PciConfig->u.type0.CIS);
  DPRINT("PciConfig->u.type0.SubVendorID      - %x\n", PciConfig->u.type0.SubVendorID);
  DPRINT("PciConfig->u.type0.SubSystemID      - %x\n", PciConfig->u.type0.SubSystemID);
  DPRINT("PciConfig->u.type0.ROMBaseAddress   - %x\n", PciConfig->u.type0.ROMBaseAddress);
  DPRINT("PciConfig->u.type0.CapabilitiesPtr  - %x\n", PciConfig->u.type0.CapabilitiesPtr);
  DPRINT("PciConfig->u.type0.Reserved1[0]     - %x\n", PciConfig->u.type0.Reserved1[0]);
  DPRINT("PciConfig->u.type0.Reserved1[1]     - %x\n", PciConfig->u.type0.Reserved1[1]);
  DPRINT("PciConfig->u.type0.Reserved1[2]     - %x\n", PciConfig->u.type0.Reserved1[2]);
  DPRINT("PciConfig->u.type0.Reserved2        - %x\n", PciConfig->u.type0.Reserved2);
  DPRINT("PciConfig->u.type0.InterruptLine    - %x\n", PciConfig->u.type0.InterruptLine);
  DPRINT("PciConfig->u.type0.InterruptPin     - %x\n", PciConfig->u.type0.InterruptPin);
  DPRINT("PciConfig->u.type0.MinimumGrant     - %x\n", PciConfig->u.type0.MinimumGrant);
  DPRINT("PciConfig->u.type0.MaximumLatency   - %x\n", PciConfig->u.type0.MaximumLatency);

  ControllerFdoExtension->VendorId = PciConfig->VendorID;
  ControllerFdoExtension->DeviceId = PciConfig->DeviceID;

  ControllerFdoExtension->ControllerMode = CONTROLLER_COMPATABLE_MODE;    // Compatable mode 

  if ( PciConfig->BaseClass != PCI_CLASS_MASS_STORAGE_CTLR )
    return STATUS_UNSUCCESSFUL;

  switch ( PciConfig->SubClass )
  {
    case 0x00:
      DPRINT("PciConfig->SubClass == 0x00 FIXME\n");                      //SCSI
      break;

    case 0x01:
      DPRINT("PciConfig->SubClass == 0x01\n");                            //IDE
      if ( PciConfig->ProgIf & 0x05 )
        ControllerFdoExtension->ControllerMode = CONTROLLER_NATIVE_MODE;  // Native mode 
      break;

    case 0x04:
      DPRINT("PciConfig->SubClass == 0x04 FIXME\n");                      //RAID
      break;

    case 0x06:
      DPRINT("PciConfig->SubClass == 0x06\n");                            //SATA (AHCI only?)

      BaseAddress = PciConfig->u.type0.BaseAddresses[5];
      DPRINT("BaseAddress - %p\n", BaseAddress);

      if ( BaseAddress )
      {
        DPRINT("Found AHCI controller!\n");
        ControllerFdoExtension->ControllerMode = CONTROLLER_AHCI_MODE;    // AHCI mode 
      }
      else
      {
        DPRINT("BaseAddress == 0\n");
        ControllerFdoExtension->AhciRegisters = NULL;
      }
      break;

    case 0x80:
      DPRINT("PciConfig->SubClass == 0x80 FIXME\n");
      break;

    default:
      DPRINT("PciConfig->SubClass == unknown FIXME\n");
      break;
  }

  return STATUS_SUCCESS;
}

NTSTATUS NTAPI
AhciXAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT ControllerPdo)
{
  PDEVICE_OBJECT             ControllerFdo;
  PFDO_CONTROLLER_EXTENSION  ControllerFdoExtension;
  ULONG                      BytesRead;
  NTSTATUS                   Status;

  DPRINT("AhciXAddDevice(%p %p)\n", DriverObject, ControllerPdo);

  Status = IoCreateDevice(
             DriverObject,
             sizeof(FDO_CONTROLLER_EXTENSION),
             NULL,
             FILE_DEVICE_BUS_EXTENDER,
             FILE_DEVICE_SECURE_OPEN,
             TRUE,
             &ControllerFdo);

  if ( !NT_SUCCESS(Status) )
  {
    DPRINT1("IoCreateDevice() failed with status 0x%08lx\n", Status);
    return Status;
  }

  ControllerFdoExtension = (PFDO_CONTROLLER_EXTENSION)ControllerFdo->DeviceExtension;
  RtlZeroMemory(ControllerFdoExtension, sizeof(FDO_CONTROLLER_EXTENSION));

  ControllerFdoExtension->Common.IsFDO = TRUE;

  Status = IoAttachDeviceToDeviceStackSafe(
             ControllerFdo,
             ControllerPdo,
             &ControllerFdoExtension->LowerDevice);

  if ( !NT_SUCCESS(Status) )
  {
    DPRINT1("IoAttachDeviceToDeviceStackSafe() failed with status 0x%08lx\n", Status);
    return Status;
  }

  Status = GetBusInterface(ControllerFdoExtension);

  if ( !NT_SUCCESS(Status) )
  {
    DPRINT("GetBusInterface() failed with status 0x%08lx\n", Status);
    IoDetachDevice(ControllerFdoExtension->LowerDevice);
    return Status;
  }

  BytesRead = (*ControllerFdoExtension->BusInterface->GetBusData)(
                          ControllerFdoExtension->BusInterface->Context,
                          PCI_WHICHSPACE_CONFIG,
                          &ControllerFdoExtension->PciConfig,
                          0,
                          PCI_COMMON_HDR_LENGTH);

  if ( BytesRead != PCI_COMMON_HDR_LENGTH )
  {
    DPRINT1("BusInterface->GetBusData() failed()\n");
    ReleaseBusInterface(ControllerFdoExtension);
    IoDetachDevice(ControllerFdoExtension->LowerDevice);
    return STATUS_IO_DEVICE_ERROR;
  }

  Status = AhciXFindMassStorageController(
                ControllerFdoExtension,
                &ControllerFdoExtension->PciConfig);

  ControllerFdo->Flags &= ~DO_DEVICE_INITIALIZING;

  return STATUS_SUCCESS;
}

NTSTATUS NTAPI
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath)
{
  DPRINT("AhciX DriverEntry (%p '%wZ')\n", DriverObject, RegistryPath);

  DriverObject->DriverExtension->AddDevice                    = AhciXAddDevice;
  DriverObject->DriverUnload                                  = AhciXUnload;
  DriverObject->DriverStartIo                                 = 0;  // AhciXStartIo;

  DriverObject->MajorFunction[IRP_MJ_CREATE]                  = AhciXForwardOrIgnore;
  DriverObject->MajorFunction[IRP_MJ_CLOSE]                   = AhciXForwardOrIgnore;
  DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]          = AhciXForwardOrIgnore;  // AhciXDispatchDeviceControl;
  DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = AhciXForwardOrIgnore;  // AhciXDispatchScsi;

  DriverObject->MajorFunction[IRP_MJ_POWER]                   = AhciXForwardOrIgnore;  // AhciXDispatchPower;
  DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL]          = AhciXForwardOrIgnore;  // AhciXDispatchSystemControl;
  DriverObject->MajorFunction[IRP_MJ_PNP]                     = AhciXPnpDispatch;

  return STATUS_SUCCESS;
}
