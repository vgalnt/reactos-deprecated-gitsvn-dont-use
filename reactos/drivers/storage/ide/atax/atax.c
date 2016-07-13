#include "atax.h"               

#define NDEBUG
#include <debug.h>


ULONG AtaXChannelCounter  = 0;


VOID NTAPI
AtaXStartIo(
    IN PDEVICE_OBJECT AtaXChannelFdo,
    IN PIRP Irp)
{
  DPRINT("AtaXStartIo ... \n");
  ASSERT(FALSE);
}

DRIVER_DISPATCH AtaXDispatchDeviceControl;
NTSTATUS NTAPI
AtaXDispatchDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
  DPRINT("AtaXDispatchDeviceControl: ... \n");

  if (((PCOMMON_ATAX_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->IsFDO)
  {
    ASSERT(FALSE);
    return STATUS_NOT_SUPPORTED;
  }
  else
  {
    ASSERT(FALSE);
    return STATUS_NOT_SUPPORTED;
  }
}

DRIVER_DISPATCH AtaXDispatchScsi;
NTSTATUS NTAPI
AtaXDispatchScsi(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
  DPRINT("AtaXDispatchScsi: ... \n");
  if (((PCOMMON_ATAX_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->IsFDO)
  {
    ASSERT(FALSE);
    return STATUS_NOT_SUPPORTED;
  }
  else
  {
    ASSERT(FALSE);
    return STATUS_NOT_SUPPORTED;
  }
}

DRIVER_DISPATCH AtaXDispatchPnp;
NTSTATUS NTAPI
AtaXDispatchPnp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
ASSERT(FALSE);
  if ( ((PCOMMON_ATAX_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->IsFDO )
    return 0;//AtaXChannelFdoDispatchPnp(DeviceObject, Irp);
  else
    return 0;//AtaXDevicePdoDispatchPnp(DeviceObject, Irp);
}

DRIVER_UNLOAD AtaXUnload;
VOID NTAPI 
AtaXUnload(IN PDRIVER_OBJECT DriverObject)
{
  DPRINT1("ATAX Unload FIXME \n");
}

DRIVER_DISPATCH AtaXCompleteIrp;
NTSTATUS NTAPI
AtaXCompleteIrp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
  //DPRINT("AtaXCompleteIrp\n");
  Irp->IoStatus.Status = STATUS_SUCCESS;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return STATUS_SUCCESS;
}

NTSTATUS NTAPI
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath)
{
  DPRINT1("ATAX DriverEntry(%p '%wZ')\n", DriverObject, RegistryPath);

  DriverObject->DriverExtension->AddDevice = AddChannelFdo;
  DriverObject->DriverUnload               = AtaXUnload;
  DriverObject->DriverStartIo              = AtaXStartIo;

  DriverObject->MajorFunction[IRP_MJ_CREATE]                  = AtaXCompleteIrp;
  DriverObject->MajorFunction[IRP_MJ_CLOSE]                   = AtaXCompleteIrp;
  DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]          = AtaXDispatchDeviceControl;
  DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = AtaXDispatchScsi;          // IRP_MJ_SCSI == IRP_MJ_INTERNAL_DEVICE_CONTROL
  DriverObject->MajorFunction[IRP_MJ_POWER]                   = AtaXCompleteIrp;           // AtaXDispatchPower;
  DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL]          = AtaXCompleteIrp;           // AtaXDispatchSystemControl;
  DriverObject->MajorFunction[IRP_MJ_PNP]                     = AtaXDispatchPnp;

  DPRINT1("ATAX DriverEntry: return STATUS_SUCCESS\n" );
  return STATUS_SUCCESS;
}
