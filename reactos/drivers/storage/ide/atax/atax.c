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
AtaXSataWaitOnBusy(IN PATAX_REGISTERS_1 AtaXRegisters1)
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
AtaXSataWaitForDrq(IN PATAX_REGISTERS_1 AtaXRegisters1)
{
  ULONG ix;
  UCHAR Status;

  for ( ix = 0; ix < 1000; ix++ )
  {
    Status = READ_PORT_UCHAR(AtaXRegisters1->Status);

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

VOID NTAPI 
AtaXSataSoftReset(
    IN PATAX_REGISTERS_1 AtaXRegisters1,
    IN ULONG DeviceNumber)
{
  ULONG ix = 1000*1000;
  
  WRITE_PORT_UCHAR(AtaXRegisters1->DriveSelect,(UCHAR)(((DeviceNumber & 1) << 4) | IDE_DRIVE_SELECT));
  KeStallExecutionProcessor(500);
 
  WRITE_PORT_UCHAR(AtaXRegisters1->Command, IDE_COMMAND_ATAPI_RESET);
  
  while ((READ_PORT_UCHAR(AtaXRegisters1->Status) & IDE_STATUS_BUSY) && ix--)
    KeStallExecutionProcessor(25);
  
  WRITE_PORT_UCHAR(AtaXRegisters1->DriveSelect,(UCHAR)((DeviceNumber << 4) | IDE_DRIVE_SELECT));
  AtaXSataWaitOnBusy(AtaXRegisters1);
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

  // запрос DPC после обработки прерывани€
  AtaXChannelFdoExtension->InterruptData.Flags |= ATAX_NOTIFICATION_NEEDED;
}

NTSTATUS
AtaReadWrite(
    IN PFDO_CHANNEL_EXTENSION AtaXChannelFdoExtension,
    IN PSCSI_REQUEST_BLOCK Srb)
{
  PATAX_REGISTERS_1   AtaXRegisters1;
  PIDENTIFY_DATA      IdentifyData;
  UCHAR               StatusByte;
  UCHAR               StatusByte2;
  ULONG               StartingSector;
  ULONG               WordCount;
  USHORT              SectorsPerTrack;
  USHORT              Heads;
  ULONG               ix;
  ULONG               DeviceNumber;
  UCHAR               HighLBA;
  UCHAR               MidLBA;
  UCHAR               LowLBA;
  UCHAR               DriveSelect;

  DPRINT("AtaReadWrite: ... \n");

  DeviceNumber = Srb->TargetId & 1;

  IdentifyData = &AtaXChannelFdoExtension->FullIdentifyData[DeviceNumber];

  SectorsPerTrack = IdentifyData->NumSectorsPerTrack;
  Heads           = IdentifyData->NumHeads;

  AtaXRegisters1 = &AtaXChannelFdoExtension->BaseIoAddress1;

  // ¬ыбираем устройство по DeviceNumber
  WRITE_PORT_UCHAR(AtaXRegisters1->DriveSelect, (UCHAR)((DeviceNumber << 4) | IDE_DRIVE_SELECT));
  StatusByte2 = READ_PORT_UCHAR(AtaXRegisters1->Status);

  if ( StatusByte2 & IDE_STATUS_BUSY )
  {
    DPRINT("AtaReadWrite: Returning BUSY status\n");
    Srb->SrbStatus = SRB_STATUS_BUSY;
    return STATUS_DEVICE_BUSY;
  }

  AtaXChannelFdoExtension->DataBuffer         = (PUSHORT)Srb->DataBuffer;
  AtaXChannelFdoExtension->WordsLeft          = Srb->DataTransferLength / 2;
  AtaXChannelFdoExtension->ExpectingInterrupt = TRUE;  // следующее прерывание - ожидаемое

  // устанавливаем регистр SectorCount
  WRITE_PORT_UCHAR(AtaXRegisters1->SectorCount, (UCHAR)((Srb->DataTransferLength + 0x1FF) / 0x200));

  // начальный сектор извлекаем из CDB
  StartingSector = ((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte3       |
                   ((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte2 << 8  |
                   ((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte1 << 16 |
                   ((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte0 << 24;

  DPRINT("AtaReadWrite: Starting sector is %x, Number of bytes %x\n", StartingSector, Srb->DataTransferLength);

  // устанавливаем регистр LowLBA
  LowLBA =  (UCHAR)((StartingSector % SectorsPerTrack) + 1);
  WRITE_PORT_UCHAR(AtaXRegisters1->LowLBA, LowLBA);

  // устанавливаем регистр MidLBA
  MidLBA =  (UCHAR)(StartingSector / (SectorsPerTrack * Heads));
  WRITE_PORT_UCHAR(AtaXRegisters1->MidLBA, MidLBA);

  // устанавливаем регистр HighLBA
  HighLBA = (UCHAR)((StartingSector / (SectorsPerTrack * Heads)) >> 8);
  WRITE_PORT_UCHAR(AtaXRegisters1->HighLBA, HighLBA);

  // устанавливаем регистр DriveSelect
  DriveSelect = (UCHAR)(((StartingSector / SectorsPerTrack) % Heads) | (DeviceNumber << 4) | 0xA0);
  WRITE_PORT_UCHAR(AtaXRegisters1->DriveSelect, DriveSelect);

  DPRINT("AtaReadWrite: Cylinder %x Head %x Sector %x\n",
          StartingSector  / (SectorsPerTrack  * Heads),
          (StartingSector /  SectorsPerTrack) % Heads,
          StartingSector  %  SectorsPerTrack + 1);

  // определ€ем - будем читать или записывать
  if ( Srb->SrbFlags & SRB_FLAGS_DATA_IN )
  {
    // отправл€ем команду чтени€
    if ( Srb->SrbFlags & SRB_FLAGS_USE_DMA )
    {
ASSERT(FALSE);
    }
    else
    {
      AtaXChannelFdoExtension->BusMaster = FALSE;
      WRITE_PORT_UCHAR(AtaXRegisters1->Command, IDE_COMMAND_READ);
    }
  }
  else
  {
    // отправл€ем команду записи
    if ( Srb->SrbFlags & SRB_FLAGS_USE_DMA )
    {
ASSERT(FALSE);
    }
    else
    {
      AtaXChannelFdoExtension->BusMaster = FALSE;

      if ( AtaXChannelFdoExtension->MaximumBlockXfer[DeviceNumber] )
      {
        DPRINT("AtaReadWrite: FIXME MaximumBlockXfer\n");
      }

      WordCount = 256;
      WRITE_PORT_UCHAR(AtaXRegisters1->Command, IDE_COMMAND_WRITE);
    }

    // ждем BSY и DRQ
    AtaXWaitOnBaseBusy(AtaXRegisters1);
    StatusByte = READ_PORT_UCHAR(AtaXRegisters1->Status);

    if ( StatusByte & IDE_STATUS_BUSY )
    {
      DPRINT("AtaReadWrite: Returning BUSY status %x\n", StatusByte);
      return SRB_STATUS_BUSY;
    }

    for ( ix = 0; ix < 1000; ix++ )
    {
      StatusByte = READ_PORT_UCHAR(AtaXRegisters1->Status);
      if ( StatusByte & IDE_STATUS_DRQ )
        break;
      KeStallExecutionProcessor(50);
    }

    if ( !(StatusByte & IDE_STATUS_DRQ) )
    {
      AtaXChannelFdoExtension->WordsLeft          = 0;
      AtaXChannelFdoExtension->ExpectingInterrupt = FALSE;    // следующее прерывание не ожидаемое
      AtaXChannelFdoExtension->CurrentSrb         = NULL;     // очищаем текущий запрос
      return SRB_STATUS_TIMEOUT;
    }

    // записываем сектор (256 words)
    DPRINT("AtaReadWrite: WRITE_PORT_BUFFER_USHORT %x, DataBuffer - %x, WordCount - %x\n",
            AtaXRegisters1->Data, AtaXChannelFdoExtension->DataBuffer, WordCount);

    WRITE_PORT_BUFFER_USHORT(AtaXRegisters1->Data, AtaXChannelFdoExtension->DataBuffer, WordCount);
  
    // корректируем адрес буфера данных и кол-во оставшихс€ дл€ записи слов
    AtaXChannelFdoExtension->WordsLeft -= WordCount;
    AtaXChannelFdoExtension->DataBuffer += WordCount;
  }

  // ждЄм аппаратное прерывание
  return STATUS_SUCCESS;
}

NTSTATUS
AtaSendCommand(
    IN PFDO_CHANNEL_EXTENSION AtaXChannelFdoExtension,
    IN PSCSI_REQUEST_BLOCK Srb)
{
  NTSTATUS  Status;

  DPRINT("AtaSendCommand: Command %x to device %d\n", Srb->Cdb[0], Srb->TargetId);
  ASSERT(AtaXChannelFdoExtension);

  switch ( Srb->Cdb[0] )
  {
    case SCSIOP_READ:              /* 0x28 */
    case SCSIOP_WRITE:             /* 0x2A */
      DPRINT("AtaSendCommand / SCSIOP_READ or SCSIOP_WRITE\n");
      ASSERT(AtaXChannelFdoExtension->DeviceFlags[Srb->TargetId] & DFLAGS_DEVICE_PRESENT); //if ATA
      AtaXChannelFdoExtension->CurrentSrb = Srb;
      Status = AtaReadWrite(AtaXChannelFdoExtension, Srb);
      DPRINT(" AtaSendCommand: return - %x\n", Status);
      return Status;

    case SCSIOP_TEST_UNIT_READY:
      DPRINT("AtaSendCommand / SCSIOP_TEST_UNIT_READY FIXME\n");
      Status = SRB_STATUS_INVALID_REQUEST;
      break;

    default:
      DPRINT("AtaSendCommand: Unsupported command FIXME%x\n", Srb->Cdb[0]);
      Status = SRB_STATUS_INVALID_REQUEST;
      break;
  }
 
  DPRINT(" AtaSendCommand: return - %x\n", Status);
  return Status;
}

VOID
Scsi2Atapi(IN PSCSI_REQUEST_BLOCK Srb)
{
  Srb->CdbLength = 12;

  switch (Srb->Cdb[0])
  {
    case SCSIOP_MODE_SENSE:
    {
      PMODE_SENSE_10 modeSense10;
      UCHAR PageCode;
      UCHAR Length;

      modeSense10 = (PMODE_SENSE_10)Srb->Cdb;
      PageCode = ((PCDB)Srb->Cdb)->MODE_SENSE.PageCode;
      Length = ((PCDB)Srb->Cdb)->MODE_SENSE.AllocationLength;

      RtlZeroMemory(Srb->Cdb, MAXIMUM_CDB_SIZE);

      modeSense10->OperationCode = ATAPI_MODE_SENSE;
      modeSense10->PageCode = PageCode;
      modeSense10->ParameterListLengthMsb = 0;
      modeSense10->ParameterListLengthLsb = Length;
      break;
    }

    case SCSIOP_MODE_SELECT:
    {
      PMODE_SELECT_10 modeSelect10;
      UCHAR Length;

      modeSelect10 = (PMODE_SELECT_10)Srb->Cdb;
      Length = ((PCDB)Srb->Cdb)->MODE_SELECT.ParameterListLength;

      RtlZeroMemory(Srb->Cdb, MAXIMUM_CDB_SIZE);

      modeSelect10->OperationCode = ATAPI_MODE_SELECT;
      modeSelect10->PFBit = 1;
      modeSelect10->ParameterListLengthMsb = 0;
      modeSelect10->ParameterListLengthLsb = Length;
      break;
    }

    case SCSIOP_FORMAT_UNIT:
      Srb->Cdb[0] = ATAPI_FORMAT_UNIT;
      break;
  }
}

NTSTATUS
AtapiSendCommand(
    IN PFDO_CHANNEL_EXTENSION AtaXChannelFdoExtension,
    IN PSCSI_REQUEST_BLOCK Srb)
{
  PATAX_REGISTERS_1  AtaXRegisters1;
  PATAX_REGISTERS_2  AtaXRegisters2;
  UCHAR              ByteCountLow;
  UCHAR              ByteCountHigh;
  ULONG              Flags;
  UCHAR              StatusByte;
  ULONG              ix;
  ULONG              DeviceNumber;

  DPRINT("AtapiSendCommand: Command %x to device %d\n", Srb->Cdb[0], Srb->TargetId);
  
  ASSERT(AtaXChannelFdoExtension);
  AtaXRegisters1 = &AtaXChannelFdoExtension->BaseIoAddress1;
  AtaXRegisters2 = &AtaXChannelFdoExtension->BaseIoAddress2;

  Flags = AtaXChannelFdoExtension->DeviceFlags[Srb->TargetId];
  if ( !(Flags & DFLAGS_ATAPI_DEVICE) )
    return SRB_STATUS_SELECTION_TIMEOUT;  //не ATAPI устройство

  DeviceNumber = Srb->TargetId & 1;

  // ¬ыбираем устройство
  WRITE_PORT_UCHAR(AtaXRegisters1->DriveSelect, (UCHAR)((DeviceNumber << 4) | IDE_DRIVE_SELECT));

  // —читываем альтернативный регистр состо€ни€
  StatusByte = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus);
  DPRINT("AtapiSendCommand: Entered with StatusByte - %x\n", StatusByte);

  if ( StatusByte & IDE_STATUS_BUSY )
  {
    DPRINT("AtapiSendCommand: Device busy (%x)\n", StatusByte);
    return SRB_STATUS_BUSY;
  }

  if ( StatusByte & IDE_STATUS_ERROR )
  {
    if ( Srb->Cdb[0] != SCSIOP_REQUEST_SENSE )
    {
      DPRINT("AtapiSendCommand: Error on entry: (%x) \n", StatusByte);
      return AtaXMapError(AtaXChannelFdoExtension, Srb);
    }
  }

  if ( IS_RDP(Srb->Cdb[0]) )
  {
    AtaXChannelFdoExtension->RDP = TRUE;
    DPRINT("AtapiSendCommand: %x mapped as DSC restrictive\n", Srb->Cdb[0]);
  }
  else
  {
    AtaXChannelFdoExtension->RDP = FALSE;
  }

  if ( StatusByte & IDE_STATUS_DRQ )
  {
    DPRINT("AtapiSendCommand: Entered with status (%x). Attempting to recover.\n", StatusByte);

    for ( ix = 0; ix < 0x10000; ix++ )
    {
      // —читываем альтернативный регистр состо€ни€
      StatusByte = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus);

      if ( StatusByte & IDE_STATUS_DRQ )
        READ_PORT_USHORT(AtaXRegisters1->Data);
      else
        break;
    }

    if ( ix == 0x10000 )
    {
      DPRINT("AtapiSendCommand: DRQ still asserted.Status (%x)\n", StatusByte);
      AtaXSoftReset(AtaXRegisters1, AtaXRegisters2, DeviceNumber);

      DPRINT("AtapiSendCommand: Issued soft reset to Atapi device. \n");
      // инициализируем Atapi устройство
      AtaXIssueIdentify(AtaXChannelFdoExtension,
                        &AtaXChannelFdoExtension->FullIdentifyData[DeviceNumber],
                        DeviceNumber,
                        IDE_COMMAND_ATAPI_IDENTIFY);

      // —ообщаем драйверу, что шина была сброшена
      AtaXNotification(ResetDetected, AtaXChannelFdoExtension, 0);

      // —брасываем пол€ аппаратного расширени€
      AtaXChannelFdoExtension->ExpectingInterrupt = FALSE;
      AtaXChannelFdoExtension->RDP                = FALSE;

      return SRB_STATUS_BUS_RESET;
    }
  }

  // ѕреобразуем SCSI-команды в ATAPI команды
  switch ( Srb->Cdb[0] )
  {
    case SCSIOP_MODE_SENSE:
    case SCSIOP_MODE_SELECT:
    case SCSIOP_FORMAT_UNIT:
      if ( !(Flags & DFLAGS_TAPE_DEVICE) )
        Scsi2Atapi(Srb);
    break;
  }

  AtaXChannelFdoExtension->DataBuffer = (PUSHORT)Srb->DataBuffer;   // указатель буфера данных
  AtaXChannelFdoExtension->WordsLeft = Srb->DataTransferLength / 2; // осталось передать слов

  AtaXWaitOnBusy(AtaXRegisters2);
  StatusByte = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus);

  // устанавливаем количество байт пересылки в соотвктствующие регистры
  ByteCountLow = (UCHAR)(Srb->DataTransferLength & 0xFF);
  ByteCountHigh = (UCHAR)(Srb->DataTransferLength >> 8);

  if ( Srb->DataTransferLength >= 0x10000 )
    ByteCountLow = ByteCountHigh = 0xFF;

  WRITE_PORT_UCHAR(AtaXRegisters1->ByteCountLow, ByteCountLow);
  WRITE_PORT_UCHAR(AtaXRegisters1->ByteCountHigh, ByteCountHigh);
  WRITE_PORT_UCHAR(AtaXRegisters1->Features, (Srb->SrbFlags & SRB_FLAGS_USE_DMA) != 0);

  // отправл€ем команду IDE_COMMAND_ATAPI_PACKET (0xA0)
  WRITE_PORT_UCHAR(AtaXRegisters1->Command, IDE_COMMAND_ATAPI_PACKET);

  if ( Flags & DFLAGS_INT_DRQ )
  {
    // это устройство выдает прерывание, когда готово получать пакет
    DPRINT("AtapiSendCommand: Wait for interrupt to send packet. Status (%x)\n", StatusByte);
    AtaXChannelFdoExtension->ExpectingInterrupt = TRUE;
    return SRB_STATUS_PENDING;
  }
  else
  {
    // ждем DRQ
    AtaXWaitOnBusy(AtaXRegisters2);
    AtaXWaitForDrq(AtaXRegisters2);
    StatusByte = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus);

    if ( !(StatusByte & IDE_STATUS_DRQ) )
    {
      DPRINT("AtapiSendCommand: DRQ never asserted (%x)\n", StatusByte);
      return SRB_STATUS_ERROR;
    }
  }

  // считываем регистр состо€ни€, тем самым сбрасываем прерывание
  StatusByte = READ_PORT_UCHAR(AtaXRegisters1->Status);
  AtaXWaitOnBusy(AtaXRegisters2);

if ( 0 ) //debug messages
{
  UCHAR ix;

  for ( ix = 0; ix < Srb->CdbLength; ix++ )
  {
    DPRINT("AtapiSendCommand: Srb->Cdb[%d] - %x\n", ix, *((PUCHAR)Srb->Cdb + ix));
  }
}

  // отправл€ем CDB устройству
  WRITE_PORT_BUFFER_USHORT(AtaXRegisters1->Data, (PUSHORT)Srb->Cdb, 6);

  // будем ждать запланированное прерывание
  AtaXChannelFdoExtension->ExpectingInterrupt = TRUE;

  if ( Srb->SrbFlags & SRB_FLAGS_USE_DMA )
  {
    //PBUS_MASTER_INTERFACE BusMasterInterface;

    DPRINT("AtapiSendCommand: SRB_FLAGS_USE_DMA\n");

    //BusMasterInterface = &AtaXChannelFdoExtension->BusMasterInterface;
    AtaXChannelFdoExtension->BusMaster = TRUE;
ASSERT(FALSE);
    //BusMasterInterface->BusMasterStart(BusMasterInterface->ChannelPdoExtension);
  }

  DPRINT(" AtapiSendCommand: return - %x\n", SRB_STATUS_PENDING);
  return SRB_STATUS_PENDING;
}

NTSTATUS
AtaXSendCommand(
    IN PFDO_CHANNEL_EXTENSION AtaXChannelFdoExtension,
    IN PSCSI_REQUEST_BLOCK Srb)
{
  NTSTATUS  Status;

  ASSERT(AtaXChannelFdoExtension);
  ASSERT(AtaXChannelFdoExtension->CommonExtension.IsFDO);

  if ( AtaXChannelFdoExtension->DeviceFlags[Srb->TargetId] & DFLAGS_ATAPI_DEVICE )
  {
    Status = AtapiSendCommand(AtaXChannelFdoExtension, Srb);
  }
  else if ( AtaXChannelFdoExtension->DeviceFlags[Srb->TargetId] & DFLAGS_DEVICE_PRESENT )
  {
    Status = AtaSendCommand(AtaXChannelFdoExtension, Srb);
  }
  else
  {
    Status = SRB_STATUS_SELECTION_TIMEOUT;
  }

  return Status;
}

BOOLEAN
StartIo(
    IN PFDO_CHANNEL_EXTENSION  AtaXChannelFdoExtension,
    IN PSCSI_REQUEST_BLOCK Srb)
{
  NTSTATUS  Status;

  DPRINT("StartIo: ... \n");
  ASSERT(AtaXChannelFdoExtension);
  ASSERT(AtaXChannelFdoExtension->CommonExtension.IsFDO);

  switch ( Srb->Function )
  {
    case SRB_FUNCTION_EXECUTE_SCSI:
      DPRINT("StartIo / SRB_FUNCTION_EXECUTE_SCSI \n");
      // только один запрос может быть текущим на канале
      if ( AtaXChannelFdoExtension->CurrentSrb )
      {
        DPRINT("StartIo: Already have a request!\n");

        Srb->SrbStatus = SRB_STATUS_BUSY;

        AtaXNotification(RequestComplete,
                         AtaXChannelFdoExtension,
                         Srb);
        return FALSE;
      }

      // запрос €вл€етс€ активным на канале
      AtaXChannelFdoExtension->CurrentSrb = Srb;

      // отправл€ем команду устройству
      Status = AtaXSendCommand(AtaXChannelFdoExtension, Srb);
      break;

    case SRB_FUNCTION_IO_CONTROL:
      DPRINT("StartIo: SRB_FUNCTION_IO_CONTROL / ControlCode %x\n", ((PSRB_IO_CONTROL)(Srb->DataBuffer))->ControlCode );
ASSERT(FALSE);
      break;

    default:
      DPRINT("StartIo: Srb->Function - %x FIXME\n", Srb->Function);
      Status = SRB_STATUS_INVALID_REQUEST;
      break;
  }

  if ( Status != SRB_STATUS_PENDING )
  {
    DPRINT("StartIo: Srb %p complete with Status %x\n", Srb, Status);

    AtaXChannelFdoExtension->CurrentSrb = NULL;
    Srb->SrbStatus = (UCHAR)Status;

    AtaXNotification(RequestComplete,
                     AtaXChannelFdoExtension,
                     Srb);

    AtaXNotification(NextRequest,
                     AtaXChannelFdoExtension,
                     NULL);
  }

  DPRINT("StartIo: return TRUE\n");
  return TRUE;
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
    //DPRINT(" AtaXStartIo: Srb->SrbFlags & (SRB_FLAGS_DATA_IN | SRB_FLAGS_DATA_OUT) == TRUE \n");

    if ( Irp->MdlAddress != NULL )
    {
      //DPRINT(" AtaXStartIo: Irp->MdlAddress - %p\n", Irp->MdlAddress);
      SrbInfo->DataOffset = MmGetMdlVirtualAddress(Irp->MdlAddress);
    }

    //DPRINT(" AtaXStartIo: Srb->SrbFlags - %x\n", Srb->SrbFlags);

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

  Srb->SrbFlags &= ~SRB_FLAGS_USE_DMA; // use PIO
  //DPRINT(" AtaXStartIo: Srb->SrbFlags - %x\n", Srb->SrbFlags);

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
    return AtaXDevicePdoDeviceControl(DeviceObject, Irp);
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
    return AtaXDevicePdoDispatchPnp(DeviceObject, Irp);
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

  // —оздаем директорию "\Device\Ide"
  AtaXCreateIdeDirectory();

  DPRINT1("ATAX DriverEntry: return STATUS_SUCCESS\n" );
  return STATUS_SUCCESS;
}
