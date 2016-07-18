#include "atax.h"               

//#define NDEBUG
#include <debug.h>


ULONG  AtaXDeviceCounter = 0;


NTSTATUS
AtaXPassDownIrpAndForget(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
  PDEVICE_OBJECT LowerDevice;
  
  ASSERT(((PCOMMON_ATAX_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->IsFDO);
  LowerDevice = ((PFDO_CHANNEL_EXTENSION)
                 DeviceObject->DeviceExtension)->CommonExtension.LowerDevice;
  ASSERT(LowerDevice);
  
  IoSkipCurrentIrpStackLocation(Irp);

  DPRINT("Calling lower device %p [%wZ]\n", LowerDevice, &LowerDevice->DriverObject->DriverName);
  return IoCallDriver(LowerDevice, Irp);
}

IO_COMPLETION_ROUTINE AtaXGenericCompletion;
NTSTATUS NTAPI 
AtaXGenericCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context)
{
  //DPRINT("AtaXGenericCompletionRoutine \n" );

  if ( Irp->PendingReturned )
    KeSetEvent((PKEVENT)Context, IO_NO_INCREMENT, FALSE);

  return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
AtaXQueryBusInterface(IN PDEVICE_OBJECT AtaXChannelFdo)
{
  PFDO_CHANNEL_EXTENSION   AtaXChannelFdoExtension;
  KEVENT                   Event;
  PIRP                     Irp;
  IO_STATUS_BLOCK          IoStatus;
  PIO_STACK_LOCATION       IoStack;
  NTSTATUS                 Status;
  PBUS_INTERFACE_STANDARD  BusInterface;

  DPRINT("AtaXQueryBusInterface: \n");

  AtaXChannelFdoExtension = (PFDO_CHANNEL_EXTENSION)AtaXChannelFdo->DeviceExtension;

  KeInitializeEvent(&Event, NotificationEvent, FALSE);

  Irp = IoBuildSynchronousFsdRequest(
            IRP_MJ_PNP,
            AtaXChannelFdoExtension->CommonExtension.LowerDevice,
            NULL,
            0,
            NULL,
            &Event,
            &IoStatus);

  if ( Irp == NULL )
    return STATUS_INSUFFICIENT_RESOURCES;

  Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;

  IoStack = IoGetNextIrpStackLocation(Irp);

  IoStack->MajorFunction = IRP_MJ_PNP;
  IoStack->MinorFunction = IRP_MN_QUERY_INTERFACE;

  // HACK!!! (правильно использовать поле InterfaceType и GUID_***)
  // Size - это тип интерфейса (в отличии от InterfaceType)
  // 1: QueryControllerProperties
  // 3: QueryPciBusInterface
  // 5: QueryBusMasterInterface
  // 7: QueryAhciInterface
  // 9: QuerySataInterface
  IoStack->Parameters.QueryInterface.Size = 3; // QueryPciBusInterface

  //любой GUID, должен совпадать с GUID в обрабатывающей функции нижнего PDO (PciIdeXPdoPnpDispatch)
  IoStack->Parameters.QueryInterface.InterfaceType = (LPGUID)&GUID_BUS_INTERFACE_STANDARD;

  IoStack->Parameters.QueryInterface.Version = 1; //не используется
  IoStack->Parameters.QueryInterface.Interface = (PINTERFACE)&AtaXChannelFdoExtension->BusInterface;
  IoStack->Parameters.QueryInterface.InterfaceSpecificData = NULL; //не используется

  Status = IoCallDriver(AtaXChannelFdoExtension->CommonExtension.LowerDevice, Irp);

  if (Status == STATUS_PENDING)
  {
    KeWaitForSingleObject(
          &Event,
          Executive,
          KernelMode,
          FALSE,
          NULL);

    Status = IoStatus.Status;
  }

  DPRINT("AtaXQueryBusInterface: IRP_MN_QUERY_INTERFACE Status - %p\n", Status);

  if ( NT_SUCCESS(Status) )
  {
    BusInterface = AtaXChannelFdoExtension->BusInterface;
    DPRINT("AtaXQueryBusInterface: BusInterface    - %p\n", BusInterface);
  }
  else
  {
    return Status;
  }

  if ( BusInterface->Size == sizeof(BUS_INTERFACE_STANDARD) )
    Status = STATUS_SUCCESS;
  else
    Status = STATUS_INFO_LENGTH_MISMATCH;

  return Status;
}

NTSTATUS
AtaXQueryControllerProperties(
    IN PDEVICE_OBJECT AtaXChannelFdo)
{
  PFDO_CHANNEL_EXTENSION  AtaXChannelFdoExtension;
  KEVENT                  Event;
  PIRP                    Irp;
  IO_STATUS_BLOCK         IoStatus;
  PIO_STACK_LOCATION      IoStack;
  NTSTATUS                Status;

  DPRINT("AtaXQueryControllerProperties: ...\n");

  AtaXChannelFdoExtension = (PFDO_CHANNEL_EXTENSION)AtaXChannelFdo->DeviceExtension;

  KeInitializeEvent(&Event, NotificationEvent, FALSE);

  Irp = IoBuildSynchronousFsdRequest(
               IRP_MJ_PNP,
               AtaXChannelFdoExtension->CommonExtension.LowerDevice,
               NULL,
               0,
               NULL,
               &Event,
               &IoStatus);

  if ( Irp == NULL )
    return STATUS_INSUFFICIENT_RESOURCES;

  Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;

  IoStack = IoGetNextIrpStackLocation(Irp);
  IoStack->MajorFunction = IRP_MJ_PNP;
  IoStack->MinorFunction = IRP_MN_QUERY_INTERFACE;

  IoStack->Parameters.QueryInterface.Size = 1; //тип интерфейса = ControllerProperties
  IoStack->Parameters.QueryInterface.InterfaceType = (LPGUID)&GUID_BUS_INTERFACE_STANDARD; //не важно какой GUID, только чтобы совпадал с GUID в обрабатывающей функции
  IoStack->Parameters.QueryInterface.Version = 1; //не используется
  IoStack->Parameters.QueryInterface.Interface = (PINTERFACE)&AtaXChannelFdoExtension->HwDeviceExtension.ControllerProperties; // + AtaXChannelFdoExtension->HwDeviceExtension.MiniControllerExtension
  IoStack->Parameters.QueryInterface.InterfaceSpecificData = NULL; //не используется

  Status = IoCallDriver(AtaXChannelFdoExtension->CommonExtension.LowerDevice, Irp);

  if (Status == STATUS_PENDING)
  {
    KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
    Status = IoStatus.Status;
  }

  DPRINT("AtaXQueryControllerProperties: IRP_MN_QUERY_INTERFACE Status - %p\n", Status);

  if ( NT_SUCCESS(Status) )
  {
    PIDE_CONTROLLER_PROPERTIES  ControllerProperties;

    ControllerProperties = AtaXChannelFdoExtension->HwDeviceExtension.ControllerProperties;

    DPRINT("AtaXQueryControllerProperties: ControllerProperties    - %p\n", ControllerProperties);
    DPRINT("AtaXQueryControllerProperties: MiniControllerExtension - %p\n", AtaXChannelFdoExtension->HwDeviceExtension.MiniControllerExtension);

    if ( ControllerProperties )
    {
      DPRINT("ControllerProperties->SupportedTransferMode[0][0]   - %p\n", ControllerProperties->SupportedTransferMode[0][0]);
      DPRINT("ControllerProperties->IgnoreActiveBitForAtaDevice   - %p\n", ControllerProperties->IgnoreActiveBitForAtaDevice);
      DPRINT("ControllerProperties->AlwaysClearBusMasterInterrupt - %p\n", ControllerProperties->AlwaysClearBusMasterInterrupt);
      DPRINT("ControllerProperties->AlignmentRequirement          - %p\n", ControllerProperties->AlignmentRequirement);
      DPRINT("ControllerProperties->DefaultPIO                    - %p\n", ControllerProperties->DefaultPIO);
      DPRINT("ControllerProperties->PciIdeUdmaModesSupported      - %p\n", ControllerProperties->PciIdeUdmaModesSupported);
    }
  }
  else
  {
    return Status;
  }

  DPRINT("AtaXQueryControllerProperties: AtaXChannelFdoExtension->HwDeviceExtension.ControllerProperties->Size - %x\n", AtaXChannelFdoExtension->HwDeviceExtension.ControllerProperties->Size);

  if ( AtaXChannelFdoExtension->HwDeviceExtension.ControllerProperties->Size == sizeof(IDE_CONTROLLER_PROPERTIES) )
  {
    Status = STATUS_SUCCESS;
  }
  else
  {
    Status = STATUS_INFO_LENGTH_MISMATCH;
  }

  return Status;
}

NTSTATUS
AtaXQueryBusMasterInterface(IN PDEVICE_OBJECT AtaXChannelFdo)
{
  PFDO_CHANNEL_EXTENSION  AtaXChannelFdoExtension;
  KEVENT                  Event;
  NTSTATUS                Status;
  PIRP                    Irp;
  IO_STATUS_BLOCK         IoStatus;
  PIO_STACK_LOCATION      Stack;


  DPRINT("AtaXQueryBusMasterInterface: ... \n");

  AtaXChannelFdoExtension = (PFDO_CHANNEL_EXTENSION)AtaXChannelFdo->DeviceExtension;

  KeInitializeEvent(&Event, NotificationEvent, FALSE);

  Irp = IoBuildSynchronousFsdRequest(
               IRP_MJ_PNP,
               AtaXChannelFdoExtension->CommonExtension.LowerDevice,
               NULL,
               0,
               NULL,
               &Event,
               &IoStatus);

  if ( Irp == NULL )
    return STATUS_INSUFFICIENT_RESOURCES;

  Stack = IoGetNextIrpStackLocation(Irp);

  Stack->MajorFunction = IRP_MJ_PNP;
  Stack->MinorFunction = IRP_MN_QUERY_INTERFACE;

  Stack->Parameters.QueryInterface.Size = 5; // тип интерфейса = QueryBusMasterInterface
  Stack->Parameters.QueryInterface.InterfaceType = (LPGUID)&GUID_BUS_INTERFACE_STANDARD;
  Stack->Parameters.QueryInterface.Version = 1; //не используется
  Stack->Parameters.QueryInterface.Interface = (PINTERFACE)&AtaXChannelFdoExtension->BusMasterInterface;
  Stack->Parameters.QueryInterface.InterfaceSpecificData = NULL; //не используется

  Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;

  Status = IoCallDriver(AtaXChannelFdoExtension->CommonExtension.LowerDevice, Irp);  // call pciide

  if ( Status == STATUS_PENDING )
  {
    KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
    Status = IoStatus.Status;
  }

  return Status;
}

NTSTATUS 
AtaXParseTranslatedResources(
    IN PFDO_CHANNEL_EXTENSION AtaXChannelFdoExtension,
    IN PCM_RESOURCE_LIST ResourcesTranslated)
{
  ULONG     jx;
  ULONG     AddressLength = 0;
  NTSTATUS  Status = STATUS_SUCCESS;

  DPRINT("AtaXParseTranslatedResources: ... \n");

  if ( ResourcesTranslated->Count == 0 )
  {
    DPRINT1("ERROR: ResourcesTranslated->Count = 0\n");
    ASSERT(FALSE);
    return STATUS_INVALID_PARAMETER;
  }

  if ( ResourcesTranslated->Count > 1 )
  {
    DPRINT1("ERROR: ResourcesTranslated->Count > 1\n");
    ASSERT(FALSE);
    return STATUS_INVALID_PARAMETER;
  }

  // WDM драйвер использует только первый CM_FULL_RESOURCE_DESCRIPTOR
  for ( jx = 0; jx < ResourcesTranslated->List[0].PartialResourceList.Count; ++jx )  // CM_PARTIAL_RESOURCE_DESCRIPTOR jx
  {
    PCM_PARTIAL_RESOURCE_DESCRIPTOR Descriptor;
    Descriptor = ResourcesTranslated->List[0].PartialResourceList.PartialDescriptors + jx;
    ASSERT(Descriptor->Type != CmResourceTypeDeviceSpecific);  // ResType_ClassSpecific (0xFFFF)

    switch ( Descriptor->Type )
    {
      case CmResourceTypePort:       /* 1 */
      case CmResourceTypeMemory:     /* 3 */
      {
        DPRINT("Descriptor->Type / CmResourceTypePort or CmResourceTypeMemory\n");
        if ( jx == 0 && Descriptor->u.Port.Length == 8 )
        {
           if ( Descriptor->u.Port.Start.HighPart == 0 )
           {
              AtaXChannelFdoExtension->BaseIoAddress1.Data        = (PUSHORT)(Descriptor->u.Port.Start.LowPart + 0);
              AtaXChannelFdoExtension->BaseIoAddress1.Error       = (PUCHAR) (Descriptor->u.Port.Start.LowPart + 1);
              AtaXChannelFdoExtension->BaseIoAddress1.SectorCount = (PUCHAR) (Descriptor->u.Port.Start.LowPart + 2);
              AtaXChannelFdoExtension->BaseIoAddress1.LowLBA      = (PUCHAR) (Descriptor->u.Port.Start.LowPart + 3);
              AtaXChannelFdoExtension->BaseIoAddress1.MidLBA      = (PUCHAR) (Descriptor->u.Port.Start.LowPart + 4);
              AtaXChannelFdoExtension->BaseIoAddress1.HighLBA     = (PUCHAR) (Descriptor->u.Port.Start.LowPart + 5);
              AtaXChannelFdoExtension->BaseIoAddress1.DriveSelect = (PUCHAR) (Descriptor->u.Port.Start.LowPart + 6);
              AtaXChannelFdoExtension->BaseIoAddress1.Command     = (PUCHAR) (Descriptor->u.Port.Start.LowPart + 7);
           }
        }

        if ( Descriptor->u.Port.Length == 1 )
        {
           if ( Descriptor->u.Port.Start.HighPart == 0 )
           {
              if ( AddressLength == 1 )
                continue; //next PartialDescriptor

              AtaXChannelFdoExtension->BaseIoAddress2.DeviceControl = (PUCHAR)(Descriptor->u.Port.Start.LowPart + 0);
              //AtaXChannelFdoExtension->BaseIoAddress2.DriveAddress  = (PUCHAR)(Descriptor->u.Port.Start.LowPart + 1);

              AddressLength = 1;
          }
        }

        DPRINT("AtaXParseTranslatedResources:  Start.LowPart - %p \n", Descriptor->u.Port.Start.LowPart );
        DPRINT("AtaXParseTranslatedResources:  Length        - %x \n", Descriptor->u.Port.Length );

        break;
      }

      case CmResourceTypeInterrupt:  /* 2 */
      {
        AtaXChannelFdoExtension->InterruptShareDisposition = Descriptor->ShareDisposition;

        if ( 0)//AtaXChannelFdoExtension->SataInterface.Size )
        {
ASSERT(FALSE);
        }
        else
        {
          AtaXChannelFdoExtension->InterruptShareDisposition = Descriptor->ShareDisposition;
          AtaXChannelFdoExtension->InterruptFlags            = Descriptor->Flags;
          AtaXChannelFdoExtension->InterruptLevel            = Descriptor->u.Interrupt.Level;
          AtaXChannelFdoExtension->InterruptVector           = Descriptor->u.Interrupt.Vector;
          AtaXChannelFdoExtension->InterruptAffinity         = Descriptor->u.Interrupt.Affinity;
        }

        DPRINT("Descriptor->Type / CmResourceTypeInterrupt\n");
        DPRINT("AtaXParseTranslatedResources: Descriptor->ShareDisposition   - %x \n", Descriptor->ShareDisposition );
        DPRINT("AtaXParseTranslatedResources: Descriptor->Flags              - %x \n", Descriptor->Flags );
        DPRINT("AtaXParseTranslatedResources: Descriptor->u.Interrupt.Level  - %x \n", Descriptor->u.Interrupt.Level );
        DPRINT("AtaXParseTranslatedResources: Descriptor->u.Interrupt.Vector - %x \n", Descriptor->u.Interrupt.Vector );

        break;
      }

      default:
      {
        DPRINT1("Descriptor->Type / Unknownn - %x\n", Descriptor->Type);
        ASSERT(FALSE);
      }
    }
  }

  return Status;
}

BOOLEAN 
InterruptRoutine(IN  PFDO_CHANNEL_EXTENSION  AtaXChannelFdoExtension)
{
  PSCSI_REQUEST_BLOCK    Srb;
  PATAX_REGISTERS_1      AtaXRegisters1;
  PATAX_REGISTERS_2      AtaXRegisters2;
  UCHAR                  StatusByte;
  UCHAR                  InterruptReason;
  BOOLEAN                Result      = FALSE;
  BOOLEAN                AtapiDevice = FALSE;
  BOOLEAN                BusMaster   = FALSE;
  ULONG                  WordCount = 0;
  ULONG                  WordsThisInterrupt = 256;
  ULONG                  Status;
  ULONG                  BusMasterStatus;
  ULONG                  ix;
  PBUS_MASTER_INTERFACE  BusMasterInterface;

  DPRINT(" InterruptRoutine:  AtaXChannelFdoExtension - %p \n", AtaXChannelFdoExtension );

  BusMasterInterface = &AtaXChannelFdoExtension->BusMasterInterface;
  AtaXRegisters1     = &AtaXChannelFdoExtension->BaseIoAddress1;
  AtaXRegisters2     = &AtaXChannelFdoExtension->BaseIoAddress2;
  Srb                = AtaXChannelFdoExtension->CurrentSrb;

  if ( AtaXChannelFdoExtension->BusMaster )
  {
    BusMasterStatus = BusMasterInterface->BusMasterReadStatus(
                                BusMasterInterface->ChannelPdoExtension);

    DPRINT(" InterruptRoutine:  BusMasterReadStatus - %x \n",  BusMasterStatus);
    // Interrupt bit. When this bit is read as a one,
    // all data transfered from the drive is visible in system memory
    if ( BusMasterStatus & 4 )
    {
      BusMasterInterface->BusMasterStop(BusMasterInterface->ChannelPdoExtension);
      Result = TRUE;
    }
  }

  if ( !Srb )
  {
    DPRINT("InterruptRoutine: CurrentSrb is NULL\n");
    // считываем регистр состояния и сбрасываем прерывание
    StatusByte = READ_PORT_UCHAR(AtaXRegisters1->Status);
    return Result;
  }

  if ( AtaXChannelFdoExtension->ExpectingInterrupt == FALSE )
  {
    DPRINT("InterruptRoutine: Unexpected interrupt\n");
    return Result;
  }

  if ( !(AtaXChannelFdoExtension->DeviceFlags[Srb->TargetId] & DFLAGS_USE_DMA) )
  {
    BusMasterStatus = BusMasterInterface->BusMasterReadStatus(
                               BusMasterInterface->ChannelPdoExtension);

    DPRINT(" InterruptRoutine:  BusMasterReadStatus - %x \n",  BusMasterStatus);
    // Interrupt bit. When this bit is read as a one,
    // all data transfered from the drive is visible in system memory
    if ( BusMasterStatus & 4 )
    {
      BusMasterInterface->BusMasterStop(BusMasterInterface->ChannelPdoExtension);
    }
  }

  if ( AtaXChannelFdoExtension->BusMaster )
  {
    BusMaster = TRUE;
    AtaXChannelFdoExtension->BusMaster = FALSE; //reset BusMaster
  }

  // считываем регистр состояния и сбрасываем прерывание
  StatusByte = READ_PORT_UCHAR(AtaXRegisters1->Status);
  DPRINT1("InterruptRoutine: Entered with Status - %x\n", StatusByte);

  if ( StatusByte & IDE_STATUS_BUSY )
  {
    DPRINT("InterruptRoutine: StatusByte & IDE_STATUS_BUSY \n");

    for ( ix = 0; ix < 10; ix++ )
    {
      // считываем регистр состояния и сбрасываем прерывание
      StatusByte = READ_PORT_UCHAR(AtaXRegisters1->Status);
      if ( !(StatusByte & IDE_STATUS_BUSY) )
        break;
      KeStallExecutionProcessor(5000);
    }

    if ( ix == 10 )
    {
      DPRINT("InterruptRoutine: StatusByte & IDE_STATUS_BUSY. Status %x, Base IO %x\n", StatusByte, AtaXRegisters1);
      return TRUE;
    }
  }

  if ( StatusByte & IDE_STATUS_ERROR ) // если есть ошибка
  {
    DPRINT1("InterruptRoutine: StatusByte & IDE_STATUS_ERROR\n");
    if ( Srb->Cdb[0] != SCSIOP_REQUEST_SENSE )
    {
      // этот запрос сбойный
      Status = SRB_STATUS_ERROR;
      goto CompleteRequest;
    }
  }

  // причина прерывания
  if ( AtaXChannelFdoExtension->DeviceFlags[Srb->TargetId] & DFLAGS_ATAPI_DEVICE )
  {
    // ATAPI устройство
    InterruptReason = (READ_PORT_UCHAR(AtaXRegisters1->InterruptReason) & 3);
    AtapiDevice = TRUE;
    WordsThisInterrupt = 256;
  }
  else
  {
    // ATA устройство
    if ( BusMaster )
    {
       goto CompleteRequest;
    }

    if ( StatusByte & IDE_STATUS_DRQ )
    {
      if ( AtaXChannelFdoExtension->MaximumBlockXfer[Srb->TargetId] )
        WordsThisInterrupt = 256 * AtaXChannelFdoExtension->MaximumBlockXfer[Srb->TargetId];

      if ( Srb->SrbFlags & SRB_FLAGS_DATA_IN )
      {
        InterruptReason =  2;
      }
      else if ( Srb->SrbFlags & SRB_FLAGS_DATA_OUT )
      {
        InterruptReason = 0;
      }
      else
      {
        Status = SRB_STATUS_ERROR;
        goto CompleteRequest;
      }
    }
    else if ( StatusByte & IDE_STATUS_BUSY )
    {
      return FALSE;
    }
    else
    {
      if ( AtaXChannelFdoExtension->WordsLeft )
      {
        InterruptReason = (Srb->SrbFlags & SRB_FLAGS_DATA_IN) ?  2 : 0;
      }
      else
      {
        // команда выполнена - verify, write, or the SMART enable/disable, also get_media_status
        InterruptReason = 3;
      }
    }
  }

  DPRINT1("InterruptRoutine: InterruptReason - %x\n", InterruptReason);

  if ( InterruptReason == 1 && (StatusByte & IDE_STATUS_DRQ) )
  {
    // запись Atapi пакета
    DPRINT("InterruptRoutine: Writing Atapi packet.\n");

    // отправляем CDB устройству
    WRITE_PORT_BUFFER_USHORT(AtaXRegisters1->Data, (PUSHORT)Srb->Cdb, 6);
    return TRUE;
  }
  else if ( InterruptReason == 0 && (StatusByte & IDE_STATUS_DRQ) )
  {
    // запись данных
ASSERT(FALSE);
    return TRUE;
  }
  else if ( InterruptReason == 2 && (StatusByte & IDE_STATUS_DRQ) )
  {
    // чтение данных
ASSERT(FALSE);
    return TRUE;
  }
  else if ( InterruptReason == 3  && !(StatusByte & IDE_STATUS_DRQ) )
  {
    // команда завершена
    if ( !BusMaster )
    {
      if ( AtaXChannelFdoExtension->WordsLeft )
        Status = SRB_STATUS_DATA_OVERRUN;
      else
        Status = SRB_STATUS_SUCCESS;
    }

CompleteRequest:
    //DPRINT("InterruptRoutine: CompleteRequest\n");

    if ( BusMaster )
    {
ASSERT(FALSE);
       DPRINT("InterruptRoutine: Status - %x\n", Status);
    }

    if ( Status == SRB_STATUS_ERROR )  // ошибка
    {
      // map error to specific SRB Status and handle request sense
      DPRINT("InterruptRoutine: Status == SRB_STATUS_ERROR\n");
      Status = AtaXMapError(AtaXChannelFdoExtension, Srb);      //Status = 6;
      AtaXChannelFdoExtension->RDP = FALSE;
    }
    else
    {
      // подождем если занято
      for ( ix = 0; ix < 30; ix++ )
      {
        StatusByte = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus);  // Считываем альтернативный регистр состояния
        DPRINT("AtaXIssueIdentify: Status (AlternateStatus) before read words - %x\n", StatusByte);

        if ( !(StatusByte & IDE_STATUS_BUSY) )
          break;
        KeStallExecutionProcessor(500);
      }

      if ( ix == 30 )
      {
        // занято. сброс контроллера
         DPRINT("InterruptRoutine: Resetting due to BSY still up - %x. Base Io %x\n", StatusByte, AtaXRegisters1);
         DPRINT("InterruptRoutine: FIXME AtapiResetController()\n");
         //AtapiResetController(HwDeviceExtension,Srb->PathId);
         return TRUE;
      }

      // подождем если DRQ
      if ( StatusByte & IDE_STATUS_DRQ )
      {
        for ( ix = 0; ix < 500; ix++ )
        {
          StatusByte = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus);  // Считываем альтернативный регистр состояния
          DPRINT("AtaXIssueIdentify: Status (AlternateStatus) before read words - %x\n", StatusByte);

          if ( !(StatusByte & IDE_STATUS_DRQ) )
            break;
          KeStallExecutionProcessor(100);
        }

        if ( ix == 500 )
        {
          // сброс контроллера
          DPRINT("InterruptRoutine: Resetting due to DRQ still up - %x\n", StatusByte);
          DPRINT("InterruptRoutine: FIXME AtapiResetController()\n");
          //AtapiResetController(HwDeviceExtension,Srb->PathId);
          return TRUE;
        }
      }
    }

    AtaXChannelFdoExtension->ExpectingInterrupt = FALSE;

    if ( Srb != NULL )
    {
      Srb->SrbStatus = (UCHAR)Status;
      if ( AtaXChannelFdoExtension->WordsLeft )
      {
        if ( !(AtaXChannelFdoExtension->DeviceFlags[Srb->TargetId] & DFLAGS_TAPE_DEVICE) )
        {
          if ( Status == SRB_STATUS_DATA_OVERRUN )
            Srb->DataTransferLength -= AtaXChannelFdoExtension->WordsLeft;
          else
            Srb->DataTransferLength = 0;
        }
        else
        {
          Srb->DataTransferLength -= AtaXChannelFdoExtension->WordsLeft;
        }
      }

      if ( Srb->Function != SRB_FUNCTION_IO_CONTROL )
      {
        // команда завершена?
        if ( !(AtaXChannelFdoExtension->RDP) )
          AtaXNotification(RequestComplete, AtaXChannelFdoExtension, Srb);
      }
      else
      {
        PSENDCMDOUTPARAMS  cmdOutParameters;
        UCHAR  error = 0;

        cmdOutParameters = (PSENDCMDOUTPARAMS)(((PUCHAR)Srb->DataBuffer) + sizeof(SRB_IO_CONTROL));
        if ( Status != SRB_STATUS_SUCCESS )
          error = READ_PORT_UCHAR(AtaXRegisters1->Error);

        // построение SMART Status block
        cmdOutParameters->cBufferSize = WordCount;
        cmdOutParameters->DriverStatus.bDriverError = (error) ? SMART_IDE_ERROR : 0;
        cmdOutParameters->DriverStatus.bIDEError = error;

        if ( AtaXChannelFdoExtension->SmartCommand == RETURN_SMART_STATUS )
        {
          cmdOutParameters->bBuffer[0] = RETURN_SMART_STATUS;
          cmdOutParameters->bBuffer[1] = READ_PORT_UCHAR(AtaXRegisters1->InterruptReason);
          cmdOutParameters->bBuffer[2] = READ_PORT_UCHAR(AtaXRegisters1->LowLBA);//Unused1
          cmdOutParameters->bBuffer[3] = READ_PORT_UCHAR(AtaXRegisters1->ByteCountLow);
          cmdOutParameters->bBuffer[4] = READ_PORT_UCHAR(AtaXRegisters1->ByteCountHigh);
          cmdOutParameters->bBuffer[5] = READ_PORT_UCHAR(AtaXRegisters1->DriveSelect);
          cmdOutParameters->bBuffer[6] = SMART_CMD;
          cmdOutParameters->cBufferSize = 8;
        }

        AtaXNotification(RequestComplete, AtaXChannelFdoExtension, Srb);  // команда завершена
      }
    }
    else
    {
      DPRINT("InterruptRoutine: No SRB!\n");
    }

    if ( !(AtaXChannelFdoExtension->RDP) )  // готовность к следующему запросу
    {
      AtaXChannelFdoExtension->CurrentSrb = NULL;
      AtaXNotification(NextRequest, AtaXChannelFdoExtension, NULL);
    }
    else
    {
      DPRINT("InterruptRoutine: AtaXNotification FIXME\n");
      //AtaXNotification(RequestTimerCall, AtaXChannelFdoExtension, AtapiCallBack, 2000);
    }

    DPRINT("InterruptRoutine: return TRUE\n");
    return TRUE;
  }
  else
  {
    // неожидаемое прерывание
    DPRINT("InterruptRoutine: Unexpected interrupt. InterruptReason %x. Status %x.\n", InterruptReason, StatusByte);
    return FALSE;
  }

  DPRINT("InterruptRoutine: return TRUE\n");
  return TRUE;
}

