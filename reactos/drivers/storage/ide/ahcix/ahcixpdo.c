
#include "ahcix.h"

//#define NDEBUG
#include <debug.h>


NTSTATUS
DuplicateUnicodeString(
    IN ULONG Flags,
    IN PCUNICODE_STRING SourceString,
    OUT PUNICODE_STRING DestinationString)
{
  if ( SourceString == NULL || DestinationString == NULL
   ||  SourceString->Length > SourceString->MaximumLength
   || (SourceString->Length == 0 && SourceString->MaximumLength > 0 && SourceString->Buffer == NULL)
   ||  Flags == RTL_DUPLICATE_UNICODE_STRING_ALLOCATE_NULL_STRING || Flags >= 4 )
  {
    return STATUS_INVALID_PARAMETER;
  }

  if ( (SourceString->Length == 0)
    && (Flags != (RTL_DUPLICATE_UNICODE_STRING_NULL_TERMINATE |
                  RTL_DUPLICATE_UNICODE_STRING_ALLOCATE_NULL_STRING)) )
  {
    DestinationString->Length        = 0;
    DestinationString->MaximumLength = 0;
    DestinationString->Buffer        = NULL;
  }
  else
  {
    USHORT DestMaxLength = SourceString->Length;

    if ( Flags & RTL_DUPLICATE_UNICODE_STRING_NULL_TERMINATE )
      DestMaxLength += sizeof(UNICODE_NULL);

    DestinationString->Buffer = ExAllocatePool(PagedPool, DestMaxLength);

    if ( DestinationString->Buffer == NULL )
      return STATUS_NO_MEMORY;

    RtlCopyMemory(DestinationString->Buffer, SourceString->Buffer, SourceString->Length);
    DestinationString->Length = SourceString->Length;
    DestinationString->MaximumLength = DestMaxLength;

    if ( Flags & RTL_DUPLICATE_UNICODE_STRING_NULL_TERMINATE )
      DestinationString->Buffer[DestinationString->Length / sizeof(WCHAR)] = 0;
  }

  return STATUS_SUCCESS;
}

NTSTATUS
AhciXPdoQueryId(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    OUT ULONG_PTR* Information)
{
  PPDO_CHANNEL_EXTENSION     DeviceExtension;
  PFDO_CONTROLLER_EXTENSION  ControllerFdoExtension;
  WCHAR                      Buffer[256];
  ULONG                      Index = 0;
  ULONG                      IdType;
  UNICODE_STRING             SourceString;
  UNICODE_STRING             String;
  NTSTATUS                   Status;

  IdType = IoGetCurrentIrpStackLocation(Irp)->Parameters.QueryId.IdType;
  DeviceExtension = (PPDO_CHANNEL_EXTENSION)DeviceObject->DeviceExtension;
  ControllerFdoExtension = (PFDO_CONTROLLER_EXTENSION)
                           DeviceExtension->ControllerFdo->DeviceExtension;
  
  switch ( IdType )
  {
    case BusQueryDeviceID:
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_ID / BusQueryDeviceID\n");
      RtlInitUnicodeString(&SourceString, L"AHCI\\IDEChannel");
      break;

    case BusQueryHardwareIDs:
    {
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_ID / BusQueryHardwareIDs\n");
    
      switch ( ControllerFdoExtension->VendorId )
      {
        case 0x8086:
        {
          switch (ControllerFdoExtension->DeviceId)
          {
            case 0x2829: //SATA controller [0106]: Intel Corporation 82801HM/HEM (ICH8M/ICH8M-E) SATA Controller [AHCI mode] [8086:2829] (rev 02)
              Index += swprintf(&Buffer[Index], L"Intel-ICH8M") + 1;
              break;

            default:
              Index += swprintf(&Buffer[Index], L"Intel-%04x", ControllerFdoExtension->DeviceId) + 1;
              break;
          }
          break;
        }

        default:
          DPRINT("BusQueryCompatibleIDs / VendorId - %x\n", ControllerFdoExtension->VendorId);
          break;
      }

      if (DeviceExtension->AhciInterface.Channel == 0)
        Index += swprintf(&Buffer[Index], L"Primary_IDE_Channel") + 1;
      else
        Index += swprintf(&Buffer[Index], L"Secondary_IDE_Channel") + 1;

      Index += swprintf(&Buffer[Index], L"*PNP0600") + 1;
      Buffer[Index] = UNICODE_NULL;

      SourceString.Length = SourceString.MaximumLength = Index * sizeof(WCHAR);
      SourceString.Buffer = Buffer;

      break;
    }

    case BusQueryCompatibleIDs:
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_ID / BusQueryCompatibleIDs\n");
      Index += swprintf(&Buffer[Index], L"*PNP0600") + 1;
      Buffer[Index] = UNICODE_NULL;
      SourceString.Length = SourceString.MaximumLength = Index * sizeof(WCHAR);
      SourceString.Buffer = Buffer;
      break;

    case BusQueryInstanceID:
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_ID / BusQueryInstanceID\n");
      swprintf(Buffer, L"%lu", DeviceExtension->AhciInterface.Channel);
      RtlInitUnicodeString(&SourceString, Buffer);
      break;

    default:
      DPRINT1("IRP_MJ_PNP / IRP_MN_QUERY_ID / unknown query id type 0x%lx\n", IdType);
      ASSERT(FALSE);
      return STATUS_NOT_SUPPORTED;
  }
  
  Status = DuplicateUnicodeString(
                    RTL_DUPLICATE_UNICODE_STRING_NULL_TERMINATE,
                    &SourceString,
                    &String);

  *Information = (ULONG_PTR)String.Buffer;
  return Status;
}

