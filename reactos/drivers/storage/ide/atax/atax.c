#include "atax.h"               

//#define NDEBUG
#include <debug.h>


ULONG AtaXChannelCounter  = 0;


VOID 
AtaXWaitOnBusy(IN PATAX_REGISTERS_2 AtaXRegisters2)
{
  ULONG ix;
  UCHAR Status;

  for ( ix = 0; ix < 20000; ix++ )
  {
    Status = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus);

    if ( Status & IDE_STATUS_BUSY )
    {
      KeStallExecutionProcessor(50);
      continue;
    }
    else
    {
      break;
    }
  }
}

VOID 
AtaXSoftReset(
    IN PATAX_REGISTERS_1 AtaXRegisters1,
    IN PATAX_REGISTERS_2 AtaXRegisters2,
    IN ULONG DeviceNumber)
{
  ULONG ix = 1000*1000;
  
  WRITE_PORT_UCHAR(AtaXRegisters1->DriveSelect,
                  (UCHAR)(((DeviceNumber & 1) << 4) | IDE_DRIVE_SELECT));

  KeStallExecutionProcessor(500);
 
  WRITE_PORT_UCHAR(AtaXRegisters1->Command, IDE_COMMAND_ATAPI_RESET);
  
  while ((READ_PORT_UCHAR(AtaXRegisters1->Status) & IDE_STATUS_BUSY) && ix--)
    KeStallExecutionProcessor(25);
  
  WRITE_PORT_UCHAR(AtaXRegisters1->DriveSelect,
                  (UCHAR)((DeviceNumber << 4) | IDE_DRIVE_SELECT));

  AtaXWaitOnBusy(AtaXRegisters2);

  KeStallExecutionProcessor(500);
}

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
  if ( ((PCOMMON_ATAX_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->IsFDO )
    return AtaXChannelFdoDispatchPnp(DeviceObject, Irp);
  else
ASSERT(FALSE);
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

VOID
AtaXCreateIdeDirectory(VOID)
{
  OBJECT_ATTRIBUTES  ObjectAttributes;
  UNICODE_STRING     ObjectName;
  HANDLE             DirectoryHandle;
  NTSTATUS           Result;

  RtlInitUnicodeString(&ObjectName, L"\\Device\\Ide");

  InitializeObjectAttributes(
                     &ObjectAttributes,
                     &ObjectName,
                     OBJ_CASE_INSENSITIVE | OBJ_PERMANENT,
                     NULL,
                     NULL);

  Result = ZwCreateDirectoryObject(
                     &DirectoryHandle,
                     DIRECTORY_ALL_ACCESS,
                     &ObjectAttributes);

  DPRINT("AtaXCreateIdeDirectory Result - %p \n", Result);
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

  // Создаем директорию "\Device\Ide"
  AtaXCreateIdeDirectory();

  DPRINT1("ATAX DriverEntry: return STATUS_SUCCESS\n" );
  return STATUS_SUCCESS;
}