BOOLEAN 
AtaXChannelInterrupt(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext)
{
  PFDO_CHANNEL_EXTENSION  AtaXChannelFdoExtension;
  BOOLEAN Result;

  DPRINT(" AtaXChannelInterrupt:  Interrupt - %p, ServiceContext - %p \n", Interrupt, ServiceContext );

  AtaXChannelFdoExtension = (PFDO_CHANNEL_EXTENSION)((PDEVICE_OBJECT)ServiceContext)->DeviceExtension;

  Result = InterruptRoutine(AtaXChannelFdoExtension);

  DPRINT("AtaXChannelInterrupt: InterruptData.Flags - %x\n", AtaXChannelFdoExtension->InterruptData.Flags);
  if ( AtaXChannelFdoExtension->InterruptData.Flags & ATAX_NOTIFICATION_NEEDED )  // если ATAX_NOTIFICATION_NEEDED, то запрашиваем DPC
  {
    DPRINT1("AtaXChannelInterrupt: KeInsertQueueDpc \n");
    KeInsertQueueDpc(&AtaXChannelFdoExtension->Dpc, NULL, NULL);
  }

  return Result;
}

VOID NTAPI
AtaXDpc(
    IN PKDPC Dpc,
    IN PDEVICE_OBJECT AtaXChannelFdo,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2)
{
ASSERT(FALSE);
}

