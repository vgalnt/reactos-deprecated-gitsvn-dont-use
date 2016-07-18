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
AtaXWaitForDrq(IN PATAX_REGISTERS_2 AtaXRegisters2)
{
  ULONG ix;
  UCHAR Status;

  for ( ix = 0; ix < 1000; ix++ )
  {
    Status = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus);

    if ( Status & IDE_STATUS_BUSY )
      KeStallExecutionProcessor(50);
    else if ( Status & IDE_STATUS_DRQ )
      break;
    else
      KeStallExecutionProcessor(50);
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

ULONG 
AtaXMapError(
    IN PFDO_CHANNEL_EXTENSION AtaXChannelFdoExtension,
    IN PSCSI_REQUEST_BLOCK Srb)
{
  PATAX_REGISTERS_1  AtaXRegisters1 = &AtaXChannelFdoExtension->BaseIoAddress1;
  UCHAR              ErrorByte;
  UCHAR              SrbStatus = SRB_STATUS_ERROR;
  UCHAR              ScsiStatus=0;

  DPRINT("AtaXMapError: AtaXChannelFdoExtension - %p, Srb - %p\n", AtaXChannelFdoExtension, Srb);

  ErrorByte = READ_PORT_UCHAR(AtaXRegisters1->Error);  // Read the error register.
  DPRINT("AtaXMapError: Error register is %x\n", ErrorByte);

  if ( AtaXChannelFdoExtension->DeviceFlags[Srb->TargetId] & DFLAGS_ATAPI_DEVICE )
  {
ASSERT(FALSE);
  }
  else
  {
ASSERT(FALSE);
  }

  Srb->ScsiStatus = ScsiStatus;  // Set SCSI status to indicate a check condition.
  return SrbStatus;
}

PSCSI_REQUEST_BLOCK_INFO
AtaXGetSrbData(IN PSCSI_REQUEST_BLOCK Srb)
{
  PSCSI_REQUEST_BLOCK_INFO  SrbData;
  PDEVICE_OBJECT            AtaXDevicePdo;
  PPDO_DEVICE_EXTENSION     AtaXDevicePdoExtension;
  PIRP                      Irp;

  Irp = Srb->OriginalRequest;

  if ( Irp )
  {
    AtaXDevicePdo = IoGetCurrentIrpStackLocation(Irp)->DeviceObject;
    AtaXDevicePdoExtension = (PPDO_DEVICE_EXTENSION)AtaXDevicePdo->DeviceExtension;
    SrbData = &AtaXDevicePdoExtension->SrbInfo;
  }
  else
  { 
    SrbData = 0;
  }

  return SrbData;
}


VOID
AtaXNotification(
    IN SCSI_NOTIFICATION_TYPE NotificationType,
    IN PFDO_CHANNEL_EXTENSION AtaXChannelFdoExtension, ...)
{
  va_list  ap;

  DPRINT("AtaXNotification: NotificationType - %x\n", NotificationType);

  va_start(ap, AtaXChannelFdoExtension);

  switch ( NotificationType )
  {
    case RequestComplete:         /*  0  */
    {
      PSCSI_REQUEST_BLOCK       Srb;
      PSCSI_REQUEST_BLOCK_INFO  SrbData;

      Srb = (PSCSI_REQUEST_BLOCK) va_arg (ap, PSCSI_REQUEST_BLOCK);

      DPRINT("AtaXNotification: RequestComplete (Srb %p)\n", Srb);

      ASSERT(Srb->SrbStatus != SRB_STATUS_PENDING);
      ASSERT(Srb->Function != SRB_FUNCTION_EXECUTE_SCSI ||
             Srb->SrbStatus != SRB_STATUS_SUCCESS || Srb->ScsiStatus == SCSISTAT_GOOD);

      if ( !(Srb->SrbFlags & SRB_FLAGS_IS_ACTIVE) )
      {
        va_end(ap);
        return;
      }

      Srb->SrbFlags &= ~SRB_FLAGS_IS_ACTIVE;

      if ( Srb->Function == SRB_FUNCTION_ABORT_COMMAND )
      {
        // TODO
        ASSERT(FALSE);
      }
      else
      {
        SrbData = AtaXGetSrbData(Srb);

        ASSERT(SrbData->CompletedRequests == NULL);
        ASSERT(SrbData->Srb != NULL);

        if ( (Srb->SrbStatus == SRB_STATUS_SUCCESS) &&
            ((Srb->Cdb[0] == SCSIOP_READ) || (Srb->Cdb[0] == SCSIOP_WRITE)) )
        {
          ASSERT(Srb->DataTransferLength);
        }

        SrbData->CompletedRequests = AtaXChannelFdoExtension->InterruptData.CompletedRequests;
        AtaXChannelFdoExtension->InterruptData.CompletedRequests = SrbData;
      }

      break;
    }
  
    case NextRequest:             /*  1  */
      DPRINT("AtaXNotification: NextRequest\n");
      AtaXChannelFdoExtension->InterruptData.Flags |= ATAX_NEXT_REQUEST_READY;
      break;
  
    case NextLuRequest:           /*  2  */
      DPRINT1("AtaXNotification: NextLuRequest\n");
      //DPRINT("AtaXNotification: NextLuRequest(PathId %u  TargetId %u  Lun %u)\n", PathId, TargetId, Lun);
      AtaXChannelFdoExtension->InterruptData.Flags |= ATAX_NEXT_REQUEST_READY;
      break;

    case ResetDetected:           /*  3  */
      DPRINT1("AtaXNotification: ResetDetected\n");
      AtaXChannelFdoExtension->InterruptData.Flags |= ATAX_RESET | ATAX_RESET_REPORTED;
      break;

    case CallDisableInterrupts:   /*  4  */
      DPRINT1("AtaXNotification: UNIMPLEMENTED SCSI Notification called: CallDisableInterrupts!\n");
      break;

    case CallEnableInterrupts:    /*  5  */
      DPRINT1("AtaXNotification: UNIMPLEMENTED SCSI Notification called: CallEnableInterrupts!\n");
      break;

    case RequestTimerCall:        /*  6  */
      DPRINT("AtaXNotification: : RequestTimerCall\n");
      AtaXChannelFdoExtension->InterruptData.Flags |= ATAX_TIMER_NEEDED;
      break;

    case BusChangeDetected:       /*  7  */
      DPRINT1("AtaXNotification: UNIMPLEMENTED SCSI Notification called: BusChangeDetected!\n");
      break;

    case WMIEvent:                /*  8  */
      DPRINT1("AtaXNotification: UNIMPLEMENTED SCSI Notification called: WMIEvent!\n");
      break;

    case WMIReregister:           /*  9  */
      DPRINT1("AtaXNotification: UNIMPLEMENTED SCSI Notification called: WMIReregister!\n");
      break;

    case LinkUp:                  /* 0xA */
      DPRINT1("AtaXNotification: UNIMPLEMENTED SCSI Notification called: LinkUp!\n");
      break;

    case LinkDown:                /* 0xB */
      DPRINT1("AtaXNotification: UNIMPLEMENTED SCSI Notification called: LinkDown!\n");
      break;

    case QueryTickCount:          /* 0xC */
      DPRINT1("AtaXNotification: UNIMPLEMENTED SCSI Notification called: QueryTickCount!\n");
      break;

    case BufferOverrunDetected:   /* 0xD */
      DPRINT1("AtaXNotification: UNIMPLEMENTED SCSI Notification called: BufferOverrunDetected!\n");
      break;

    case TraceNotification:       /* 0xE */
      DPRINT1("AtaXNotification: UNIMPLEMENTED SCSI Notification called: TraceNotification!\n");
      break;

    default:
      DPRINT1 ("AtaXNotification: Unknown notification type: %lu\n", NotificationType);
      break;
  }

  va_end(ap);

  // запрос DPC после обработки прерывания
  AtaXChannelFdoExtension->InterruptData.Flags |= ATAX_NOTIFICATION_NEEDED;
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
