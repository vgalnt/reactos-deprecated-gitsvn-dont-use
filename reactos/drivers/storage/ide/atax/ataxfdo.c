#include "atax.h"               

//#define NDEBUG
#include <debug.h>


ULONG  AtaXDeviceCounter = 0;

#ifdef NDEBUG
  BOOLEAN AtaXDEBUG = FALSE;
#else
  BOOLEAN AtaXDEBUG = TRUE;
#endif


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
AtaXQueryAhciInterface(IN PDEVICE_OBJECT AtaXChannelFdo)
{
  PFDO_CHANNEL_EXTENSION  AtaXChannelFdoExtension;
  KEVENT                  Event;
  NTSTATUS                Status;
  PIRP                    Irp;
  IO_STATUS_BLOCK         IoStatus;
  PIO_STACK_LOCATION      Stack;

  DPRINT("AtaXQueryAhciInterface: ... \n");

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
    return STATUS_INSUFFICIENT_RESOURCES;    // no memory

  Stack = IoGetNextIrpStackLocation(Irp);

  Stack->MajorFunction = IRP_MJ_PNP;
  Stack->MinorFunction = IRP_MN_QUERY_INTERFACE;

  Stack->Parameters.QueryInterface.Size = 7; // тип интерфейса = AtaXQueryAhciInterface
  Stack->Parameters.QueryInterface.InterfaceType = (LPGUID)&GUID_BUS_INTERFACE_STANDARD;
  Stack->Parameters.QueryInterface.Version = 1; //не используется
  Stack->Parameters.QueryInterface.Interface = (PINTERFACE)&AtaXChannelFdoExtension->AhciInterface;
  Stack->Parameters.QueryInterface.InterfaceSpecificData = NULL; //не используется

  Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;

  Status = IoCallDriver(AtaXChannelFdoExtension->CommonExtension.LowerDevice, Irp);  // call pciide, or ahcix, or ... 

  if ( Status == STATUS_PENDING )
  {
    KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
    Status = IoStatus.Status;
  }

  DPRINT("AtaXQueryAhciInterface: AtaXChannelFdoExtension->AhciControllerInterface - %p\n", AtaXChannelFdoExtension->AhciInterface);

  DPRINT("AtaXQueryAhciInterface: return Status - %x\n", Status);
  return Status;
}