NTSTATUS
AtaXCreateSymLinks(IN PFDO_CHANNEL_EXTENSION AtaXChannelFdoExtension)
{
  NTSTATUS        Status = STATUS_SUCCESS;
  WCHAR           DeviceString[80];
  WCHAR           SourceString[80];
  UNICODE_STRING  DeviceName;
  UNICODE_STRING  SymbolicLinkName;
  ULONG           ScsiPortCount;
  ULONG           ix;

  swprintf(DeviceString, L"\\Device\\Ide\\IdePort%d", AtaXChannelFdoExtension->Channel);
  RtlInitUnicodeString(&DeviceName, DeviceString);

  ScsiPortCount = IoGetConfigurationInformation()->ScsiPortCount;
  DPRINT("AtaXCreateSymLinks: ScsiPortCount - %x \n", ScsiPortCount);

  for ( ix = 0; ix <= ScsiPortCount; ix++ )
  {
    swprintf(SourceString, L"\\Device\\ScsiPort%d", ix);
    RtlInitUnicodeString(&SymbolicLinkName, SourceString);
    Status = IoCreateSymbolicLink(&SymbolicLinkName, &DeviceName);

    if ( NT_SUCCESS(Status) )
    {
       DPRINT(" AtaXCreateSymLinks: %wZ\n", &SymbolicLinkName);

       swprintf(SourceString, L"\\DosDevices\\Scsi%d:", ix);
       RtlInitUnicodeString(&SymbolicLinkName, SourceString);
       IoCreateSymbolicLink(&SymbolicLinkName, &DeviceName);

       DPRINT("AtaXCreateSymLinks: %wZ\n", &SymbolicLinkName);
       break;
    }
  }

  if ( NT_SUCCESS(Status) )
    ++IoGetConfigurationInformation()->ScsiPortCount;

  return Status;
}