NTSTATUS
AhciXPdoQueryDeviceText(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    OUT ULONG_PTR* Information)
{
  PPDO_CHANNEL_EXTENSION  DeviceExtension;
  ULONG                   DeviceTextType;
  PCWSTR                  SourceString;
  UNICODE_STRING          String;
  
  DeviceTextType = IoGetCurrentIrpStackLocation(Irp)->Parameters.QueryDeviceText.DeviceTextType;
  DeviceExtension = (PPDO_CHANNEL_EXTENSION)DeviceObject->DeviceExtension;
  
  switch ( DeviceTextType )
  {
    case DeviceTextDescription:
    case DeviceTextLocationInformation:
    {
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_DEVICE_TEXT / %S\n",
      DeviceTextType == DeviceTextDescription ? L"DeviceTextDescription" : L"DeviceTextLocationInformation");
  
      if (DeviceExtension->AhciInterface.Channel == 0)
        SourceString = L"Primary channel";
      else
        SourceString = L"Secondary channel";
  
      break;
    }

    default:
      DPRINT1("IRP_MJ_PNP / IRP_MN_QUERY_DEVICE_TEXT / unknown type 0x%lx\n", DeviceTextType);
      ASSERT(FALSE);
      return STATUS_NOT_SUPPORTED;
  }
  
  if ( RtlCreateUnicodeString(&String, SourceString) )
  {
    *Information = (ULONG_PTR)String.Buffer;
    return STATUS_SUCCESS;
  }
  else
  {
    return STATUS_INSUFFICIENT_RESOURCES;
  }
}

NTSTATUS
AhciXPdoQueryResourceRequirements(
    IN PPDO_CHANNEL_EXTENSION ChannelPdoExtension,
    IN PIRP Irp,
    OUT ULONG_PTR* Information)
{
  PIO_RESOURCE_REQUIREMENTS_LIST  RequirementsList;
  PIO_RESOURCE_DESCRIPTOR         Descriptor;
  ULONG                           ListSize;
  PFDO_CONTROLLER_EXTENSION       ControllerFdoExtension;

  ControllerFdoExtension = (PFDO_CONTROLLER_EXTENSION)
                           ChannelPdoExtension->ControllerFdo->DeviceExtension;

  // только AHCI режим. FIXME: SATA режим
  ListSize = sizeof(IO_RESOURCE_REQUIREMENTS_LIST);
       //+ 1 * sizeof(IO_RESOURCE_DESCRIPTOR);

  RequirementsList = ExAllocatePool(PagedPool, ListSize);

  if ( !RequirementsList )
    return STATUS_INSUFFICIENT_RESOURCES;

  RtlZeroMemory(RequirementsList, ListSize);

  RequirementsList->ListSize         = ListSize;
  RequirementsList->AlternativeLists = 1;

  RequirementsList->List[0].Version  = 1;
  RequirementsList->List[0].Revision = 1;
  RequirementsList->List[0].Count    = 1; //2

  Descriptor = &RequirementsList->List[0].Descriptors[0];

  /* base */
  //Descriptor->Option           = 0; /* Required */
  //Descriptor->Type             = CmResourceTypeMemory;
  //Descriptor->ShareDisposition = CmResourceShareDeviceExclusive;
  //Descriptor->Flags            = CM_RESOURCE_MEMORY_READ_WRITE;
  //
  //Descriptor->u.Memory.Length    = 0x1100;
  //Descriptor->u.Memory.Alignment = 1;
  //Descriptor->u.Memory.MinimumAddress.QuadPart = (ULONGLONG)Base;
  //Descriptor->u.Memory.MaximumAddress.QuadPart = (ULONGLONG)(Base + 0x1100 - 1);

  //Descriptor++;

  /* Interrupt */
  Descriptor->Option           = 0; /* Required */
  Descriptor->Type             = CmResourceTypeInterrupt;
  Descriptor->ShareDisposition = CmResourceShareShared;
  Descriptor->Flags            = CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;  //(?? CM_RESOURCE_INTERRUPT_LATCHED == CM_RESOURCE_INTERRUPT_LEVEL_LATCHED_BITS);

  Descriptor->u.Interrupt.MinimumVector = ControllerFdoExtension->PciConfig.u.type0.InterruptLine;
  Descriptor->u.Interrupt.MaximumVector = ControllerFdoExtension->PciConfig.u.type0.InterruptLine;

  *Information = (ULONG_PTR)RequirementsList;
  return STATUS_SUCCESS;
}