NTSTATUS
AtaXQuerySataInterface(IN PDEVICE_OBJECT AtaXChannelFdo)
{
  PFDO_CHANNEL_EXTENSION  AtaXChannelFdoExtension;
  KEVENT                  Event;
  NTSTATUS                Status;
  PIRP                    Irp;
  IO_STATUS_BLOCK         IoStatus;
  PIO_STACK_LOCATION      Stack;

  DPRINT("AtaXQuerySataInterface: ... \n");

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
    return STATUS_INSUFFICIENT_RESOURCES; // no memory

  Stack = IoGetNextIrpStackLocation(Irp);

  Stack->MajorFunction = IRP_MJ_PNP;
  Stack->MinorFunction = IRP_MN_QUERY_INTERFACE;

  Stack->Parameters.QueryInterface.Size = 9; // тип интерфейса = AtaXQuerySataInterface
  Stack->Parameters.QueryInterface.InterfaceType = (LPGUID)&GUID_BUS_INTERFACE_STANDARD;
  Stack->Parameters.QueryInterface.Version = 1; //не используется
  Stack->Parameters.QueryInterface.Interface = (PINTERFACE)&AtaXChannelFdoExtension->SataInterface;
  Stack->Parameters.QueryInterface.InterfaceSpecificData = NULL; //не используется

  DPRINT("AtaXQuerySataInterface: AtaXChannelFdoExtension->SataInterface.SataBaseAddress - %p\n", AtaXChannelFdoExtension->SataInterface.SataBaseAddress);

  Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;

  Status = IoCallDriver(AtaXChannelFdoExtension->CommonExtension.LowerDevice, Irp);  // call pciide, or ahcix, or ... 

  if ( Status == STATUS_PENDING )
  {
    KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
    Status = IoStatus.Status;
  }

  DPRINT("AtaXQuerySataInterface: AtaXChannelFdoExtension->SataInterface.SataBaseAddress - %p\n", AtaXChannelFdoExtension->SataInterface.SataBaseAddress);

  if ( NT_SUCCESS(Status) )
  {
    PSATA_INTERFACE  SataInterface = &AtaXChannelFdoExtension->SataInterface;

    DPRINT("AtaXQuerySataInterface: SataInterface       - %p \n", SataInterface);

    DPRINT("AtaXQuerySataInterface: ChannelPdoExtension - %p \n", SataInterface->ChannelPdoExtension);
    DPRINT("AtaXQuerySataInterface: SataBaseAddress     - %p \n", SataInterface->SataBaseAddress);
    DPRINT("AtaXQuerySataInterface: InterruptResource   - %p \n", SataInterface->InterruptResource);

    DPRINT("AtaXQuerySataInterface: InterruptResource->InterruptShareDisposition - %x \n", SataInterface->InterruptResource->InterruptShareDisposition);
    DPRINT("AtaXQuerySataInterface: InterruptResource->InterruptFlags            - %x \n", SataInterface->InterruptResource->InterruptFlags);
    DPRINT("AtaXQuerySataInterface: InterruptResource->InterruptLevel            - %x \n", SataInterface->InterruptResource->InterruptLevel);
    DPRINT("AtaXQuerySataInterface: InterruptResource->InterruptVector           - %x \n", SataInterface->InterruptResource->InterruptVector);
    DPRINT("AtaXQuerySataInterface: InterruptResource->InterruptObject           - %p \n", SataInterface->InterruptResource->InterruptObject);
  }

  DPRINT("AtaXQuerySataInterface: return Status - %x\n", Status);
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

        if ( AtaXChannelFdoExtension->SataInterface.Size )
        {
          PSATA_INTERRUPT_RESOURCE  InterruptResource = AtaXChannelFdoExtension->SataInterface.InterruptResource;

          AtaXChannelFdoExtension->InterruptShareDisposition = InterruptResource->InterruptShareDisposition;
          AtaXChannelFdoExtension->InterruptFlags            = InterruptResource->InterruptFlags;
          AtaXChannelFdoExtension->InterruptLevel            = InterruptResource->InterruptLevel;
          AtaXChannelFdoExtension->InterruptVector           = InterruptResource->InterruptVector;
          AtaXChannelFdoExtension->InterruptAffinity         = InterruptResource->InterruptAffinity;
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

ULONG
Atapi2Scsi(
    IN PSCSI_REQUEST_BLOCK Srb,
    IN char *DataBuffer,
    IN ULONG ByteCount)
{
  ULONG bytesAdjust = 0;

  DPRINT("Atapi2Scsi: Srb->Cdb[0] == ATAPI_MODE_SENSE - %x\n", Srb->Cdb[0] == ATAPI_MODE_SENSE);

  if ( Srb->Cdb[0] == ATAPI_MODE_SENSE )
  {
    PMODE_PARAMETER_HEADER_10  header_10 = (PMODE_PARAMETER_HEADER_10)DataBuffer;
    PMODE_PARAMETER_HEADER     header    = (PMODE_PARAMETER_HEADER)DataBuffer;

    header->ModeDataLength = header_10->ModeDataLengthLsb;
    header->MediumType     = header_10->MediumType;

    // ATAPI Mode Parameter Header doesn't have these fields
    header->DeviceSpecificParameter = header_10->Reserved[0];
    header->BlockDescriptorLength   = header_10->Reserved[1];

    ByteCount -= sizeof(MODE_PARAMETER_HEADER_10);
    if ( ByteCount > 0 )
      RtlMoveMemory(DataBuffer+sizeof(MODE_PARAMETER_HEADER),
                    DataBuffer+sizeof(MODE_PARAMETER_HEADER_10),
                    ByteCount);

    // change ATAPI_MODE_SENSE opcode back to SCSIOP_MODE_SENSE so that we don't convert again
    Srb->Cdb[0] = SCSIOP_MODE_SENSE;

    bytesAdjust = sizeof(MODE_PARAMETER_HEADER_10) -
                  sizeof(MODE_PARAMETER_HEADER);
  }

  return bytesAdjust >> 1;  // convert to words
}

NTSTATUS
AtaXConvertSrbStatus(UCHAR SrbStatus)
{
  switch ( SRB_STATUS(SrbStatus) )
  {
    case SRB_STATUS_TIMEOUT:                /* 0x09 */
    case SRB_STATUS_COMMAND_TIMEOUT:        /* 0x0B */
    case SRB_STATUS_BUS_RESET:              /* 0x0E */
      return STATUS_IO_TIMEOUT;

    case SRB_STATUS_INVALID_REQUEST:        /* 0x06 */
    case SRB_STATUS_BAD_SRB_BLOCK_LENGTH:   /* 0x15 */
    case SRB_STATUS_BAD_FUNCTION:           /* 0x22 */
      return STATUS_INVALID_DEVICE_REQUEST;

    case SRB_STATUS_NO_DEVICE:              /* 0x08 */
    case SRB_STATUS_INVALID_LUN:            /* 0x20 */
    case SRB_STATUS_INVALID_TARGET_ID:      /* 0x21 */
    case SRB_STATUS_NO_HBA:                 /* 0x11 */
      return STATUS_DEVICE_DOES_NOT_EXIST;

    case SRB_STATUS_DATA_OVERRUN:           /* 0x12 */
      return STATUS_BUFFER_OVERFLOW;

    case SRB_STATUS_SELECTION_TIMEOUT:      /* 0x0A */
      return STATUS_DEVICE_NOT_CONNECTED;

    default:
      return STATUS_IO_DEVICE_ERROR;
  }

  return STATUS_IO_DEVICE_ERROR;
}

VOID
AtaXPrintSrbStatus(UCHAR SrbStatus)
{
  UCHAR  Status;

  //DPRINT("SrbStatus - %x\n", SrbStatus);

  if ( SrbStatus & SRB_STATUS_AUTOSENSE_VALID ) {                      // 0x80
    DPRINT("SrbStatus & SRB_STATUS_AUTOSENSE_VALID\n");
  }
  if ( SrbStatus & SRB_STATUS_QUEUE_FROZEN ) {                         // 0x40
    DPRINT("SrbStatus & SRB_STATUS_QUEUE_FROZEN\n");
  }

  Status = SRB_STATUS(SrbStatus);

  switch ( Status )
  {
    case SRB_STATUS_PENDING :                 // 0x00
      DPRINT("SrbStatus == SRB_STATUS_PENDING (0x00)\n");
      break;

    case SRB_STATUS_SUCCESS :                 // 0x01
      DPRINT("SrbStatus == SRB_STATUS_SUCCESS (0x01)\n");
      break;

    case SRB_STATUS_ABORTED :                 // 0x02
      DPRINT("SrbStatus == SRB_STATUS_ABORTED (0x02)\n");
      break;

    case SRB_STATUS_ABORT_FAILED :            // 0x03
      DPRINT("SrbStatus ==  SRB_STATUS_ABORT_FAILED (0x03)\n");
      break;

    case SRB_STATUS_ERROR :                   // 0x04
      DPRINT("SrbStatus ==  SRB_STATUS_ERROR (0x04)\n");
      break;

    case SRB_STATUS_BUSY :                    // 0x05 \n");
      DPRINT("SrbStatus ==  SRB_STATUS_BUSY (0x05)\n");
      break;

    case SRB_STATUS_INVALID_REQUEST :         // 0x06 \n");
      DPRINT("SrbStatus ==  SRB_STATUS_INVALID_REQUEST (0x06)\n");
      break;

    case SRB_STATUS_INVALID_PATH_ID :         // 0x07
      DPRINT("SrbStatus ==  SRB_STATUS_INVALID_PATH_ID (0x07)\n");
      break;

    case SRB_STATUS_NO_DEVICE :               // 0x08
      DPRINT("SrbStatus ==  SRB_STATUS_NO_DEVICE (0x08)\n");
      break;

    case SRB_STATUS_TIMEOUT :                 // 0x09
      DPRINT("SrbStatus ==  SRB_STATUS_TIMEOUT (0x09)\n");
      break;

    case SRB_STATUS_SELECTION_TIMEOUT :       // 0x0A
      DPRINT("SrbStatus ==  SRB_STATUS_SELECTION_TIMEOUT (0x0A)\n");
      break;

    case SRB_STATUS_COMMAND_TIMEOUT :         // 0x0B
      DPRINT("SrbStatus ==  SRB_STATUS_COMMAND_TIMEOUT (0x0B)\n");
      break;

    case SRB_STATUS_MESSAGE_REJECTED :        // 0x0D
      DPRINT("SrbStatus ==  SRB_STATUS_MESSAGE_REJECTED (0x0D)\n");
      break;

    case SRB_STATUS_BUS_RESET :               // 0x0E
      DPRINT("SrbStatus ==  SRB_STATUS_BUS_RESET (0x0E)\n");
      break;

    case SRB_STATUS_PARITY_ERROR :            // 0x0F
      DPRINT("SrbStatus ==  SRB_STATUS_PARITY_ERROR (0x0F)\n");
      break;

    case SRB_STATUS_REQUEST_SENSE_FAILED :    // 0x10
      DPRINT("SrbStatus ==  SRB_STATUS_REQUEST_SENSE_FAILED (0X10)\n");
      break;

    case SRB_STATUS_NO_HBA :                  // 0x11
      DPRINT("SrbStatus ==  SRB_STATUS_NO_HBA (0x11)\n");
      break;

    case SRB_STATUS_DATA_OVERRUN :            // 0x12
      DPRINT("SrbStatus ==  SRB_STATUS_DATA_OVERRUN (0x12)\n");
      break;

    case SRB_STATUS_UNEXPECTED_BUS_FREE :     // 0x13
      DPRINT("SrbStatus ==  SRB_STATUS_UNEXPECTED_BUS_FREE (0x13)\n");
      break;

    case SRB_STATUS_PHASE_SEQUENCE_FAILURE :  // 0x14
      DPRINT("SrbStatus ==  SRB_STATUS_PHASE_SEQUENCE_FAILURE (0x14)\n");
      break;

    case SRB_STATUS_BAD_SRB_BLOCK_LENGTH :    // 0x15
      DPRINT("SrbStatus ==  SRB_STATUS_BAD_SRB_BLOCK_LENGTH (0x15)\n");
      break;

    case SRB_STATUS_REQUEST_FLUSHED :         // 0x16
      DPRINT("SrbStatus ==  SRB_STATUS_REQUEST_FLUSHED (0x16)\n");
      break;

    case SRB_STATUS_INVALID_LUN :             // 0x20
      DPRINT("SrbStatus ==  SRB_STATUS_INVALID_LUN (0x20)\n");
      break;

    case SRB_STATUS_INVALID_TARGET_ID :       // 0x21
      DPRINT("SrbStatus ==  SRB_STATUS_INVALID_TARGET_ID (0x21)\n");
      break;

    case SRB_STATUS_BAD_FUNCTION :            // 0x22
      DPRINT("SrbStatus ==  SRB_STATUS_BAD_FUNCTION (0x22)\n");
      break;

    case SRB_STATUS_ERROR_RECOVERY :          // 0x23
      DPRINT("SrbStatus ==  SRB_STATUS_ERROR_RECOVERY (0x23)\n");
      break;

    case SRB_STATUS_NOT_POWERED :             // 0x24
      DPRINT("SrbStatus ==  SRB_STATUS_NOT_POWERED (0x24)\n");
      break;

    case SRB_STATUS_LINK_DOWN :               // 0x25
      DPRINT("SrbStatus ==  SRB_STATUS_LINK_DOWN (0x25)\n");
      break;

    case SRB_STATUS_INTERNAL_ERROR :          // 0x30
      DPRINT("SrbStatus ==  SRB_STATUS_INTERNAL_ERROR (0x30)\n");
      break;

    default:
      DPRINT("SrbStatus ==  Unknown!!! \n");
      ASSERT(FALSE);
      break;
  }
}

VOID
AtaXPrintSrbScsiStatus(UCHAR ScsiStatus)
{
  //DPRINT("ScsiStatus - %x\n", ScsiStatus);

  switch ( ScsiStatus )
  {
    case SCSISTAT_GOOD :                   // 0x00
      DPRINT("ScsiStatus - SCSISTAT_GOOD (0x00)\n");
      break;

    case SCSISTAT_CHECK_CONDITION :        // 0x02
      DPRINT("ScsiStatus - SCSISTAT_CHECK_CONDITION (0x02)\n");
      break;

    case SCSISTAT_CONDITION_MET :          // 0x04
      DPRINT("ScsiStatus - SCSISTAT_CONDITION_MET (0x04)\n");
      break;

    case SCSISTAT_BUSY :                   // 0x08
      DPRINT("ScsiStatus - SCSISTAT_BUSY (0x08)\n");
      break;

    case SCSISTAT_INTERMEDIATE :           // 0x10
      DPRINT("ScsiStatus - SCSISTAT_INTERMEDIATE (0x10)\n");
      break;

    case SCSISTAT_INTERMEDIATE_COND_MET :  // 0x14
      DPRINT("ScsiStatus - SCSISTAT_INTERMEDIATE_COND_MET (0x14)\n");
      break;

    case SCSISTAT_RESERVATION_CONFLICT :   // 0x18
      DPRINT("ScsiStatus - SCSISTAT_RESERVATION_CONFLICT (0x18)\n");
      break;

    case SCSISTAT_COMMAND_TERMINATED :     // 0x22
      DPRINT("ScsiStatus - SCSISTAT_COMMAND_TERMINATED (0x22)\n");
      break;

    case SCSISTAT_QUEUE_FULL :             // 0x28
      DPRINT("ScsiStatus - SCSISTAT_QUEUE_FULL (0x28)\n");
      break;

    default:
      DPRINT("ScsiStatus - Unknown SCSISTAT_\n");
      ASSERT(FALSE);
      break;
  }
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
  BOOLEAN                AhciControl = FALSE;
  BOOLEAN                SataMode;
  ULONG                  WordCount = 0;
  ULONG                  WordsThisInterrupt = 256;
  ULONG                  Status;
  ULONG                  BusMasterStatus;
  ULONG                  ix;
  PBUS_MASTER_INTERFACE  BusMasterInterface;
  ULONG                  AhciInterruptStatus;

  BusMasterInterface = &AtaXChannelFdoExtension->BusMasterInterface;
  AtaXRegisters1     = &AtaXChannelFdoExtension->BaseIoAddress1;
  AtaXRegisters2     = &AtaXChannelFdoExtension->BaseIoAddress2;
  Srb                = AtaXChannelFdoExtension->CurrentSrb;

  //DPRINT("InterruptRoutine: AtaXChannelFdoExtension - %p, Srb - %p\n", AtaXChannelFdoExtension, Srb);

  if ( AtaXChannelFdoExtension->AhciInterface )
  {
    // AHCI

    Status = AtaXChannelFdoExtension->AhciInterface->AhciInterrupt(
                 AtaXChannelFdoExtension->AhciInterface->ChannelPdoExtension,
                 &AtaXChannelFdoExtension->FullIdentifyData[0],
                 Srb);

    if ( NT_SUCCESS(Status) )
    {
      Result = TRUE;

      if ( Srb )
      {
        AtaXChannelFdoExtension->ExpectingInterrupt = FALSE;
  
        Status = SRB_STATUS_SUCCESS;
        //Status = SRB_STATUS_ERROR;
        //Status = SRB_STATUS_DATA_OVERRUN;
        Srb->SrbStatus = (UCHAR)Status;
  
        AtaXNotification(RequestComplete, AtaXChannelFdoExtension, Srb);
  
        AtaXChannelFdoExtension->WordsLeft  = 0;
        AtaXChannelFdoExtension->CurrentSrb = NULL;
  
        AtaXNotification(NextRequest, AtaXChannelFdoExtension, NULL);
      }
    }
    else
    {
      //PortInterruptStatus.AsULONG == 0
      return FALSE;
    }

    AhciInterruptStatus = 1 << AtaXChannelFdoExtension->AhciInterface->AhciChannel;
    WRITE_REGISTER_ULONG((PULONG)&AtaXChannelFdoExtension->AhciInterface->Abar->InterruptStatus, AhciInterruptStatus);

    AhciControl = TRUE;

    //DPRINT("InterruptRoutine: return TRUE\n");
    return TRUE;
  }

  if ( AtaXChannelFdoExtension->SataInterface.SataBaseAddress )
    SataMode = TRUE;
  else
    SataMode = FALSE;

  if ( AtaXChannelFdoExtension->BusMaster )
  {
    BusMasterStatus = BusMasterInterface->BusMasterReadStatus(BusMasterInterface->ChannelPdoExtension);
    DPRINT(" InterruptRoutine:  BusMasterReadStatus - %x \n",  BusMasterStatus);
    //Interrupt bit. When this bit is read as a one, all data transfered from the drive is visible in system memory
    if ( BusMasterStatus & 4 )
    {
      BusMasterInterface->BusMasterStop(BusMasterInterface->ChannelPdoExtension);
      Result = TRUE;
    }
  }

  if ( !Srb )
  {
    DPRINT("InterruptRoutine: CurrentSrb is NULL\n");
    if ( AhciControl == FALSE )
    {
      // считываем регистр состояния и сбрасываем прерывание
      StatusByte = READ_PORT_UCHAR(AtaXRegisters1->Status);
    }
    return Result;
  }

  if ( AtaXChannelFdoExtension->ExpectingInterrupt == FALSE )
  {
    DPRINT("InterruptRoutine: Unexpected interrupt\n");
    return Result;
  }

  if ( !(AtaXChannelFdoExtension->DeviceFlags[Srb->TargetId] & DFLAGS_USE_DMA) )
  {
    BusMasterStatus = BusMasterInterface->BusMasterReadStatus(BusMasterInterface->ChannelPdoExtension);

    //Interrupt bit. When this bit is read as a one, all data transfered from the drive is visible in system memory
    if ( BusMasterStatus & 4 )
      BusMasterInterface->BusMasterStop(BusMasterInterface->ChannelPdoExtension);
 }

  if ( AtaXChannelFdoExtension->BusMaster )
  {
    BusMaster = TRUE;
    AtaXChannelFdoExtension->BusMaster = FALSE; //reset BusMaster
  }

  // считываем регистр состояния и сбрасываем прерывание
  StatusByte = READ_PORT_UCHAR(AtaXRegisters1->Status);

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
      //DPRINT("InterruptRoutine: StatusByte & IDE_STATUS_BUSY. Status %x, Base IO %x\n", StatusByte, AtaXRegisters1);

      DPRINT("InterruptRoutine: FIXME AtaXNotification(RequestTimerCall(...\n");
      //AtaXNotification(RequestTimerCall,
      //                     AtaXChannelFdoExtension,
      //                     AtapiCallBack,
      //                     500);
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

  // причина этого прерывания
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
      goto CompleteRequest;

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

  //DPRINT("InterruptRoutine: InterruptReason - %x\n", InterruptReason);

  if ( InterruptReason == 1 && (StatusByte & IDE_STATUS_DRQ) )
  {
    // запись Atapi пакета
    // отправляем CDB устройству
    //DPRINT("InterruptRoutine: Writing Atapi packet.\n");
    WRITE_PORT_BUFFER_USHORT(AtaXRegisters1->Data, (PUSHORT)Srb->Cdb, 6);
    return TRUE;
  }
  else if ( InterruptReason == 0 && (StatusByte & IDE_STATUS_DRQ) )
  {
    // запись данных
    if ( AtaXChannelFdoExtension->DeviceFlags[Srb->TargetId] & DFLAGS_ATAPI_DEVICE )  // Atapi
    {
      WordCount  = READ_PORT_UCHAR(AtaXRegisters1->ByteCountLow);
      WordCount |= READ_PORT_UCHAR(AtaXRegisters1->ByteCountHigh) << 8;

      WordCount >>= 1; // bytes --> words

      //if ( WordCount != AtaXChannelFdoExtension->WordsLeft )
        //DPRINT("InterruptRoutine: %d words requested; %d words xferred\n", AtaXChannelFdoExtension->WordsLeft, WordCount);

      if ( WordCount > AtaXChannelFdoExtension->WordsLeft )
           WordCount = AtaXChannelFdoExtension->WordsLeft;
    }
    else
    {
      // Ata
      if ( AtaXChannelFdoExtension->WordsLeft < WordsThisInterrupt ) // сколько слов осталось передать?
         WordCount = AtaXChannelFdoExtension->WordsLeft;             // меньше чем сектор (512/2) - только запрашиваемые
      else
         WordCount = WordsThisInterrupt;                             // целый сектор
    }

    // убеждаемся, что команда действительно записи
    if ( Srb->SrbFlags & SRB_FLAGS_DATA_OUT )
    {
      //DPRINT("InterruptRoutine: Write interrupt\n");

      if ( SataMode )
        AtaXSataWaitOnBusy(AtaXRegisters1);
      else
        AtaXWaitOnBusy(AtaXRegisters2);

      if ( AtapiDevice || !AtaXChannelFdoExtension->DWordIO )
      {
        //DPRINT("InterruptRoutine: WRITE_PORT_BUFFER_USHORT %x, DataBuffer - %x, WordCount - %x\n", AtaXRegisters1->Data, AtaXChannelFdoExtension->DataBuffer, WordCount);
        WRITE_PORT_BUFFER_USHORT(AtaXRegisters1->Data, AtaXChannelFdoExtension->DataBuffer, WordCount);
        //DPRINT("InterruptRoutine: WRITE_PORT_BUFFER_USHORT ok\n");
      }
      else
      {
        DPRINT("InterruptRoutine: FIXME DWordIO()\n");
      }
    }
    else
    {
      // и как мы сюда попали?
      DPRINT1("InterruptRoutine: Interrupt reason %x, but Srb is for a write %x.\n", InterruptReason, Srb);
      Status = SRB_STATUS_ERROR;
      goto CompleteRequest;
    }

    // корректируем адрес буфера данных и кол-во оставшихся для записи слов
    AtaXChannelFdoExtension->DataBuffer += WordCount;
    AtaXChannelFdoExtension->WordsLeft -= WordCount;
    //DPRINT("InterruptRoutine: WordsLeft - %x\n", AtaXChannelFdoExtension->WordsLeft);
    return TRUE;
  }
  else if ( InterruptReason == 2 && (StatusByte & IDE_STATUS_DRQ) )
  {
    // чтение данных

    if ( AtaXChannelFdoExtension->DeviceFlags[Srb->TargetId] & DFLAGS_ATAPI_DEVICE )
    {
      // чтение Atapi - данных
      WordCount = READ_PORT_UCHAR(AtaXRegisters1->ByteCountLow);
      WordCount |= READ_PORT_UCHAR(AtaXRegisters1->ByteCountHigh) << 8;

      WordCount >>= 1; // bytes --> words

      //if ( WordCount != AtaXChannelFdoExtension->WordsLeft )
        //DPRINT("InterruptRoutine: %d words requested; %d words xferred\n", AtaXChannelFdoExtension->WordsLeft, WordCount);

      if ( WordCount > AtaXChannelFdoExtension->WordsLeft )
        WordCount = AtaXChannelFdoExtension->WordsLeft;
    }
    else
    {
      // чтение данных Ata
      if ( AtaXChannelFdoExtension->WordsLeft < WordsThisInterrupt )  // сколько слов осталось передать?
        WordCount = AtaXChannelFdoExtension->WordsLeft;               // меньше чем сектор (512/2) - только запрашиваемые
      else
        WordCount = WordsThisInterrupt;                               // целый сектор
    }

    // убеждаемся, что команда действительно чтения
    if ( Srb->SrbFlags & SRB_FLAGS_DATA_IN )
    {
      DPRINT("InterruptRoutine: Read interrupt\n");

      if ( SataMode )
        AtaXSataWaitOnBusy(AtaXRegisters1);
      else
        AtaXWaitOnBusy(AtaXRegisters2);

      if ( AtapiDevice || !AtaXChannelFdoExtension->DWordIO )
      {
        READ_PORT_BUFFER_USHORT(AtaXRegisters1->Data, AtaXChannelFdoExtension->DataBuffer, WordCount);
        //DPRINT("InterruptRoutine: READ_PORT_BUFFER_USHORT %x, DataBuffer - %x, WordCount - %x\n", AtaXRegisters1->Data, AtaXChannelFdoExtension->DataBuffer, WordCount);

        DPRINT("InterruptRoutine: Srb->Cdb[0]- %x\n", Srb->Cdb[0]);
        if ( Srb->Cdb[0] == SCSIOP_REQUEST_SENSE )
        {
          DPRINT("InterruptRoutine: Srb->DataTransferLength - %x\n", Srb->DataTransferLength);
          DPRINT("InterruptRoutine: AtaXChannelFdoExtension->DataBuffer - %x\n", AtaXChannelFdoExtension->DataBuffer);
          DPRINT("InterruptRoutine: AtaXChannelFdoExtension->DataBuffer[0]  - %x\n", AtaXChannelFdoExtension->DataBuffer[0]);
          DPRINT("InterruptRoutine: AtaXChannelFdoExtension->DataBuffer[1]  - %x\n", AtaXChannelFdoExtension->DataBuffer[1]);
          DPRINT("InterruptRoutine: AtaXChannelFdoExtension->DataBuffer[2]  - %x\n", AtaXChannelFdoExtension->DataBuffer[2]);
          DPRINT("InterruptRoutine: AtaXChannelFdoExtension->DataBuffer[3]  - %x\n", AtaXChannelFdoExtension->DataBuffer[3]);
          DPRINT("InterruptRoutine: AtaXChannelFdoExtension->DataBuffer[4]  - %x\n", AtaXChannelFdoExtension->DataBuffer[4]);
          DPRINT("InterruptRoutine: AtaXChannelFdoExtension->DataBuffer[5]  - %x\n", AtaXChannelFdoExtension->DataBuffer[5]);
          DPRINT("InterruptRoutine: AtaXChannelFdoExtension->DataBuffer[6]  - %x\n", AtaXChannelFdoExtension->DataBuffer[6]);
          DPRINT("InterruptRoutine: AtaXChannelFdoExtension->DataBuffer[7]  - %x\n", AtaXChannelFdoExtension->DataBuffer[7]);
          DPRINT("InterruptRoutine: AtaXChannelFdoExtension->DataBuffer[8]  - %x\n", AtaXChannelFdoExtension->DataBuffer[8]);
    
          if ( Srb->DataBuffer )
          {
            DPRINT("InterruptRoutine: *((PUCHAR)Srb->DataBuffer + 00) - %x\n", *((PUCHAR)Srb->DataBuffer + 0));
            DPRINT("InterruptRoutine: *((PUCHAR)Srb->DataBuffer + 01) - %x\n", *((PUCHAR)Srb->DataBuffer + 1));
            DPRINT("InterruptRoutine: *((PUCHAR)Srb->DataBuffer + 02) - %x\n", *((PUCHAR)Srb->DataBuffer + 2));
            DPRINT("InterruptRoutine: *((PUCHAR)Srb->DataBuffer + 03) - %x\n", *((PUCHAR)Srb->DataBuffer + 3));
            DPRINT("InterruptRoutine: *((PUCHAR)Srb->DataBuffer + 04) - %x\n", *((PUCHAR)Srb->DataBuffer + 4));
            DPRINT("InterruptRoutine: *((PUCHAR)Srb->DataBuffer + 05) - %x\n", *((PUCHAR)Srb->DataBuffer + 5));
            DPRINT("InterruptRoutine: *((PUCHAR)Srb->DataBuffer + 06) - %x\n", *((PUCHAR)Srb->DataBuffer + 6));
            DPRINT("InterruptRoutine: *((PUCHAR)Srb->DataBuffer + 07) - %x\n", *((PUCHAR)Srb->DataBuffer + 7));
            DPRINT("InterruptRoutine: *((PUCHAR)Srb->DataBuffer + 08) - %x\n", *((PUCHAR)Srb->DataBuffer + 8));
            DPRINT("InterruptRoutine: *((PUCHAR)Srb->DataBuffer + 09) - %x\n", *((PUCHAR)Srb->DataBuffer + 9));
            DPRINT("InterruptRoutine: *((PUCHAR)Srb->DataBuffer + 10) - %x\n", *((PUCHAR)Srb->DataBuffer + 10));
            DPRINT("InterruptRoutine: *((PUCHAR)Srb->DataBuffer + 11) - %x\n", *((PUCHAR)Srb->DataBuffer + 11));
            DPRINT("InterruptRoutine: *((PUCHAR)Srb->DataBuffer + 12) - %x\n", *((PUCHAR)Srb->DataBuffer + 12));
            DPRINT("InterruptRoutine: *((PUCHAR)Srb->DataBuffer + 13) - %x\n", *((PUCHAR)Srb->DataBuffer + 13));
            DPRINT("InterruptRoutine: *((PUCHAR)Srb->DataBuffer + 14) - %x\n", *((PUCHAR)Srb->DataBuffer + 14));
            DPRINT("InterruptRoutine: *((PUCHAR)Srb->DataBuffer + 15) - %x\n", *((PUCHAR)Srb->DataBuffer + 15));
            DPRINT("InterruptRoutine: *((PUCHAR)Srb->DataBuffer + 16) - %x\n", *((PUCHAR)Srb->DataBuffer + 16));
            DPRINT("InterruptRoutine: *((PUCHAR)Srb->DataBuffer + 17) - %x\n", *((PUCHAR)Srb->DataBuffer + 17));
          }
        }
      }
      else
      {
        DPRINT("InterruptRoutine: FIXME DWordIO()\n");
      }
    }
    else
    {
      // и как мы сюда попали?
      DPRINT1("InterruptRoutine: Interrupt reason %x, but Srb is for a read %x.\n", InterruptReason, Srb);
      Status = SRB_STATUS_ERROR;
      goto CompleteRequest;
    }

    // переводим данные ATAPI назад в SCSI данные, если это необходимо 
    if ( Srb->Cdb[0] == ATAPI_MODE_SENSE &&
         AtaXChannelFdoExtension->DeviceFlags[Srb->TargetId] & DFLAGS_ATAPI_DEVICE )
    {
      //конвертируем и корректируем WordCount
      WordCount -= Atapi2Scsi(Srb, (char *)AtaXChannelFdoExtension->DataBuffer, WordCount << 1);
    }

    // корректируем адрес буфера данных и кол-во оставшихся для записи слов
    AtaXChannelFdoExtension->DataBuffer += WordCount;
    AtaXChannelFdoExtension->WordsLeft -= WordCount;

    // проверяем что все данные прочитаны
    if ( AtaXChannelFdoExtension->WordsLeft == 0 )
    {
      if ( AtaXChannelFdoExtension->DeviceFlags[Srb->TargetId] & DFLAGS_ATAPI_DEVICE )  // Atapi
      {
        // возвращаем корректный размер сектора 2048
        // у определенных устройств будет количество секторов == 0x00
    
        if ( (Srb->Cdb[0] == SCSIOP_READ_CAPACITY) && //0x25
             ((AtaXChannelFdoExtension->FullIdentifyData[Srb->TargetId].GeneralConfiguration >> 8) & 0x1f) == 0x05 )
        {
          DPRINT("InterruptRoutine: FullIdentifyData.GeneralConfiguration - %x\n", AtaXChannelFdoExtension->FullIdentifyData[Srb->TargetId].GeneralConfiguration);
          DPRINT("InterruptRoutine: AtaXChannelFdoExtension->DataBuffer - %p, *DataBuffer - %x\n", AtaXChannelFdoExtension->DataBuffer, *(PULONG)AtaXChannelFdoExtension->DataBuffer);

          AtaXChannelFdoExtension->DataBuffer -= WordCount;

          DPRINT("InterruptRoutine: AtaXChannelFdoExtension->DataBuffer - %p, *DataBuffer - %x\n", AtaXChannelFdoExtension->DataBuffer, *(PULONG)AtaXChannelFdoExtension->DataBuffer);
          if ( AtaXChannelFdoExtension->DataBuffer[0] == 0 )
            *((ULONG *) &(AtaXChannelFdoExtension->DataBuffer[0]) ) = 0xFFFFFF7F;

          DPRINT("InterruptRoutine: AtaXChannelFdoExtension->DataBuffer - %p, *DataBuffer - %x\n", AtaXChannelFdoExtension->DataBuffer, *(PULONG)AtaXChannelFdoExtension->DataBuffer);
          *((ULONG *) &(AtaXChannelFdoExtension->DataBuffer[2])) = 0x00080000;

          DPRINT("InterruptRoutine: AtaXChannelFdoExtension->DataBuffer - %p, *DataBuffer - %x\n", AtaXChannelFdoExtension->DataBuffer, *(PULONG)AtaXChannelFdoExtension->DataBuffer);
          AtaXChannelFdoExtension->DataBuffer += WordCount;
        }
      }
      else
      {
        // завершение для Ata
        if ( AtaXChannelFdoExtension->WordsLeft )
          Status = SRB_STATUS_DATA_OVERRUN;
        else
          Status = SRB_STATUS_SUCCESS;

        //DPRINT("InterruptRoutine: Read interrupt goto CompleteRequest. Status - %x\n", Status);
        goto CompleteRequest;
      }
    }

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
      AtaXChannelFdoExtension->WordsLeft = 0;

      // See 3.1. Status Bit Interpretation ("Programming Interface for Bus Master IDE Controller")

      /* Interrupt:  This bit is set by the rising edge of the IDE interrupt line.  This bit is cleared when a
         '1' is written to it by software.  Software can use this bit to determine if an IDE device has
         asserted its interrupt line. When this bit is read as a one, all data transfered from the drive is
         visible in system memory. */

      if ( BusMasterStatus & 4 )
        Status = SRB_STATUS_SUCCESS;


      /* Error: This bit is set when the controller encounters an error in transferring data to/from
         memory. The exact error condition is bus specific and can be determined in a bus specific
         manner. This bit is cleared when a '1' is written to it by software. */

      if ( BusMasterStatus & 2 )
        Status = SRB_STATUS_ERROR;


      /* Bus Master IDE Active: This bit is set when the Start bit is written to the Command  register.
         This bit is cleared when the last transfer for a region is performed, where EOT for that region is
         set in the region descriptor. It is also cleared when the Start bit is cleared in the Command
         register.  When this bit is read as a zero, all data transfered from the drive during the previous
         bus master command is visible in system memory, unless the bus master command was aborted. */

      if ( BusMasterStatus & 1 )
        Status = SRB_STATUS_DATA_OVERRUN;

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
        if ( SataMode )
        {
          // Считываем альтернативный регистр состояния
          StatusByte = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus);
          DPRINT("AtaXIssueIdentify: Status (AlternateStatus) before read words - %x\n", StatusByte);
          // Считываем  регистр состояния
          StatusByte = READ_PORT_UCHAR(AtaXRegisters1->Status);
          DPRINT("AtaXIssueIdentify: Status before read words - %x\n", StatusByte);
        }
        else
        {
          // Считываем альтернативный регистр состояния
          StatusByte = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus);
          DPRINT("AtaXIssueIdentify: Status (AlternateStatus) before read words - %x\n", StatusByte);
        }

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
          if ( SataMode )
          {
            // Считываем альтернативный регистр состояния
            StatusByte = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus);
            DPRINT("AtaXIssueIdentify: Status (AlternateStatus) before read words - %x\n", StatusByte);
            // Считываем  регистр состояния
            StatusByte = READ_PORT_UCHAR(AtaXRegisters1->Status);
            DPRINT("AtaXIssueIdentify: Status before read words - %x\n", StatusByte);
          }
          else
          {
            // Считываем альтернативный регистр состояния
            StatusByte = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus);
            DPRINT("AtaXIssueIdentify: Status (AlternateStatus) before read words - %x\n", StatusByte);
          }

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

        // команда завершена
        AtaXNotification(RequestComplete, AtaXChannelFdoExtension, Srb);
      }
    }
    else
    {
      DPRINT("InterruptRoutine: No SRB!\n");
    }

    // готовность к следующему запросу
    if ( !(AtaXChannelFdoExtension->RDP) )
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
    // неожиданное прерывание
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