NTSTATUS
AtaXChannelFdoQueryDeviceRelations(
    IN PDEVICE_OBJECT AtaXChannelFdo,
    IN PIRP Irp)
{
  NTSTATUS                Status = STATUS_SUCCESS;
  PFDO_CHANNEL_EXTENSION  AtaXChannelFdoExtension;
  PPDO_DEVICE_EXTENSION   AtaXDevicePdoExtension;
  PDEVICE_OBJECT          AtaXDevicePdo;
  UNICODE_STRING          DeviceName;
  WCHAR                   DeviceString[80];
  ULONG                   Count = 0;
  ULONG                   TargetId;
  ULONG                   ix;


  DPRINT("AtaXChannelFdoQueryDeviceRelations (%p %p)\n", AtaXChannelFdo, Irp);

  AtaXChannelFdoExtension = (PFDO_CHANNEL_EXTENSION)AtaXChannelFdo->DeviceExtension;

  ASSERT(AtaXChannelFdoExtension);
  ASSERT(AtaXChannelFdoExtension->CommonExtension.IsFDO);

  ix = MAX_IDE_DEVICE;

  // Перечисляем все IDE устройства на канале (Master и Slave)
  for ( TargetId = 0; TargetId < ix; TargetId++ )
  {
    DPRINT("AtaXChannelFdoQueryDeviceRelations: TargetId - %x\n", TargetId);

    // Если PDO уже есть, то пропускаем
    if ( AtaXChannelFdoExtension->AtaXDevicePdo[TargetId] )
    {
      Count++;
      continue;
    }

    // Включаем канал 
    AtaXChannelFdoExtension->ChannelState = ChannelEnabled;

    // Создаем новый PDO (PathId - номер канала (Primari-0 или Secondary-1), TargetId - номер девайса (Master-0 или Slave-1), Lun - 0 (не использ.))
    swprintf(DeviceString, L"\\Device\\Ide\\IdeDeviceP%dT%dL%d-%x",
             AtaXChannelFdoExtension->Channel,
             TargetId,
             0,
             InterlockedExchangeAdd((PLONG)&AtaXDeviceCounter, 1));

    RtlInitUnicodeString(&DeviceName, DeviceString);
    DPRINT("AtaXChannelFdoQueryDeviceRelations: Create AtaXDevicePdo -'%wZ' \n", &DeviceName);

    Status = IoCreateDevice(AtaXChannelFdo->DriverObject,
                            sizeof(PDO_DEVICE_EXTENSION),
                            &DeviceName,
                            FILE_DEVICE_MASS_STORAGE,
                            FILE_DEVICE_SECURE_OPEN,
                            FALSE,
                            &AtaXDevicePdo);

    if ( !NT_SUCCESS(Status) )
    {
       // FIXME: handle error
       DPRINT1("AtaXChannelFdoQueryDeviceRelations: Not created AtaXDevicePdo -'%wZ', Status - %p\n", &DeviceName, Status);
       continue;
    }

    DPRINT("AtaXChannelFdoQueryDeviceRelations: sizeof(PDO_DEVICE_EXTENSION) - %x\n", sizeof(PDO_DEVICE_EXTENSION));

    // Новый PDO создан. Заполняем поля объекта и его расширения
    AtaXDevicePdo->AlignmentRequirement = AtaXChannelFdo->AlignmentRequirement;
    if ( AtaXDevicePdo->AlignmentRequirement < 1 )
      AtaXDevicePdo->AlignmentRequirement = 1;
    
    AtaXDevicePdoExtension = AtaXDevicePdo->DeviceExtension;
    RtlZeroMemory(AtaXDevicePdoExtension, sizeof(PDO_DEVICE_EXTENSION));

    AtaXDevicePdoExtension->CommonExtension.IsFDO      = FALSE;
    AtaXDevicePdoExtension->CommonExtension.SelfDriver = AtaXChannelFdo->DriverObject;
    AtaXDevicePdoExtension->CommonExtension.SelfDevice = AtaXDevicePdo;

    AtaXDevicePdoExtension->AtaXChannelFdoExtension    = AtaXChannelFdoExtension;

    AtaXDevicePdoExtension->PathId   = (UCHAR)AtaXChannelFdoExtension->Channel;
    AtaXDevicePdoExtension->TargetId = (UCHAR)TargetId;
    AtaXDevicePdoExtension->Lun      = 0;

    AtaXDevicePdoExtension->MaxQueueCount = 256;
    KeInitializeDeviceQueue(&AtaXDevicePdoExtension->DeviceQueue);  // инициализируем очередь IRPs

    AtaXChannelFdoExtension->AtaXDevicePdo[TargetId] = AtaXDevicePdo;
    Count++;

    AtaXDevicePdo->Flags |= DO_DIRECT_IO;
    AtaXDevicePdo->Flags &= ~DO_DEVICE_INITIALIZING;

    DPRINT("AtaXChannelFdoQueryDeviceRelations: AtaXDevicePdo - %p, AtaXDevicePdoExtension - %p\n", AtaXDevicePdo, AtaXDevicePdoExtension);
  }

  // Идентификация ATA/ATAPI устройств, подключенных к текущему каналу
  Status = AtaXDetectDevices(AtaXChannelFdoExtension);
  DPRINT("AtaXChannelFdoQueryDeviceRelations: AtaXDetectDevices return - %x\n", Status);

ASSERT(FALSE);

  return STATUS_SUCCESS;
}

