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
AtaXWaitOnBaseBusy(IN PATAX_REGISTERS_1 AtaXRegisters1)
{
  ULONG ix;
  UCHAR Status;

  for ( ix = 0; ix < 20000; ix++ )
  {
    Status = READ_PORT_UCHAR(AtaXRegisters1->Status);
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

BOOLEAN
AtaXStartPacket(IN PVOID Context)
{
  PFDO_CHANNEL_EXTENSION    AtaXChannelFdoExtension;
  PPDO_DEVICE_EXTENSION     AtaXDevicePdoExtension;
  PIO_STACK_LOCATION        IoStack;
  PSCSI_REQUEST_BLOCK       Srb;
  PDEVICE_OBJECT            DeviceObject;
  BOOLEAN                   Result;

  DPRINT("AtaXStartPacket: Context - %p\n", Context);

  DeviceObject = (PDEVICE_OBJECT)Context;

  IoStack = IoGetCurrentIrpStackLocation(DeviceObject->CurrentIrp);
  Srb = IoStack->Parameters.Scsi.Srb;

  AtaXChannelFdoExtension = (PFDO_CHANNEL_EXTENSION)DeviceObject->DeviceExtension;
  AtaXDevicePdoExtension  = (PPDO_DEVICE_EXTENSION)AtaXChannelFdoExtension->AtaXDevicePdo[Srb->TargetId]->DeviceExtension;

  if ( AtaXChannelFdoExtension->InterruptData.Flags & ATAX_RESET )
  {
    AtaXChannelFdoExtension->InterruptData.Flags |= ATAX_RESET_REQUEST;
    return TRUE;
  }

  AtaXChannelFdoExtension->TimerCount = Srb->TimeOutValue;
  AtaXChannelFdoExtension->Flags |= ATAX_DEVICE_BUSY;

  if ( Srb->SrbFlags & SRB_FLAGS_BYPASS_FROZEN_QUEUE )
  {
    if ( Srb->Function == SRB_FUNCTION_ABORT_COMMAND )
    {
ASSERT(FALSE);
    }
    else
    {
      AtaXDevicePdoExtension->QueueCount++;
    }
  }
  else
  {
    if ( Srb->SrbFlags & SRB_FLAGS_DISABLE_DISCONNECT )
      AtaXChannelFdoExtension->Flags &= ~ATAX_DISCONNECT_ALLOWED;

    AtaXDevicePdoExtension->Flags |= ATAX_LU_ACTIVE;
    AtaXDevicePdoExtension->QueueCount++;
  }

  Srb->SrbFlags |= SRB_FLAGS_IS_ACTIVE;
  Result = StartIo(AtaXChannelFdoExtension, Srb);

  if ( AtaXChannelFdoExtension->InterruptData.Flags & ATAX_NOTIFICATION_NEEDED )
    KeInsertQueueDpc(&AtaXChannelFdoExtension->Dpc, NULL, NULL);

  DPRINT("AtaXStartPacket: Result - %x\n", Result);
  return Result;
}

NTSTATUS
SynchronizeStartPacket(PDEVICE_OBJECT AtaXChannelFdo)
{
  PFDO_CHANNEL_EXTENSION  AtaXChannelFdoExtension;
  KIRQL                   OldIrql;

  DPRINT("SynchronizeStartPacket: ... \n");
  AtaXChannelFdoExtension = (PFDO_CHANNEL_EXTENSION)AtaXChannelFdo->DeviceExtension;

  KeAcquireSpinLock(&AtaXChannelFdoExtension->SpinLock, &OldIrql);

  KeSynchronizeExecution(AtaXChannelFdoExtension->InterruptObject,
                              (PKSYNCHRONIZE_ROUTINE)AtaXStartPacket,
                              AtaXChannelFdo);

  KeReleaseSpinLock(&AtaXChannelFdoExtension->SpinLock, OldIrql);

  DPRINT(" SynchronizeStartPacket: return - STATUS_SUCCESS\n");
  return STATUS_SUCCESS;
}

VOID NTAPI
AtaXStartIo(
    IN PDEVICE_OBJECT AtaXChannelFdo,
    IN PIRP Irp)
{
  PFDO_CHANNEL_EXTENSION    AtaXChannelFdoExtension;
  PPDO_DEVICE_EXTENSION     AtaXDevicePdoExtension;
  PIO_STACK_LOCATION        IoStack;
  PSCSI_REQUEST_BLOCK       Srb;
  PSCSI_REQUEST_BLOCK_INFO  SrbInfo;
//  NTSTATUS                  Status;

  DPRINT("AtaXStartIo ... \n");
  AtaXChannelFdoExtension = (PFDO_CHANNEL_EXTENSION)AtaXChannelFdo->DeviceExtension;
  ASSERT(AtaXChannelFdoExtension);
  ASSERT(AtaXChannelFdoExtension->CommonExtension.IsFDO);

  IoStack = IoGetCurrentIrpStackLocation((PIRP)Irp);
  Srb = (PSCSI_REQUEST_BLOCK)IoStack->Parameters.Scsi.Srb;

  AtaXDevicePdoExtension = (AtaXChannelFdoExtension->AtaXDevicePdo[Srb->TargetId])->DeviceExtension;

  Srb->SrbFlags |= AtaXChannelFdoExtension->SrbFlags;  // "default" flags

  SrbInfo = &AtaXDevicePdoExtension->SrbInfo;
  Srb->SrbExtension = NULL;
  Srb->QueueTag = SP_UNTAGGED;

  if ( !SrbInfo->SequenceNumber )
  {
    AtaXChannelFdoExtension->SequenceNumber++;
    SrbInfo->SequenceNumber = AtaXChannelFdoExtension->SequenceNumber;
  }

  if ( Srb->Function == SRB_FUNCTION_ABORT_COMMAND )
  {
    DPRINT1("Abort command! Unimplemented now\n");
ASSERT(FALSE);
  }
  else
  {
    SrbInfo->Srb = Srb;
  } 

  if ( Srb->SrbFlags & (SRB_FLAGS_DATA_IN | SRB_FLAGS_DATA_OUT) )
  {
    DPRINT(" AtaXStartIo: Srb->SrbFlags & (SRB_FLAGS_DATA_IN | SRB_FLAGS_DATA_OUT) == TRUE \n");

    if ( Irp->MdlAddress != NULL )
    {
      DPRINT(" AtaXStartIo: Irp->MdlAddress - %p\n", Irp->MdlAddress);
      SrbInfo->DataOffset = MmGetMdlVirtualAddress(Irp->MdlAddress);
    }

    DPRINT(" AtaXStartIo: Srb->SrbFlags - %x\n", Srb->SrbFlags);

    if ( Srb->SrbFlags & SRB_FLAGS_USE_DMA )
    {
      PBUS_MASTER_INTERFACE BusMasterInterface;

      BusMasterInterface = &AtaXChannelFdoExtension->BusMasterInterface;
  
      if ( BusMasterInterface->BusMasterPrepare )
      {
 ASSERT(FALSE);
/*       Status = BusMasterInterface->BusMasterPrepare(
                          BusMasterInterface->ChannelPdoExtension,
                          Srb->DataBuffer,
                          Srb->DataTransferLength,
                          Irp->MdlAddress,
                          (Srb->SrbFlags & SRB_FLAGS_DATA_OUT) == SRB_FLAGS_DATA_OUT,
                          SynchronizeStartPacket,
                          AtaXChannelFdo);
        DPRINT(" AtaXStartIo: BusMasterPrepare return - %x\n", Status);

        if ( NT_SUCCESS(Status) )
          return;
  
        DPRINT("AtaXStartIo: BusMasterPrepare fail. Status - %x. Enable PIO mode.\n", Status);
        Srb->SrbFlags &= ~SRB_FLAGS_USE_DMA;  // PIO mode
*/
      }
    }
  }
  else
  {
    DPRINT(" AtaXStartIo: Srb->SrbFlags & (SRB_FLAGS_DATA_IN | SRB_FLAGS_DATA_OUT) == FALSE  \n");
    ASSERT(FALSE);
  }

  Srb->SrbFlags &= ~SRB_FLAGS_USE_DMA; // use PIO
  DPRINT(" AtaXStartIo: Srb->SrbFlags - %x\n", Srb->SrbFlags);

  if ( SynchronizeStartPacket(AtaXChannelFdo) < 0 )
  {
    DPRINT("AtaXStartIo: SynchronizeStartPacket() failed!\n");
    Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
  }

  // HACK!!!
  if ( (ULONG)Srb->DataBuffer < 0x80000000 )
  {
    do  
    {
      KeStallExecutionProcessor(1);
      DPRINT("AtaXStartIo: Srb->DataBuffer - %p, KeStallExecutionProcessor(1)\n", Srb->DataBuffer);
    } while ( AtaXChannelFdoExtension->ExpectingInterrupt == TRUE );
  }

  DPRINT("AtaXStartIo: exit\n");
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
  NTSTATUS Status;

  DPRINT("AtaXDispatchScsi: ... \n");
  if (((PCOMMON_ATAX_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->IsFDO)
  {
    ASSERT(FALSE);
    return STATUS_NOT_SUPPORTED;
  }
  else
  {
    Status = AtaXDevicePdoDispatchScsi(DeviceObject, Irp);
    DPRINT("AtaXDispatchScsi: AtaXDevicePdoDispatchScsi return - %x\n", Status);
    return Status;
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