VOID 
AtaXProcessCompletedRequest(
    IN PFDO_CHANNEL_EXTENSION AtaXChannelFdoExtension,
    IN PSCSI_REQUEST_BLOCK_INFO SrbInfo,
    OUT PBOOLEAN NeedToCallStartIo)
{
  PPDO_DEVICE_EXTENSION  AtaXDevicePdoExtension;
  PSCSI_REQUEST_BLOCK    Srb;
  PIRP                   Irp;

  Srb = SrbInfo->Srb;
  Irp = Srb->OriginalRequest;
  //DPRINT("AtaXProcessCompletedRequest: Srb - %p, Irp - %p\n", Srb, Irp);

  AtaXDevicePdoExtension = (AtaXChannelFdoExtension->AtaXDevicePdo[Srb->TargetId])->DeviceExtension;

  SrbInfo->Srb = NULL;

  if ( Srb->SrbFlags & SRB_FLAGS_DISABLE_DISCONNECT )
  {
    DPRINT("AtaXProcessCompletedRequest: SRB_FLAGS_DISABLE_DISCONNECT. Srb->SrbFlags - %x\n", Srb->SrbFlags);
    KeAcquireSpinLockAtDpcLevel(&AtaXChannelFdoExtension->SpinLock);
    AtaXChannelFdoExtension->Flags |= ATAX_DISCONNECT_ALLOWED;
    if ( !(AtaXChannelFdoExtension->InterruptData.Flags & ATAX_RESET) )
        AtaXChannelFdoExtension->TimerCount = -1;
    KeReleaseSpinLockFromDpcLevel(&AtaXChannelFdoExtension->SpinLock);

    DPRINT("AtaXProcessCompletedRequest: AtaXChannelFdoExtension->Flags - %x\n", AtaXChannelFdoExtension->Flags);

    if ( !(AtaXChannelFdoExtension->Flags & ATAX_REQUEST_PENDING) &&
         !(AtaXChannelFdoExtension->Flags & ATAX_DEVICE_BUSY)     &&
         !(*NeedToCallStartIo) )
    {
      // нет занятости, и есть ожидание запроса
      DPRINT("AtaXProcessCompletedRequest: !ATAX_REQUEST_PENDING && !ATAX_DEVICE_BUSY\n");
      DPRINT("AtaXProcessCompletedRequest: IoStartNextPacket FIXME\n");
      IoStartNextPacket(AtaXChannelFdoExtension->CommonExtension.SelfDevice, FALSE);
    }
  }

  KeAcquireSpinLockAtDpcLevel(&AtaXChannelFdoExtension->SpinLock);

  // сохраняем размер данных в IoStatus.Information
  Irp->IoStatus.Information = Srb->DataTransferLength;

  SrbInfo->SequenceNumber = 0;

  // уменьшаем счетчик очереди
  AtaXDevicePdoExtension->QueueCount--;
  //DPRINT("AtaXProcessCompletedRequest: AtaXDevicePdoExtension->QueueCount - %x\n", AtaXDevicePdoExtension->QueueCount);

  if ( Srb->QueueTag != SP_UNTAGGED )
  {
    DPRINT("AtaXProcessCompletedRequest: Srb->QueueTag != SP_UNTAGGED FIXME\n");
  }
 
  SrbInfo = NULL;

  if ( AtaXChannelFdoExtension->Flags & ATAX_REQUEST_PENDING )
  {
    DPRINT("AtaXProcessCompletedRequest: if ATAX_REQUEST_PENDING\n");
    AtaXChannelFdoExtension->Flags &= ~ATAX_REQUEST_PENDING;
    *NeedToCallStartIo = TRUE;
  }

  if ( SRB_STATUS(Srb->SrbStatus) == SRB_STATUS_SUCCESS )
  {
    //DPRINT("AtaXProcessCompletedRequest: SRB_STATUS_SUCCESS\n");
    Irp->IoStatus.Status = STATUS_SUCCESS;

    if ( !(Srb->SrbFlags & SRB_FLAGS_BYPASS_FROZEN_QUEUE) &&
         (AtaXDevicePdoExtension->RequestTimeout == -1) )
    {
      //DPRINT("AtaXProcessCompletedRequest: SRB_STATUS_SUCCESS  and  !SRB_FLAGS_BYPASS_FROZEN_QUEUE\n");
      AtaXGetNextRequest(AtaXChannelFdoExtension, AtaXDevicePdoExtension);
    }
    else
    {
      DPRINT("AtaXProcessCompletedRequest: SRB_STATUS_SUCCESS  and  SRB_FLAGS_BYPASS_FROZEN_QUEUE\n");
      KeReleaseSpinLockFromDpcLevel(&AtaXChannelFdoExtension->SpinLock);
    }

    IoCompleteRequest(Irp, IO_DISK_INCREMENT);
    return;
  }

  DPRINT("AtaXProcessCompletedRequest: SrbStatus not SUCCESS !\n");

  Irp->IoStatus.Status = AtaXConvertSrbStatus(Srb->SrbStatus);

  DPRINT("AtaXProcessCompletedRequest: Srb->ScsiStatus - %x\n", Srb->ScsiStatus);

  if ( AtaXDEBUG )
    AtaXPrintSrbStatus(Srb->SrbStatus);

  if ( AtaXDEBUG )
    AtaXPrintSrbScsiStatus(Srb->ScsiStatus);

  if ( Srb->SrbFlags & SRB_FLAGS_USE_DMA )
  {
    DPRINT("AtaXProcessCompletedRequest: FIXME! Error DMA transfer. Down speed to PIO. Srb - %p \n", Srb);
  }

  if ( (Srb->ScsiStatus == SCSISTAT_BUSY ||
        Srb->SrbStatus  == SRB_STATUS_BUSY ||
        Srb->ScsiStatus == SCSISTAT_QUEUE_FULL)
       && !(Srb->SrbFlags & SRB_FLAGS_BYPASS_FROZEN_QUEUE) )
  {
    DPRINT("AtaXProcessCompletedRequest: It's not a bypass, it's busy or the queue is full\n");

    if ( AtaXDevicePdoExtension->Flags & (LUNEX_FROZEN_QUEUE | LUNEX_BUSY) )
    {
      DPRINT("AtaXProcessCompletedRequest: it's being requeued\n");

      Srb->SrbStatus = SRB_STATUS_PENDING;
      Srb->ScsiStatus = 0;

      if ( Srb->SrbFlags & SRB_FLAGS_USE_DMA )
      {
        DPRINT("AtaXProcessCompletedRequest: DMA Srb being requeued (SRB_FLAGS_BYPASS_FROZEN_QUEUE)\n");
        ASSERT(FALSE);
      }

      if ( !KeInsertByKeyDeviceQueue(&AtaXDevicePdoExtension->DeviceQueue,
                                    &Irp->Tail.Overlay.DeviceQueueEntry,
                                    Srb->QueueSortKey) )
      {
        DPRINT("AtaXProcessCompletedRequest: !KeInsertByKeyDeviceQueue!\n");
        Srb->SrbStatus = SRB_STATUS_ERROR;
        Srb->ScsiStatus = SCSISTAT_BUSY;

        ASSERT(FALSE);
        goto Error;
      }

      KeReleaseSpinLockFromDpcLevel(&AtaXChannelFdoExtension->SpinLock);
    }
    else if (AtaXDevicePdoExtension->AttemptCount++ < 20)
    {
      DPRINT("AtaXProcessCompletedRequest: LUN is still busy\n");

      Srb->ScsiStatus = 0;
      Srb->SrbStatus = SRB_STATUS_PENDING;

      AtaXDevicePdoExtension->BusyRequest = Irp;
      AtaXDevicePdoExtension->Flags |= LUNEX_BUSY;

      KeReleaseSpinLockFromDpcLevel(&AtaXChannelFdoExtension->SpinLock);
    }
    else
    {

Error:
      DPRINT("AtaXProcessCompletedRequest: Freeze the queue\n");

      Srb->SrbStatus |= SRB_STATUS_QUEUE_FROZEN;

      AtaXDevicePdoExtension->Flags |= LUNEX_FROZEN_QUEUE;
      AtaXDevicePdoExtension->Flags &= ~LUNEX_FULL_QUEUE;

      KeReleaseSpinLockFromDpcLevel(&AtaXChannelFdoExtension->SpinLock);

      Irp->IoStatus.Status = STATUS_DEVICE_NOT_READY;
      IoCompleteRequest(Irp, IO_DISK_INCREMENT);
    }

    return;
  }

  if ( ((Srb->ScsiStatus != SCSISTAT_CHECK_CONDITION) ||
       (Srb->SrbStatus & SRB_STATUS_AUTOSENSE_VALID) ||
       !Srb->SenseInfoBuffer || !Srb->SenseInfoBufferLength)
     && Srb->SrbFlags & SRB_FLAGS_NO_QUEUE_FREEZE )
  {

    if ( AtaXDevicePdoExtension->RequestTimeout == -1 )
    {
      DPRINT("AtaXProcessCompletedRequest: AtaXDevicePdoExtension->RequestTimeout - %x\n", AtaXDevicePdoExtension->RequestTimeout);
      DPRINT("AtaXProcessCompletedRequest: Start the next request\n");
      AtaXGetNextRequest(AtaXChannelFdoExtension, AtaXDevicePdoExtension);
    }
    else
    {
      DPRINT("AtaXProcessCompletedRequest: AtaXDevicePdoExtension->RequestTimeout - %x\n", AtaXDevicePdoExtension->RequestTimeout);
      KeReleaseSpinLockFromDpcLevel(&AtaXChannelFdoExtension->SpinLock);
    }
  }
  else
  {
      DPRINT1("AtaXProcessCompletedRequest: Freeze the queue\n");
      Srb->SrbStatus |= SRB_STATUS_QUEUE_FROZEN;
      AtaXDevicePdoExtension->Flags |= LUNEX_FROZEN_QUEUE;
      
      if ( Srb->ScsiStatus == SCSISTAT_CHECK_CONDITION &&
         !(Srb->SrbStatus & SRB_STATUS_AUTOSENSE_VALID) &&
         Srb->SenseInfoBuffer && Srb->SenseInfoBufferLength )
      {
        DPRINT("AtaXProcessCompletedRequest: we need a request sense\n");
  
        if ( AtaXDevicePdoExtension->Flags & LUNEX_BUSY )
        {
          DPRINT("AtaXProcessCompletedRequest: AtaXDevicePdoExtension->Flags & LUNEX_BUSY \n");
          DPRINT("AtaXProcessCompletedRequest: Requeueing busy request to allow request sense\n");
  
          if ( !KeInsertByKeyDeviceQueue(&AtaXDevicePdoExtension->DeviceQueue,
               &AtaXDevicePdoExtension->BusyRequest->Tail.Overlay.DeviceQueueEntry,
               Srb->QueueSortKey) )
          {
            DPRINT("AtaXProcessCompletedRequest: We should never get here \n");
            ASSERT(FALSE);
  
            KeReleaseSpinLockFromDpcLevel(&AtaXChannelFdoExtension->SpinLock);
            IoCompleteRequest(Irp, IO_DISK_INCREMENT);

            return;
          }
  
          AtaXDevicePdoExtension->Flags &= ~(LUNEX_FULL_QUEUE | LUNEX_BUSY);
        }
  
        KeReleaseSpinLockFromDpcLevel(&AtaXChannelFdoExtension->SpinLock);
        AtaXSendRequestSense(AtaXChannelFdoExtension, Srb);
        return;
      }
  
      KeReleaseSpinLockFromDpcLevel(&AtaXChannelFdoExtension->SpinLock);
  }

  IoCompleteRequest(Irp, IO_DISK_INCREMENT);
  DPRINT("AtaXProcessCompletedRequest: exit \n");
}