NTSTATUS
AtaXChannelFdoStartDevice(
    IN PDEVICE_OBJECT AtaXChannelFdo,
    IN PIRP Irp)
{
  PFDO_CHANNEL_EXTENSION  AtaXChannelFdoExtension;
  PCM_RESOURCE_LIST       ResourcesTranslated;
  PDEVICE_OBJECT          LowerDevice;
  KEVENT                  Event;
  NTSTATUS                Status;

  DPRINT("AtaXChannelFdoStartDevice: AtaXChannelFdo - %p, Irp - %p)\n", AtaXChannelFdo, Irp);

  AtaXChannelFdoExtension = (PFDO_CHANNEL_EXTENSION)AtaXChannelFdo->DeviceExtension;

  ASSERT(AtaXChannelFdoExtension);
  ASSERT(AtaXChannelFdoExtension->CommonExtension.IsFDO);

  LowerDevice = AtaXChannelFdoExtension->CommonExtension.LowerDevice;
  ASSERT(LowerDevice);

  // Для функции StartDevice в IRP передается указатель на структуру PCM_RESOURCE_LIST с ресурсами
  // (могут быть RAW и Translated. см. DDK)
  ResourcesTranslated = IoGetCurrentIrpStackLocation(Irp)->
                        Parameters.StartDevice.AllocatedResourcesTranslated;

  if ( AtaXChannelFdoExtension->Channel < MAX_IDE_CHANNEL )
  {
    DPRINT("AtaXChannelFdoStartDevice: ResourcesTranslated - %p\n", ResourcesTranslated);
    if ( ResourcesTranslated )
    {
      if ( ResourcesTranslated->Count == 0 )
        return STATUS_INVALID_PARAMETER;
    }
    else
    {
      return STATUS_INVALID_PARAMETER;
    }
  }

  // Стартуем нижний объект (pciide и ему подобный)
  KeInitializeEvent(&Event, SynchronizationEvent, FALSE);
  IoCopyCurrentIrpStackLocationToNext(Irp);
  Irp->IoStatus.Status = STATUS_SUCCESS;
  IoSetCompletionRoutine(Irp, AtaXGenericCompletion, &Event, TRUE, TRUE, TRUE);

  DPRINT("Calling lower device %p [%wZ]\n", LowerDevice, &LowerDevice->DriverObject->DriverName);
  Status = IoCallDriver(AtaXChannelFdoExtension->CommonExtension.LowerDevice, Irp);

  if ( Status == STATUS_PENDING )
  {
    KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
    Status = Irp->IoStatus.Status;
  }
  DPRINT("AtaXChannelFdoStartDevice: IoCallDriver Status - %x\n", Status);

  Status = AtaXQueryBusInterface(AtaXChannelFdo);
  DPRINT("AtaXChannelFdoStartDevice: AtaXQueryBusInterface return Status - %p\n", Status);

  Status = AtaXParseTranslatedResources(AtaXChannelFdoExtension, ResourcesTranslated);

  if ( NT_SUCCESS(Status) && AtaXChannelFdoExtension->InterruptLevel )
  {
    Status = IoConnectInterrupt(
               &AtaXChannelFdoExtension->InterruptObject,                                    // OUT PKINTERRUPT       *InterruptObject,
               (PKSERVICE_ROUTINE)AtaXChannelInterrupt,                                      // IN PKSERVICE_ROUTINE  ServiceRoutine,
               AtaXChannelFdoExtension->CommonExtension.SelfDevice,                          // IN PVOID              ServiceContext,
               NULL,                                                                         // IN PKSPIN_LOCK        SpinLock         OPTIONAL,
               AtaXChannelFdoExtension->InterruptVector,                                     // IN ULONG              Vector,
               AtaXChannelFdoExtension->InterruptLevel,                                      // IN KIRQL              Irql,
               AtaXChannelFdoExtension->InterruptLevel,                                      // IN KIRQL              SynchronizeIrql,
               (KINTERRUPT_MODE)(AtaXChannelFdoExtension->InterruptFlags & 1),               // IN KINTERRUPT_MODE    InterruptMode,
               AtaXChannelFdoExtension->InterruptShareDisposition == CmResourceShareShared,  // IN BOOLEAN            ShareVector,
               AtaXChannelFdoExtension->InterruptAffinity,                                   // IN KAFFINITY          ProcessorEnableMask,
               FALSE);                                                                       // IN BOOLEAN            FloatingSave

    DPRINT("IoConnectInterrupt: Status           - %x \n", Status);
    DPRINT("IoConnectInterrupt: *InterruptObject - %p \n", *(PULONG)AtaXChannelFdoExtension->InterruptObject);
    DPRINT("IoConnectInterrupt:  InterruptObject - %p \n", AtaXChannelFdoExtension->InterruptObject);

    DPRINT("Vector              - %x \n", AtaXChannelFdoExtension->InterruptVector);
    DPRINT("Irql                - %x \n", AtaXChannelFdoExtension->InterruptLevel);
    DPRINT("InterruptMode       - %x \n", (KINTERRUPT_MODE)(AtaXChannelFdoExtension->InterruptFlags & 1) );
    DPRINT("ShareVector         - %x \n", AtaXChannelFdoExtension->InterruptShareDisposition == CmResourceShareShared );
    DPRINT("ProcessorEnableMask - %x \n", AtaXChannelFdoExtension->InterruptAffinity);
    
    if ( NT_SUCCESS(Status) )
    {
       // Разрешаем прерывания
       WRITE_PORT_UCHAR(AtaXChannelFdoExtension->BaseIoAddress2.DeviceControl, 0); //Bit 1 (nIEN): 0 - Enable Interrupt, 1 - Disable Interrupt

       KeInitializeSpinLock(&AtaXChannelFdoExtension->SpinLock);

       KeInitializeDpc(&AtaXChannelFdoExtension->Dpc,
                       (PKDEFERRED_ROUTINE)AtaXDpc,
                       AtaXChannelFdo);

       AtaXChannelFdoExtension->Flags |= ATAX_DISCONNECT_ALLOWED;  // set flag that it's allowed to disconnect during this command

       if ( AtaXChannelFdoExtension->ChannelState == ChannelDisabled )
       {
         Status = AtaXQueryControllerProperties(AtaXChannelFdo);  // запрос конфигурационной информации IDE контроллера
         DPRINT("AtaXChannelFdoStartDevice: AtaXQueryControllerProperties return Status - %x\n", Status);

         Status = AtaXQueryBusMasterInterface(AtaXChannelFdo);
         DPRINT("AtaXChannelFdoStartDevice: AtaXQueryBusMasterInterface return Status - %x\n", Status);

         AtaXChannelFdoExtension->ChannelState = ChannelEnabled;
       }

       // Создаем символические ссылки
       Status = AtaXCreateSymLinks(AtaXChannelFdoExtension);
    }
  }
else
  {
ASSERT(FALSE);
  }

  Irp->IoStatus.Information = 0;
  Irp->IoStatus.Status = Status;

  DPRINT("AtaXChannelFdoStartDevice return - %x \n", Status);
  return Status;
}

