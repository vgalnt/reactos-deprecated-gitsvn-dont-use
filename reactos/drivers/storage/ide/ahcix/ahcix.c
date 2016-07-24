
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