BOOLEAN
AtaXSaveInterruptData(IN PVOID Context)
{
  PATAX_SAVE_INTERRUPT      InterruptContext = Context;
  PSCSI_REQUEST_BLOCK       Srb;
  PSCSI_REQUEST_BLOCK_INFO  SrbInfo;
  PFDO_CHANNEL_EXTENSION    AtaXChannelFdoExtension;
  PPDO_DEVICE_EXTENSION     AtaXDevicePdoExtension;

  AtaXChannelFdoExtension = InterruptContext->DeviceExtension;

  if ( !(AtaXChannelFdoExtension->InterruptData.Flags & ATAX_NOTIFICATION_NEEDED) )
  {
    DPRINT("AtaXSaveInterruptData: return FALSE\n");
    return FALSE;
  }

  *InterruptContext->InterruptData = AtaXChannelFdoExtension->InterruptData;

  AtaXChannelFdoExtension->InterruptData.Flags &= (ATAX_RESET | ATAX_RESET_REQUEST | ATAX_DISABLE_INTERRUPTS);
  AtaXChannelFdoExtension->InterruptData.CompletedRequests = NULL;

  SrbInfo = InterruptContext->InterruptData->CompletedRequests;

  while ( SrbInfo )
  {
    ASSERT(SrbInfo->Srb);
    Srb = SrbInfo->Srb;

    AtaXDevicePdoExtension = AtaXChannelFdoExtension->AtaXDevicePdo[Srb->TargetId]->DeviceExtension;

    if ( Srb->SrbStatus != SRB_STATUS_SUCCESS &&
         Srb->SrbStatus != SRB_STATUS_PENDING )
    {
      DPRINT("AtaXSaveInterruptData: Srb->SenseInfoBuffer       - %p\n", Srb->SenseInfoBuffer);
      DPRINT("AtaXSaveInterruptData: Srb->SenseInfoBufferLength - %p\n", Srb->SenseInfoBufferLength);

      if ( Srb->SenseInfoBuffer && Srb->SenseInfoBufferLength &&
           Srb->ScsiStatus == SCSISTAT_CHECK_CONDITION )
      {
        if ( AtaXDevicePdoExtension->Flags & LUNEX_NEED_REQUEST_SENSE )
        {
          // Это означает: мы попытались отправить REQUEST SENSE, но безуспешно
          Srb->ScsiStatus = SCSISTAT_GOOD;
          Srb->SrbStatus = SRB_STATUS_REQUEST_SENSE_FAILED;
        }
        else
        {
          AtaXDevicePdoExtension->Flags |= LUNEX_NEED_REQUEST_SENSE;
        }

        if ( Srb->ScsiStatus == SCSISTAT_QUEUE_FULL )
          ASSERT(FALSE);
      }
    }

    AtaXDevicePdoExtension->RequestTimeout = -1;

    SrbInfo = SrbInfo->CompletedRequests;
  }

  return TRUE;
}