NTSTATUS
AtaXChannelFdoDispatchPnp(
    IN PDEVICE_OBJECT AtaXChannelFdo,
    IN PIRP Irp)
{
  PIO_STACK_LOCATION  Stack = IoGetCurrentIrpStackLocation(Irp);
  ULONG               MinorFunction = Stack->MinorFunction;
  NTSTATUS            Status;

  switch ( MinorFunction )
  {
    case IRP_MN_START_DEVICE:                 /* 0x00 */  //AtaXChannelFdoStartDevice
    {
      DPRINT("IRP_MJ_PNP / IRP_MN_START_DEVICE\n");
      Status = AtaXChannelFdoStartDevice(AtaXChannelFdo, Irp);
      break;
    }

    case IRP_MN_QUERY_REMOVE_DEVICE:          /* 0x01 */
    case IRP_MN_CANCEL_REMOVE_DEVICE:         /* 0x03 */
    case IRP_MN_QUERY_STOP_DEVICE:            /* 0x05 */
    case IRP_MN_CANCEL_STOP_DEVICE:           /* 0x06 */  //AtaSuccessAndPassDownIrpAndForget
    {
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY(CANCEL)_REMOVE(STOP)_DEVICE\n");
      Irp->IoStatus.Status = STATUS_SUCCESS;
      return AtaXPassDownIrpAndForget(AtaXChannelFdo, Irp);
    }

    case IRP_MN_REMOVE_DEVICE:                /* 0x02 */  //AtaXChannelFdoRemoveDevice
    {
      DPRINT("IRP_MJ_PNP / IRP_MN_REMOVE_DEVICE\n");
ASSERT(FALSE);
      Status = STATUS_SUCCESS;//AtaXChannelFdoRemoveDevice(AtaXChannelFdo, Irp);
      break;
    }

    case IRP_MN_STOP_DEVICE:                  /* 0x04 */  //AtaXChannelFdoStopDevice
    {
      DPRINT("IRP_MJ_PNP / IRP_MN_STOP_DEVICE\n");
ASSERT(FALSE);
      Status = STATUS_SUCCESS;//AtaXChannelFdoStopDevice(AtaXChannelFdo, Irp);
      break;
    }

    case IRP_MN_QUERY_DEVICE_RELATIONS:       /* 0x07 */  //AtaXChannelFdoQueryDeviceRelations
    {
      switch (Stack->Parameters.QueryDeviceRelations.Type)
      {
        case BusRelations:
          DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_DEVICE_RELATIONS / BusRelations\n");
          Status = AtaXChannelFdoQueryDeviceRelations(AtaXChannelFdo, Irp);
          break;
        
        default:
          DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_DEVICE_RELATIONS / Unknown type 0x%lx\n", Stack->Parameters.QueryDeviceRelations.Type);
          return AtaXPassDownIrpAndForget(AtaXChannelFdo, Irp);
      }
      break;
    }

    case IRP_MN_FILTER_RESOURCE_REQUIREMENTS: /* 0x0d */  //AtaXChannelFdoFilterResourceRequirements
    {
      DPRINT("IRP_MJ_PNP / IRP_MN_FILTER_RESOURCE_REQUIREMENTS\n");
      return AtaXPassDownIrpAndForget(AtaXChannelFdo, Irp);
    }

    case IRP_MN_QUERY_PNP_DEVICE_STATE:       /* 0x14 */  //AtaXChannelFdoQueryPnPDeviceState
    {
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_PNP_DEVICE_STATE\n");
      Status = STATUS_SUCCESS;//AtaXChannelFdoQueryPnPDeviceState(AtaXChannelFdo, Irp);
      break;
    }

    case IRP_MN_DEVICE_USAGE_NOTIFICATION:    /* 0x16 */  //AtaXChannelFdoUsageNotification
    {
      DPRINT("IRP_MJ_PNP / IRP_MN_DEVICE_USAGE_NOTIFICATION\n");
ASSERT(FALSE);
      Status = STATUS_SUCCESS;//AtaXChannelFdoUsageNotification(AtaXChannelFdo, Irp);
      break;
    }

    case IRP_MN_SURPRISE_REMOVAL:             /* 0x17 */  //AtaXChannelFdoSurpriseRemoveDevice
    {
      DPRINT("IRP_MJ_PNP / IRP_MN_SURPRISE_REMOVAL\n");
ASSERT(FALSE);
      Status = STATUS_SUCCESS;//AtaXChannelFdoSurpriseRemoveDevice(AtaXChannelFdo, Irp);
      break;
    }

    default:
    {
      DPRINT1("IRP_MJ_PNP / Unknown minor function 0x%lx\n", MinorFunction);
      return AtaXPassDownIrpAndForget(AtaXChannelFdo, Irp);
    }
  }

  Irp->IoStatus.Status = Status;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return Status;
}