NTSTATUS
AhciXStartChannel(
    IN PDEVICE_OBJECT ChannelPdo,
    IN PIRP Irp)
{
  PPDO_CHANNEL_EXTENSION     ChannelPdoExtension;
  PFDO_CONTROLLER_EXTENSION  ControllerFdoExtension;
  
  DPRINT("AhciXStartChannel: ChannelPdo - %p, Irp - %p\n", ChannelPdo, Irp);
  
  ChannelPdoExtension = (PPDO_CHANNEL_EXTENSION)ChannelPdo->DeviceExtension;
  ControllerFdoExtension = (PFDO_CONTROLLER_EXTENSION)
                           ChannelPdoExtension->ControllerFdo->DeviceExtension;
  
  if ( ControllerFdoExtension->AhciRegisters )
  {
    DPRINT("AhciXStartChannel: AhciRegisters > 0. return - STATUS_SUCCESS\n");
    return STATUS_SUCCESS;
  }
  
  DPRINT("AhciXStartChannel: return - %x\n", STATUS_SUCCESS);
  return STATUS_SUCCESS;
}

NTSTATUS NTAPI   //  PAHCI_START_IO  AhciStartIo;
AhciSendCommand(
    IN PPDO_CHANNEL_EXTENSION ChannelPdoExtension,
    IN PIDENTIFY_DATA IdentifyData,
    IN PSCSI_REQUEST_BLOCK  Srb)
{
  PFDO_CONTROLLER_EXTENSION  ControllerFdoExtension;
  AHCI_DEVICE_TYPE           DeviceType;
  PAHCI_PORT_REGISTERS       Port;
  PAHCI_COMMAND_HEADER       CommandHeader;
  PAHCI_COMMAND_TABLE        CommandTable;
  PHYSICAL_ADDRESS           PhCommandTable;
  PHYSICAL_ADDRESS           PhDataBaseAddress;
  PFIS_REGISTER_H2D          CommandFIS; // 0 Slot
  ULONG                      InterruptStatus;
  ULONG                      InterruptEnable;
  ULONG                      AhciChannel;
  BOOLEAN                    Write;
  ULONG                      StartingSector;
  USHORT                     SectorsPerTrack, Heads;
  PCDB                       AtapiCommand;
  ULONG                      Slot = 0;
  NTSTATUS                   Status;

  DPRINT("AhciSendCommand: ChannelPdoExtension - %p, Srb - %p\n", ChannelPdoExtension, Srb);
  DPRINT("AhciSendCommand: Command %x to device %d\n", Srb->Cdb[0], Srb->TargetId);

  ControllerFdoExtension = (PFDO_CONTROLLER_EXTENSION)ChannelPdoExtension->ControllerFdo->DeviceExtension;
  DPRINT("AhciSendCommand: ControllerFdoExtension - %p\n", ControllerFdoExtension);

  AhciChannel = ChannelPdoExtension->AhciInterface.AhciChannel;  // физический канал (0 ... 31)
  DPRINT("AhciSendCommand: AhciChannel - %d\n", AhciChannel);

  DeviceType = ControllerFdoExtension->DeviceType[AhciChannel];
  DPRINT("AhciSendCommand: DeviceType - %x\n", DeviceType);

  if ( DeviceType == AtaDrive )
  {
    DPRINT("AhciSendCommand: ATA device\n");

    switch ( Srb->Cdb[0] )
    {
      case SCSIOP_READ:              /* 0x28 */
      case SCSIOP_WRITE:             /* 0x2A */
        DPRINT("AhciSendCommand / SCSIOP_READ or SCSIOP_WRITE\n");
        break;

      case SCSIOP_TEST_UNIT_READY:
        DPRINT("AhciSendCommand / SCSIOP_TEST_UNIT_READY FIXME\n");
        Status = SRB_STATUS_INVALID_REQUEST;
        return Status;

      default:
        DPRINT("AhciSendCommand: Unsupported command FIXME%x\n", Srb->Cdb[0]);
        Status = SRB_STATUS_INVALID_REQUEST;
        return Status;
    }
    
    SectorsPerTrack = IdentifyData->NumSectorsPerTrack;
    Heads           = IdentifyData->NumHeads;

    // начальный сектор извлекаем из CDB
    StartingSector = ((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte3       |
                     ((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte2 << 8  |
                     ((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte1 << 16 |
                     ((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte0 << 24;

    DPRINT("AhciSendCommand: Starting sector is %x, Number of bytes %x\n", StartingSector, Srb->DataTransferLength);
    DPRINT("AhciSendCommand: Cylinder %x Head %x Sector %x\n",
            StartingSector  / (SectorsPerTrack  * Heads),
            (StartingSector /  SectorsPerTrack) % Heads,
            StartingSector  %  SectorsPerTrack + 1);
  }
  else if ( DeviceType == AtapiDrive )
  {
    /*
    DPRINT("AhciAtapiSendCommand: Srb->Cdb[0]  - %x\n", Srb->Cdb[0]);
    DPRINT("AhciAtapiSendCommand: Srb->Cdb[1]  - %x\n", Srb->Cdb[1]);
    DPRINT("AhciAtapiSendCommand: Srb->Cdb[2]  - %x\n", Srb->Cdb[2]);
    DPRINT("AhciAtapiSendCommand: Srb->Cdb[3]  - %x\n", Srb->Cdb[3]);
    DPRINT("AhciAtapiSendCommand: Srb->Cdb[4]  - %x\n", Srb->Cdb[4]);
    DPRINT("AhciAtapiSendCommand: Srb->Cdb[5]  - %x\n", Srb->Cdb[5]);
    DPRINT("AhciAtapiSendCommand: Srb->Cdb[6]  - %x\n", Srb->Cdb[6]);
    DPRINT("AhciAtapiSendCommand: Srb->Cdb[7]  - %x\n", Srb->Cdb[7]);
    DPRINT("AhciAtapiSendCommand: Srb->Cdb[8]  - %x\n", Srb->Cdb[8]);
    DPRINT("AhciAtapiSendCommand: Srb->Cdb[9]  - %x\n", Srb->Cdb[9]);
    DPRINT("AhciAtapiSendCommand: Srb->Cdb[10] - %x\n", Srb->Cdb[10]);
    DPRINT("AhciAtapiSendCommand: Srb->Cdb[11] - %x\n", Srb->Cdb[11]);
    DPRINT("AhciAtapiSendCommand: Srb->Cdb[12] - %x\n", Srb->Cdb[12]);
    DPRINT("AhciAtapiSendCommand: Srb->Cdb[13] - %x\n", Srb->Cdb[13]);
    DPRINT("AhciAtapiSendCommand: Srb->Cdb[14] - %x\n", Srb->Cdb[14]);
    DPRINT("AhciAtapiSendCommand: Srb->Cdb[15] - %x\n", Srb->Cdb[15]);
    */

    DPRINT("AhciSendCommand: ATAPI device\n");
  }

  if ( Srb->SrbFlags & SRB_FLAGS_DATA_IN )
    Write = 0;
  else if ( Srb->SrbFlags & SRB_FLAGS_DATA_OUT )
    Write = 1;
  else
    return STATUS_UNSUCCESSFUL;

  Port = ChannelPdoExtension->AhciInterface.AhciPortControl;
  //DPRINT("AhciSendCommand: Port - %p\n", Port);

  InterruptStatus = READ_REGISTER_ULONG((PULONG)&Port->InterruptStatus);
  DPRINT("AhciSendCommand: InterruptStatus - %p\n", InterruptStatus);
  WRITE_REGISTER_ULONG((PULONG)&Port->InterruptStatus, 0xFFFFFFFF);  // Clear pending interrupt bits

  InterruptEnable = READ_REGISTER_ULONG((PULONG)&Port->InterruptEnable);
  //DPRINT("AhciSendCommand: Port->InterruptEnable - %p\n", InterruptEnable);

  InterruptEnable |= 1;
  WRITE_REGISTER_ULONG((PULONG)&Port->InterruptEnable, InterruptEnable);
  DPRINT("AhciSendCommand: Port->InterruptEnable - %p\n", READ_REGISTER_ULONG((PULONG)&Port->InterruptEnable));

  CommandHeader = (PAHCI_COMMAND_HEADER)(
                  (ULONG_PTR)ChannelPdoExtension->AhciInterface.CmdListBaseAddress +
                  Slot * sizeof(AHCI_COMMAND_HEADER));

  //DPRINT("AhciSendCommand: CommandHeader         - %p\n", CommandHeader);

  CommandTable = (PAHCI_COMMAND_TABLE)
                 ChannelPdoExtension->AhciInterface.AhciBuffer +
                 Slot * sizeof(AHCI_COMMAND_TABLE);

  PhCommandTable = MmGetPhysicalAddress((PVOID)CommandTable);

  CommandHeader->DescriptionInformation.CommandFISLength = sizeof(FIS_REGISTER_H2D)/sizeof(ULONG);  // Command FIS size
  CommandHeader->DescriptionInformation.Write            = Write;                                   // Write or Read
  CommandHeader->DescriptionInformation.PRDTLength       = 1;                                       // 1 PRD (Physical Region Descriptor)

  if ( DeviceType == AtaDrive )
    CommandHeader->DescriptionInformation.ATAPI = 0;
  else if ( DeviceType == AtapiDrive )
    CommandHeader->DescriptionInformation.ATAPI = 1;

  CommandHeader->PRDTByteCount           = Srb->DataTransferLength;
  CommandHeader->CmdTableDescriptorBase  = (PAHCI_COMMAND_TABLE)PhCommandTable.LowPart;
  CommandHeader->CmdTableDescriptorBaseU = PhCommandTable.HighPart;

  PhDataBaseAddress = MmGetPhysicalAddress((PVOID)ChannelPdoExtension->AhciInterface.DmaBuffer);

  CommandTable->PRDTable.Descriptor[0].DataBaseAddress       = PhDataBaseAddress.LowPart;
  CommandTable->PRDTable.Descriptor[0].DataBaseAddressU      = PhDataBaseAddress.HighPart;
  CommandTable->PRDTable.Descriptor[0].DataByteCount         = Srb->DataTransferLength;
  CommandTable->PRDTable.Descriptor[0].InterruptOnCompletion = 1;

  DPRINT("AhciSendCommand: Srb->DataBuffer         - %p\n", Srb->DataBuffer);
  DPRINT("AhciSendCommand: Srb->DataTransferLength - %p\n", Srb->DataTransferLength);

  if ( Write )
    RtlMoveMemory((PVOID)ChannelPdoExtension->AhciInterface.DmaBuffer,
                  (PVOID)Srb->DataBuffer, Srb->DataTransferLength);
  else
    RtlZeroMemory((PVOID)ChannelPdoExtension->AhciInterface.DmaBuffer,
                  Srb->DataTransferLength);

  // Setup CommandFIS
  CommandFIS = ChannelPdoExtension->AhciInterface.CommandFIS;
  RtlZeroMemory(CommandFIS, sizeof(FIS_REGISTER_H2D));
  //ChannelPdoExtension->AhciInterface.CommandFIS = CommandFIS;

  CommandFIS->FISType      = FIS_TYPE_REGISTER_H2D;
  CommandFIS->Device       = 0;		                // Master device
  CommandFIS->RegisterType = 1;		                // Write command register

  if ( DeviceType == AtaDrive )
  {
    if ( Write )
    {
      CommandFIS->Command = IDE_COMMAND_WRITE_DMA;  // 0xCA
    }
    else
    {
      CommandFIS->Command = IDE_COMMAND_READ_DMA;  // 0xC8
    }

    CommandFIS->LBA0      = (UCHAR)((StartingSector % SectorsPerTrack) + 1);             // устанавливаем регистр LowLBA
    CommandFIS->LBA1      = (UCHAR)(StartingSector  / (SectorsPerTrack * Heads));        // устанавливаем регистр MidLBA
    CommandFIS->LBA2      = (UCHAR)((StartingSector / (SectorsPerTrack * Heads)) >> 8);  // устанавливаем регистр HighLBA
    CommandFIS->Device    = (UCHAR)((StartingSector / SectorsPerTrack) % Heads);         // устанавливаем регистр DriveSelect
    CommandFIS->CountLow  = (UCHAR)((Srb->DataTransferLength + 0x1FF) / 0x200);          // устанавливаем регистр SectorCount
    CommandFIS->CountHigh = 0;
  }
  else if ( DeviceType == AtapiDrive )
  {
    AtapiCommand = (PCDB)&CommandTable->AtapiCommand;     //UCHAR  AtapiCommand[0x10]
    RtlCopyMemory(AtapiCommand, (PVOID)Srb->Cdb, 0x10);   // FIXME: size CDB

    CommandFIS->Command = IDE_COMMAND_ATAPI_PACKET;       // 0xA0
    CommandFIS->FeaturesLow  = 1;                         // 1 - DMA, 0 - PIO

    // устанавливаем количество байт пересылки в соотвктствующие регистры
    CommandFIS->LBA0      = 0;                                                           // устанавливаем регистр LowLBA
    CommandFIS->LBA1      = (UCHAR)(Srb->DataTransferLength & 0xFF);                     // устанавливаем регистр MidLBA
    CommandFIS->LBA2      = (UCHAR)(Srb->DataTransferLength >> 8);                       // устанавливаем регистр HighLBA

                                                          //if ( Srb->DataTransferLength >= 0x10000 )
                                                          //  ByteCountLow = ByteCountHigh = 0xFF;
    CommandFIS->CountLow  = 0;  //(UCHAR)(ByteCountLow);
    CommandFIS->CountHigh = 0;  //(UCHAR)(ByteCountHigh);
  }

  /*
  DPRINT(" ... : CommandFIS                     - %p\n", CommandFIS);
  DPRINT(" ... : CommandFIS->PortMultiplierPort - %x\n", CommandFIS->PortMultiplierPort);
  DPRINT(" ... : CommandFIS->RegisterType       - %x\n", CommandFIS->RegisterType);
  DPRINT(" ... : CommandFIS->Command            - %x\n", CommandFIS->Command);
  DPRINT(" ... : CommandFIS->LBA0               - %x\n", CommandFIS->LBA0);
  DPRINT(" ... : CommandFIS->LBA1               - %x\n", CommandFIS->LBA1);
  DPRINT(" ... : CommandFIS->LBA2               - %x\n", CommandFIS->LBA2);
  DPRINT(" ... : CommandFIS->Device             - %x\n", CommandFIS->Device);
  DPRINT(" ... : CommandFIS->LBA3               - %x\n", CommandFIS->LBA3);
  DPRINT(" ... : CommandFIS->LBA4               - %x\n", CommandFIS->LBA4);
  DPRINT(" ... : CommandFIS->LBA5               - %x\n", CommandFIS->LBA5);
  DPRINT(" ... : CommandFIS->FeaturesLow        - %x\n", CommandFIS->FeaturesLow);
  DPRINT(" ... : CommandFIS->FeaturesHigh       - %x\n", CommandFIS->FeaturesHigh);
  DPRINT(" ... : CommandFIS->CountLow           - %x\n", CommandFIS->CountLow);
  DPRINT(" ... : CommandFIS->CountHigh          - %x\n", CommandFIS->CountHigh);
  DPRINT(" ... : CommandFIS->Control            - %x\n", CommandFIS->Control);
  */

  Port->CommandIssue = 1 << Slot;  // Issue command

  // ждём аппаратное прерывание
  Status = STATUS_SUCCESS;
  DPRINT("AhciSendCommand: return - %p\n", Status);
  return Status;
}

NTSTATUS
AhciXPdoPnpDispatch(
    IN PDEVICE_OBJECT ChannelPdo,
    IN PIRP Irp)
{
  PPDO_CHANNEL_EXTENSION     ChannelPdoExtension;
  PFDO_CONTROLLER_EXTENSION  ControllerFdoExtension;
  ULONG_PTR                  Information;
  PIO_STACK_LOCATION         Stack;
  ULONG                      MinorFunction;
  NTSTATUS                   Status;

  ChannelPdoExtension = (PPDO_CHANNEL_EXTENSION)ChannelPdo->DeviceExtension;
  ControllerFdoExtension = (PFDO_CONTROLLER_EXTENSION)ChannelPdoExtension->ControllerFdo->DeviceExtension;

  Information = Irp->IoStatus.Information;
  Stack = IoGetCurrentIrpStackLocation(Irp);
  MinorFunction = Stack->MinorFunction;

  switch ( MinorFunction )
  {
    /* FIXME:
     * Those are required:
     *    IRP_MN_START_DEVICE (done)
     *    IRP_MN_QUERY_STOP_DEVICE
     *    IRP_MN_STOP_DEVICE
     *    IRP_MN_CANCEL_STOP_DEVICE
     *    IRP_MN_QUERY_REMOVE_DEVICE
     *    IRP_MN_REMOVE_DEVICE
     *    IRP_MN_CANCEL_REMOVE_DEVICE
     *    IRP_MN_SURPRISE_REMOVAL
     *    IRP_MN_QUERY_CAPABILITIES (done)
     *    IRP_MN_QUERY_DEVICE_RELATIONS / TargetDeviceRelations (done)
     *    IRP_MN_QUERY_ID / BusQueryDeviceID (done)
     * Those may be required/optional:
     *    IRP_MN_DEVICE_USAGE_NOTIFICATION
     *    IRP_MN_QUERY_RESOURCES
     *    IRP_MN_QUERY_RESOURCE_REQUIREMENTS (done)
     *    IRP_MN_QUERY_DEVICE_TEXT
     *    IRP_MN_QUERY_BUS_INFORMATION
     *    IRP_MN_QUERY_INTERFACE
     *    IRP_MN_READ_CONFIG
     *    IRP_MN_WRITE_CONFIG
     *    IRP_MN_EJECT
     *    IRP_MN_SET_LOCK
     * Those are optional:
     *    IRP_MN_QUERY_DEVICE_RELATIONS / EjectionRelations
     *    IRP_MN_QUERY_ID / BusQueryHardwareIDs (done)
     *    IRP_MN_QUERY_ID / BusQueryCompatibleIDs (done)
     *    IRP_MN_QUERY_ID / BusQueryInstanceID (done)
     */
    case IRP_MN_START_DEVICE:                  /* 0x00 */
      DPRINT("IRP_MJ_PNP / IRP_MN_START_DEVICE\n");
      Status = AhciXStartChannel(ChannelPdo, Irp);
      break;

    case IRP_MN_QUERY_REMOVE_DEVICE:           /* 0x01 */
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_REMOVE_DEVICE\n");
      Status = STATUS_UNSUCCESSFUL;
      break;

    case IRP_MN_QUERY_DEVICE_RELATIONS:        /* 0x07 */
ASSERT(FALSE);
      break;

    case IRP_MN_QUERY_INTERFACE:               /* 0x08 */
    {
      // It get pseudo Interface. QueryInterface.Size - type Interface. QueryInterface.Interface - pointer on Interface.
      if ( IsEqualGUIDAligned(Stack->Parameters.QueryInterface.InterfaceType, &GUID_BUS_INTERFACE_STANDARD) )
      {
        switch ( Stack->Parameters.QueryInterface.Size )
        {
          case 1:  // QueryControllerProperties
            DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_INTERFACE / QueryControllerProperties. NOT_SUPPORTED\n");
            Status = STATUS_NOT_SUPPORTED;
            break;

          case 3:  // QueryPciBusInterface
            DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_INTERFACE / QueryPciBusInterface\n");
            *(PVOID *)Stack->Parameters.QueryInterface.Interface = ControllerFdoExtension->BusInterface;
            DPRINT("AhciXPdoPnpDispatch: BusInterface - %p\n", ControllerFdoExtension->BusInterface);
            Status = STATUS_SUCCESS;
            break;

          case 5:  // QueryBusMasterInterface
            DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_INTERFACE / QueryBusMasterInterface. NOT_SUPPORTED\n");
            Status = STATUS_NOT_SUPPORTED;
            break;

          case 7:  // QueryAhciInterface
            DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_INTERFACE / QueryAhciInterface\n");
            if ( ControllerFdoExtension->AhciRegisters )
            {
              ChannelPdoExtension->AhciInterface.AhciStartIo   = (PAHCI_START_IO)AhciSendCommand;
              ChannelPdoExtension->AhciInterface.AhciInterrupt = (PAHCI_INTERRUPT)AhciInterruptRoutine;

              *(PVOID *)Stack->Parameters.QueryInterface.Interface = &ChannelPdoExtension->AhciInterface;
              Status = STATUS_SUCCESS;
            }
            else
            {
              Status = STATUS_NOT_IMPLEMENTED;
            }
            break;

          case 9: //QuerySataInterface
            DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_INTERFACE / QuerySataInterface. NOT_SUPPORTED\n");
            Status = STATUS_NOT_SUPPORTED;
            break;

          default:
            DPRINT1("IRP_MJ_PNP / IRP_MN_QUERY_INTERFACE / Unknown Size FIXME\n");
            Status = STATUS_NOT_SUPPORTED;
            break;
        }
        break;
      }
      else
      {
        DPRINT1("IRP_MJ_PNP / IRP_MN_QUERY_INTERFACE / Unknown type\n");
        ASSERT(FALSE);
        Status = STATUS_NOT_SUPPORTED;
        break;
      }
    }

    case IRP_MN_QUERY_CAPABILITIES:            /* 0x09 */
    {
      PDEVICE_CAPABILITIES  DeviceCapabilities;
      ULONG                 ix;

      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_CAPABILITIES\n");
      DeviceCapabilities = (PDEVICE_CAPABILITIES)Stack->Parameters.DeviceCapabilities.Capabilities;

      /* FIXME: capabilities can change with connected device */
      DeviceCapabilities->LockSupported     = FALSE;
      DeviceCapabilities->EjectSupported    = FALSE;
      DeviceCapabilities->Removable         = TRUE;
      DeviceCapabilities->DockDevice        = FALSE;
      DeviceCapabilities->UniqueID          = FALSE;
      DeviceCapabilities->SilentInstall     = FALSE;
      DeviceCapabilities->RawDeviceOK       = FALSE;
      DeviceCapabilities->SurpriseRemovalOK = TRUE;
      DeviceCapabilities->HardwareDisabled  = FALSE;          /* FIXME */
      //DeviceCapabilities->NoDisplayInUI   = FALSE;          /* FIXME */
      DeviceCapabilities->DeviceState[0]    = PowerDeviceD0;  /* FIXME */

      for ( ix = 0; ix < PowerSystemMaximum; ix++ )
        DeviceCapabilities->DeviceState[ix] = PowerDeviceD3;  /* FIXME */

      //DeviceCapabilities->DeviceWake = PowerDeviceUndefined; /* FIXME */
      DeviceCapabilities->D1Latency = 0; /* FIXME */
      DeviceCapabilities->D2Latency = 0; /* FIXME */
      DeviceCapabilities->D3Latency = 0; /* FIXME */

      Status = STATUS_SUCCESS;
      break;
    }

    case IRP_MN_QUERY_RESOURCES:               /* 0x0a */
      /* This IRP is optional; do nothing */
      Information = Irp->IoStatus.Information;
      Status = Irp->IoStatus.Status;
      break;

    case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:   /* 0x0b */
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_RESOURCE_REQUIREMENTS\n");
      Status = AhciXPdoQueryResourceRequirements(ChannelPdoExtension, Irp, &Information);
      break;

    case IRP_MN_QUERY_DEVICE_TEXT:             /* 0x0c */
      Status = AhciXPdoQueryDeviceText(ChannelPdo, Irp, &Information);
      break;

    case IRP_MN_FILTER_RESOURCE_REQUIREMENTS:  /* 0x0d */
      DPRINT("IRP_MJ_PNP / IRP_MN_FILTER_RESOURCE_REQUIREMENTS\n");
      Information = Irp->IoStatus.Information;
      Status = Irp->IoStatus.Status;
      break;

    case IRP_MN_QUERY_ID:                      /* 0x13 */
      Status = AhciXPdoQueryId(ChannelPdo, Irp, &Information);
      break;

    case IRP_MN_QUERY_PNP_DEVICE_STATE:        /* 0x14 */
       DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_PNP_DEVICE_STATE\n");
       Information |= PNP_DEVICE_NOT_DISABLEABLE;
       Status = STATUS_SUCCESS;
       break;

    case IRP_MN_QUERY_BUS_INFORMATION:         /* 0x15 */
    {
      PPNP_BUS_INFORMATION BusInfo;

      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_BUS_INFORMATION\n");
      BusInfo = (PPNP_BUS_INFORMATION)
                ExAllocatePool(PagedPool, sizeof(PNP_BUS_INFORMATION));

      if ( !BusInfo )
      {
        Status = STATUS_INSUFFICIENT_RESOURCES;
      }
      else
      {
        BusInfo->LegacyBusType = PNPBus;
        BusInfo->BusNumber = 0; /* FIXME */
        Information = (ULONG_PTR)BusInfo;
        Status = STATUS_SUCCESS;
      }
      break;
    }

    default:
      // We can't forward request to the lower driver, because we are a Pdo, so we don't have lower driver...
      DPRINT1("IRP_MJ_PNP / Unknown minor function 0x%lx\n", MinorFunction);
      ASSERT(FALSE);
      Information = Irp->IoStatus.Information;
      Status = Irp->IoStatus.Status;
      break;
  }

  Irp->IoStatus.Information = Information;
  Irp->IoStatus.Status = Status;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return Status;
}