VOID NTAPI
AtaXDpc(
    IN PKDPC Dpc,
    IN PDEVICE_OBJECT AtaXChannelFdo,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2)
{
  PFDO_CHANNEL_EXTENSION    AtaXChannelFdoExtension;
  ATAX_INTERRUPT_DATA       InterruptData;
  PSCSI_REQUEST_BLOCK_INFO  SrbInfo;
  ATAX_SAVE_INTERRUPT       Context;
  BOOLEAN                   NeedToStartIo;

  AtaXChannelFdoExtension = (PFDO_CHANNEL_EXTENSION)AtaXChannelFdo->DeviceExtension;

  KeAcquireSpinLockAtDpcLevel(&AtaXChannelFdoExtension->SpinLock);
  RtlZeroMemory(&InterruptData, sizeof(ATAX_INTERRUPT_DATA));

  Context.InterruptData = &InterruptData;
  Context.DeviceExtension = AtaXChannelFdoExtension;

  if ( !KeSynchronizeExecution(AtaXChannelFdoExtension->InterruptObject,
                              (PKSYNCHRONIZE_ROUTINE)AtaXSaveInterruptData,
                              &Context) )
  {
    KeReleaseSpinLockFromDpcLevel(&AtaXChannelFdoExtension->SpinLock);
    DPRINT("AtaXDpc return\n");
    return;
  }

  if ( InterruptData.CompletedRequests )
  {
    PSCSI_REQUEST_BLOCK    Srb;
    PBUS_MASTER_INTERFACE  BusMasterInterface;

    Srb = InterruptData.CompletedRequests->Srb;
    if ( Srb->SrbFlags & 0xC0 )
    {
      BusMasterInterface = &AtaXChannelFdoExtension->BusMasterInterface;
      DPRINT("AtaXDpc:  BusMasterInterface - %p\n", BusMasterInterface);
      if ( Srb->SrbFlags & SRB_FLAGS_USE_DMA )
        BusMasterInterface->BusMasterComplete(BusMasterInterface->ChannelPdoExtension);
    }
  }

  DPRINT("AtaXDpc:  InterruptData.Flags - %x \n", InterruptData.Flags);

  if ( InterruptData.Flags & ATAX_FLUSH_ADAPTERS )
    ASSERT(FALSE);      // TODO: Implement

  if ( InterruptData.Flags & ATAX_MAP_TRANSFER )
    ASSERT(FALSE);      // TODO: Implement

  if ( InterruptData.Flags & ATAX_TIMER_NEEDED )
    ASSERT(FALSE);      // TODO: Implement

  DPRINT("AtaXDpc: AtaXChannelFdoExtension->Flags - %p \n", AtaXChannelFdoExtension->Flags);

  // если готовность к следующему запросу
  if ( InterruptData.Flags & ATAX_NEXT_REQUEST_READY )
  {
    // проверка на двойной запрос
    if ( (AtaXChannelFdoExtension->Flags & (ATAX_DEVICE_BUSY | ATAX_DISCONNECT_ALLOWED))
                                        == (ATAX_DEVICE_BUSY | ATAX_DISCONNECT_ALLOWED) )
    {
      DPRINT("AtaXDpc: Clear busy flag set by AtaXStartPacket()\n");
      AtaXChannelFdoExtension->Flags &= ~ATAX_DEVICE_BUSY;
  
      // готовы к следующему пакету, а также нет сброса?
      if ( !(InterruptData.Flags & ATAX_RESET) )
        AtaXChannelFdoExtension->TimerCount = -1;
    }
    else
    {
      AtaXChannelFdoExtension->Flags &= ~ATAX_DEVICE_BUSY;
      DPRINT("AtaXDpc: Not busy, but not ready for the next request\n");
      InterruptData.Flags &= ~ATAX_NEXT_REQUEST_READY;
    }
  }

  if ( InterruptData.Flags & ATAX_RESET_REPORTED ) // сброс?
    AtaXChannelFdoExtension->TimerCount = 4;

  KeReleaseSpinLockFromDpcLevel(&AtaXChannelFdoExtension->SpinLock);

  // если готовы для следующего пакета, запускаем его
  if ( InterruptData.Flags & ATAX_NEXT_REQUEST_READY )
    IoStartNextPacket(AtaXChannelFdoExtension->CommonExtension.SelfDevice, FALSE);

  NeedToStartIo = FALSE;

  DPRINT("AtaXDpc: InterruptData.CompletedRequests - %p\n", InterruptData.CompletedRequests);

  // цикл для списка завершенных запросов
  while ( InterruptData.CompletedRequests )
  {
    SrbInfo = InterruptData.CompletedRequests;
    DPRINT("AtaXDpc: SrbInfo->CompletedRequests - %p\n", SrbInfo->CompletedRequests);
    InterruptData.CompletedRequests = SrbInfo->CompletedRequests;

    if ( SrbInfo->CompletedRequests )
      SrbInfo->CompletedRequests = NULL;

    // обработка завершенных запросов
    AtaXProcessCompletedRequest(AtaXChannelFdoExtension, SrbInfo, &NeedToStartIo);
  }

  if ( NeedToStartIo )
  {
    ASSERT(AtaXChannelFdo->CurrentIrp != NULL);
    AtaXStartIo(AtaXChannelFdo, AtaXChannelFdo->CurrentIrp);
  }

  if ( InterruptData.Flags & ATAX_ENABLE_INT_REQUEST )
    ASSERT(FALSE);

  DPRINT("AtaXDpc return\n");
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
  PDEVICE_RELATIONS       DeviceRelations = NULL;
  BOOLEAN                 IsResponded;

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
  IsResponded = AtaXDetectDevices(AtaXChannelFdoExtension);
  DPRINT("AtaXChannelFdoQueryDeviceRelations: AtaXDetectDevices return - %x\n", IsResponded);

  // Создаем и заполняем структуру DEVICE_RELATIONS.
  // Указатель на нее (DeviceRelations) сохраняем в Irp->IoStatus.Information
  if ( Count == 0 )
    DeviceRelations = (PDEVICE_RELATIONS)ExAllocatePool(PagedPool, sizeof(DEVICE_RELATIONS));
  else
    DeviceRelations = (PDEVICE_RELATIONS)ExAllocatePool(PagedPool, sizeof(DEVICE_RELATIONS) + sizeof(PDEVICE_OBJECT) * (Count - 1));

  if ( !DeviceRelations )
    return STATUS_INSUFFICIENT_RESOURCES;
 
  DeviceRelations->Count = Count;

  for ( TargetId = 0, ix = 0; TargetId < MAX_IDE_DEVICE; TargetId++ )
  {
    if ( AtaXChannelFdoExtension->AtaXDevicePdo[TargetId] )
    {
      ObReferenceObject(AtaXChannelFdoExtension->AtaXDevicePdo[TargetId]);
      DeviceRelations->Objects[ix++] = AtaXChannelFdoExtension->AtaXDevicePdo[TargetId];
    }
  }
  
  Irp->IoStatus.Information = (ULONG_PTR)DeviceRelations;
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

  Status = AtaXQueryBusInterface(AtaXChannelFdo);                                              // запрос интерфейса шины
  DPRINT("AtaXChannelFdoStartDevice: AtaXQueryBusInterface return Status - %p\n", Status);

  Status = AtaXQueryAhciInterface(AtaXChannelFdo);                                            // запрос интерфейса AHCI порта
  DPRINT("AtaXChannelFdoStartDevice: AtaXQueryAhciInterface return Status - %p\n", Status);

  if ( NT_SUCCESS(Status) && AtaXChannelFdoExtension->AhciInterface )
  {
    // AHCI controller
    PAHCI_INTERRUPT_RESOURCE  InterruptResource;

    InterruptResource = AtaXChannelFdoExtension->AhciInterface->InterruptResource;
  
    AtaXChannelFdoExtension->InterruptVector           = InterruptResource->InterruptVector;
    AtaXChannelFdoExtension->InterruptLevel            = InterruptResource->InterruptLevel;
    AtaXChannelFdoExtension->InterruptFlags            = InterruptResource->InterruptFlags;
    AtaXChannelFdoExtension->InterruptShareDisposition = InterruptResource->InterruptShareDisposition;
    AtaXChannelFdoExtension->InterruptAffinity         = InterruptResource->InterruptAffinity;

    Status = STATUS_SUCCESS;
  }
  else
  {
    Status = AtaXQuerySataInterface(AtaXChannelFdo);                                          // запрос SATA интерфейса
    DPRINT("AtaXChannelFdoStartDevice: AtaXQuerySataInterface return Status - %p\n", Status);

    if ( NT_SUCCESS(Status) && AtaXChannelFdoExtension->SataInterface.Size )
    {
      DPRINT("AtaXChannelFdoStartDevice: AtaXChannelFdoExtension->SataInterface - %p\n", AtaXChannelFdoExtension->SataInterface);
    }
    // Определяем ресурсы
    Status = AtaXParseTranslatedResources(AtaXChannelFdoExtension, ResourcesTranslated);
  }

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
       // Bit 1 (nIEN): 0 - Enable Interrupt, 1 - Disable Interrupt
       WRITE_PORT_UCHAR(AtaXChannelFdoExtension->BaseIoAddress2.DeviceControl, 0);

       KeInitializeSpinLock(&AtaXChannelFdoExtension->SpinLock);

       KeInitializeDpc(&AtaXChannelFdoExtension->Dpc,
                       (PKDEFERRED_ROUTINE)AtaXDpc,
                       AtaXChannelFdo);

       // set flag that it's allowed to disconnect during this command
       AtaXChannelFdoExtension->Flags |= ATAX_DISCONNECT_ALLOWED;

       if ( AtaXChannelFdoExtension->ChannelState == ChannelDisabled )
       {
         if ( !AtaXChannelFdoExtension->AhciInterface ) // AtaXChannelFdoExtension->AhciInterface->Abar ?
         {
           // запрос конфигурационной информации IDE контроллера
           Status = AtaXQueryControllerProperties(AtaXChannelFdo);
           DPRINT("AtaXChannelFdoStartDevice: AtaXQueryControllerProperties return Status - %p\n", Status);

           Status = AtaXQueryBusMasterInterface(AtaXChannelFdo);
           DPRINT("AtaXChannelFdoStartDevice: AtaXQueryBusMasterInterface return Status - %p\n", Status);
         }

         AtaXChannelFdoExtension->ChannelState = ChannelEnabled;
       }

       // Создаем символические ссылки
       Status = AtaXCreateSymLinks(AtaXChannelFdoExtension);
    }
  }

  // Если не подключились к прерыванию ...
  if ( !NT_SUCCESS(Status) )
  {
     AtaXChannelFdoExtension->InterruptObject = 0;
     //AtaXRemoveChannelFdo(AtaXChannelFdoExtension);
     DPRINT("AtaXChannelFdoStartDevice - FIXME AtaXRemoveChannelFdo() \n" );
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