NTSTATUS NTAPI
AddChannelFdo(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT ChannelPdo)
{
  PFDO_CHANNEL_EXTENSION  AtaXChannelFdoExtension;
  PDEVICE_OBJECT          AtaXChannelFdo;
  PDEVICE_OBJECT          AttachedDevice;
  WCHAR                   DeviceString[80];
  UNICODE_STRING          DeviceName;
  NTSTATUS                Status;    

  DPRINT("AddChannelFdo ---------------------------------------------------------- \n"  );
  DPRINT("AddChannelFdo size FDO_CHANNEL_EXTENSION - %p \n", sizeof(FDO_CHANNEL_EXTENSION));

  swprintf(DeviceString, L"\\Device\\Ide\\IdePort%d", AtaXChannelCounter);
  RtlInitUnicodeString(&DeviceName, DeviceString);

  DPRINT("AddChannelFdo: Creating device '%wZ'\n", &DeviceName);

  // Создаем новый функциональный объект FDO для канала (Primary или Secondary)
  Status = IoCreateDevice(DriverObject, 
                          sizeof(FDO_CHANNEL_EXTENSION),
                          &DeviceName,
                          FILE_DEVICE_CONTROLLER,
                          FILE_DEVICE_SECURE_OPEN,
                          FALSE,
                          &AtaXChannelFdo);

  if ( !NT_SUCCESS(Status) )
  {
    DPRINT("AddChannelFdo: Not Created Device. Status - %p \n", Status);
    DPRINT("AddChannelFdo ---------------------------------------------------------- \n"  );
    return Status;
  }

  // Заполняем расширение FDO канала
  AtaXChannelFdoExtension = (PFDO_CHANNEL_EXTENSION)AtaXChannelFdo->DeviceExtension;
  RtlZeroMemory(AtaXChannelFdoExtension, sizeof(FDO_CHANNEL_EXTENSION));
  
  DPRINT("AddChannelFdo: AtaXChannelFdo - %p, AtaXChannelFdoExtension - %p, ChannelPdo - %p\n", AtaXChannelFdo, AtaXChannelFdoExtension, ChannelPdo);
 
  AtaXChannelFdoExtension->CommonExtension.IsFDO      = TRUE;
  AtaXChannelFdoExtension->CommonExtension.LowerPdo   = ChannelPdo;
  AtaXChannelFdoExtension->CommonExtension.SelfDriver = DriverObject;
  AtaXChannelFdoExtension->CommonExtension.SelfDevice = AtaXChannelFdo;

  // Присоединяем вновь созданный FDO к стеку объектов устройств
  AttachedDevice = IoAttachDeviceToDeviceStack(AtaXChannelFdo, ChannelPdo);

  if ( AttachedDevice )
  {
    // Объект к которому присоединились (Не обязательно это ChannelPdo. Может быть и фильтрующим DO)
    AtaXChannelFdoExtension->CommonExtension.LowerDevice = AttachedDevice;

    AtaXChannelFdo->AlignmentRequirement = AttachedDevice->AlignmentRequirement;
    if ( AtaXChannelFdo->AlignmentRequirement < 1 )
      AtaXChannelFdo->AlignmentRequirement = 1;

    // Если FDO в стеке, то увеличиваем счетчик каналов
    AtaXChannelFdoExtension->Channel = AtaXChannelCounter & 1;
    AtaXChannelCounter++;

    AtaXChannelFdo->Flags &= ~DO_DEVICE_INITIALIZING;
  }
  else
  {
    // Не судьба ...
    IoDeleteDevice(AtaXChannelFdo);
    Status = STATUS_UNSUCCESSFUL;
  }
  
  DPRINT("AddChannelFdo: Status - %x \n", Status);
  DPRINT("AddChannelFdo ---------------------------------------------------------- \n"  );
  return Status;
}
