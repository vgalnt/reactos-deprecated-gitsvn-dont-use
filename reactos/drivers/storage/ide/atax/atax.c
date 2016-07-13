#include "atax.h"               

#define NDEBUG
#include <debug.h>


NTSTATUS NTAPI
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath)
{
  DPRINT1("ATAX DriverEntry(%p '%wZ')\n", DriverObject, RegistryPath);

  DriverObject->DriverExtension->AddDevice = 0;//AddChannelFdo;
  DriverObject->DriverUnload               = 0;//AtaXUnload;
  DriverObject->DriverStartIo              = 0;//AtaXStartIo;

  DriverObject->MajorFunction[IRP_MJ_CREATE]                  = 0;//AtaXCompleteIrp;
  DriverObject->MajorFunction[IRP_MJ_CLOSE]                   = 0;//AtaXCompleteIrp;
  DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]          = 0;//AtaXDispatchDeviceControl;
  DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = 0;//AtaXDispatchScsi;          // IRP_MJ_SCSI == IRP_MJ_INTERNAL_DEVICE_CONTROL
  DriverObject->MajorFunction[IRP_MJ_POWER]                   = 0;//AtaXCompleteIrp;           // AtaXDispatchPower;
  DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL]          = 0;//AtaXCompleteIrp;           // AtaXDispatchSystemControl;
  DriverObject->MajorFunction[IRP_MJ_PNP]                     = 0;//AtaXDispatchPnp;

  DPRINT1("ATAX DriverEntry: return STATUS_SUCCESS\n" );
  return STATUS_SUCCESS;
}
