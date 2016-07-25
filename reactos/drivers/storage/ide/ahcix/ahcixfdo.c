
#include "ahcix.h"

#define NDEBUG
#include <debug.h>


VOID
AhciXPortStop(IN PAHCI_PORT_REGISTERS Port)
{
  AHCI_PORT_COMMAND  Command;

  // Stop command engine 

  Command.AsULONG = READ_REGISTER_ULONG((PULONG)&Port->Command);
  //DPRINT("AhciXPortStop: Command - %p\n", Command);
  Command.Start = 0;
  WRITE_REGISTER_ULONG((PULONG)&Port->Command, Command.AsULONG);

  while( 1 )
  {
    Command.AsULONG = READ_REGISTER_ULONG((PULONG)&Port->Command);
    if ( Command.CmdListRunning )
      continue;

    break;
  }

  Command.AsULONG = READ_REGISTER_ULONG((PULONG)&Port->Command);
  Command.FISReceiveEnable = 0;
  WRITE_REGISTER_ULONG((PULONG)&Port->Command, Command.AsULONG);

  while( 1 )
  {
    Command.AsULONG = READ_REGISTER_ULONG((PULONG)&Port->Command);
    if ( Command.FISReceiveRunning )
      continue;

    break;
  }

  Command.AsULONG = READ_REGISTER_ULONG((PULONG)&Port->Command);
  //DPRINT("AhciXPortStop: Command - %p\n", Command);
}

VOID
AhciXPortStart(IN PAHCI_PORT_REGISTERS Port)
{
  AHCI_PORT_COMMAND  Command;

  // Start command engine
  // FIXME: Software must wait for CLO (CmdListOverride) to be cleared to С0Т
  // before setting PxCMD.ST (Start) to С1Т.

  Command.AsULONG = READ_REGISTER_ULONG((PULONG)&Port->Command);
  //DPRINT("AhciXPortStart: Command - %p\n", Command);

  while( 1 )
  {
    Command.AsULONG = READ_REGISTER_ULONG((PULONG)&Port->Command);
    if ( Command.CmdListRunning )
      continue;
    else
      break;
  }

  Command.FISReceiveEnable = 1;
  Command.Start            = 1;

  WRITE_REGISTER_ULONG((PULONG)&Port->Command, Command.AsULONG);
  //DPRINT("AhciXPortStart: Command - %p\n", READ_REGISTER_ULONG((PULONG)&Port->Command));
}

NTSTATUS
AhciXChannelInit(
    IN PPDO_CHANNEL_EXTENSION ChannelPdoExtension,
    IN ULONG ChannelNumber)
{
  PFDO_CONTROLLER_EXTENSION  ControllerFdoExtension;
  PAHCI_MEMORY_REGISTERS     Abar;
  AHCI_HOST_GLOBAL_CONTROL   GlobalControl;
  ULONG                      InterruptStatus;
  PAHCI_PORT_REGISTERS       Port;
  ULONG                      DmaBuffer;
  ULONG                      AhciBuffer;
  ULONG                      AhciBufferLength;
  PHYSICAL_ADDRESS           HighestAddress = {{0xFFFFFFFF, 0}};
  PAHCI_COMMAND_HEADER       AhciCommandHeader;
  PAHCI_COMMAND_LIST         CmdListBaseAddress;
  PHYSICAL_ADDRESS           PhCmdListBaseAddress;
  PAHCI_RECEIVED_FIS         FISBaseAddress;
  PHYSICAL_ADDRESS           PhFISBaseAddress;
  PAHCI_COMMAND_TABLE        AhciCommandTable;
  PHYSICAL_ADDRESS           PhAhciCommandTable;
  NTSTATUS                   Status=0;
  ULONG                      ix;

  DPRINT("AhciXChannelInit: ChannelPdoExtension - %p, ChannelNumber - %x\n", ChannelPdoExtension, ChannelNumber);

  ControllerFdoExtension = (PFDO_CONTROLLER_EXTENSION)ChannelPdoExtension->ControllerFdo->DeviceExtension;
  Abar = ControllerFdoExtension->AhciRegisters;
  GlobalControl.AsULONG = READ_REGISTER_ULONG((PULONG)&Abar->GlobalHostControl);
  InterruptStatus = READ_REGISTER_ULONG((PULONG)&Abar->InterruptStatus);

  Port = (PAHCI_PORT_REGISTERS)&Abar->PortControl[ChannelNumber];
  ChannelPdoExtension->AhciInterface.AhciPortControl = Port;

  DPRINT("AhciXChannelInit: Abar                          - %p\n", Abar);
  DPRINT("AhciXChannelInit: GlobalControl                 - %p\n", GlobalControl);
  DPRINT("AhciXChannelInit: GlobalControl.InterruptEnable - %d\n", GlobalControl.InterruptEnable);
  DPRINT("AhciXChannelInit: InterruptStatus               - %p\n", InterruptStatus);
  DPRINT("AhciXChannelInit: Port                          - %p\n", Port);

  // FIXME 
  AhciBufferLength = 32 * sizeof(AHCI_COMMAND_TABLE) +  // 0x5000   // aligned to a 128-byte cache line (0x0080)
                          sizeof(AHCI_COMMAND_LIST)  +  // 0x0400   // 1024 byte - aligned (0x0400)
                          sizeof(AHCI_RECEIVED_FIS)  +  // 0x0100   // 256 byte - aligned  (0x0100)
                          sizeof(IDENTIFY_DATA);        // 0x0200

  DPRINT("AhciXChannelInit: sizeof(AHCI_PORT_REGISTERS) - 0x%x\n", sizeof(AHCI_PORT_REGISTERS));
  DPRINT("AhciXChannelInit: sizeof(AHCI_RECEIVED_FIS)   - 0x%x\n", sizeof(AHCI_RECEIVED_FIS));
  DPRINT("AhciXChannelInit: sizeof(AHCI_COMMAND_LIST)   - 0x%x\n", sizeof(AHCI_COMMAND_LIST));
  DPRINT("AhciXChannelInit: sizeof(AHCI_COMMAND_HEADER) - 0x%x\n", sizeof(AHCI_COMMAND_HEADER));
  DPRINT("AhciXChannelInit: sizeof(AHCI_COMMAND_TABLE)  - 0x%x\n", sizeof(AHCI_COMMAND_TABLE));

  /*ChannelPdoExtension->AhciInterface.AhciBuffer =
              MmAllocateContiguousMemorySpecifyCache(
                        AhciBufferLength
                        IN PHYSICAL_ADDRESS  LowestAcceptableAddress,
                        IN PHYSICAL_ADDRESS  HighestAcceptableAddress,
                        IN PHYSICAL_ADDRESS  BoundaryAddressMultiple  OPTIONAL,
                        MmNonCached); */

  AhciBuffer = (ULONG_PTR)MmAllocateContiguousMemory(AhciBufferLength, HighestAddress);

  if ( !AhciBuffer )
  {
    DPRINT1("AhciXChannelInit: Not Created AhciBuffer!\n");
    Status = STATUS_INSUFFICIENT_RESOURCES;
    return Status;
  }

  RtlZeroMemory((PVOID)AhciBuffer, AhciBufferLength);
  ChannelPdoExtension->AhciInterface.AhciBuffer = AhciBuffer;
  DPRINT("AhciXChannelInit: AhciBuffer       - %p\n", AhciBuffer);
  DPRINT("AhciXChannelInit: AhciBufferLength - %x\n", AhciBufferLength);

  DmaBuffer = (ULONG_PTR)MmAllocateContiguousMemory(0x20000, HighestAddress);   // 128 Kb - Maximum byte transfer buffer

  if ( !DmaBuffer )
  {
    DPRINT1("AhciXChannelInit: Not Created DmaBuffer!\n");
    Status = STATUS_INSUFFICIENT_RESOURCES;
    return Status;
  }

  RtlZeroMemory((PVOID)DmaBuffer, 0x20000);
  ChannelPdoExtension->AhciInterface.DmaBuffer = DmaBuffer;
  DPRINT("AhciXChannelInit: DmaBuffer - %p\n", DmaBuffer);

  AhciXPortStop(Port);

  // 0x00 PxCLB  Command List Base Address, 1024 byte - aligned
  CmdListBaseAddress = (PAHCI_COMMAND_LIST)(AhciBuffer + 32 * sizeof(AHCI_COMMAND_TABLE));
  ChannelPdoExtension->AhciInterface.CmdListBaseAddress = CmdListBaseAddress;

  PhCmdListBaseAddress = MmGetPhysicalAddress(CmdListBaseAddress);
  ChannelPdoExtension->AhciInterface.PhCmdListBaseAddress = PhCmdListBaseAddress;

  WRITE_REGISTER_ULONG((PULONG)&Port->CmdListBaseAddress,
                       PhCmdListBaseAddress.LowPart);

  WRITE_REGISTER_ULONG((PULONG)&Port->CmdListBaseAddressUpper,
                       PhCmdListBaseAddress.HighPart);

  // 0x08, PxFB  256 byte - aligned
  FISBaseAddress = (PAHCI_RECEIVED_FIS)(AhciBuffer +
                   32 * sizeof(AHCI_COMMAND_TABLE) +
                   sizeof(AHCI_COMMAND_LIST));

  ChannelPdoExtension->AhciInterface.FISBaseAddress = FISBaseAddress;

  PhFISBaseAddress = MmGetPhysicalAddress(FISBaseAddress);
  ChannelPdoExtension->AhciInterface.PhFISBaseAddress = PhFISBaseAddress;

  WRITE_REGISTER_ULONG((PULONG)&Port->FISBaseAddress,
                       PhFISBaseAddress.LowPart);

  WRITE_REGISTER_ULONG((PULONG)&Port->FISBaseAddressUpper,
                       PhFISBaseAddress.HighPart);

  DPRINT("AhciXChannelInit: AhciBuffer                    - %p\n", AhciBuffer);
  DPRINT("AhciXChannelInit: CmdListBaseAddress            - %p\n", CmdListBaseAddress);
  DPRINT("AhciXChannelInit: FISBaseAddress                - %p\n", FISBaseAddress);

  DPRINT("AhciXChannelInit: Port->CmdListBaseAddress      - %p\n", READ_REGISTER_ULONG((PULONG)&Port->CmdListBaseAddress));
  DPRINT("AhciXChannelInit: Port->CmdListBaseAddressUpper - %p\n", READ_REGISTER_ULONG((PULONG)&Port->CmdListBaseAddressUpper));
  DPRINT("AhciXChannelInit: Port->FISBaseAddress          - %p\n", READ_REGISTER_ULONG((PULONG)&Port->FISBaseAddress));
  DPRINT("AhciXChannelInit: Port->FISBaseAddressUpper     - %p\n", READ_REGISTER_ULONG((PULONG)&Port->FISBaseAddressUpper));

  for ( ix = 0; ix < 32; ix++ )
  {
    AhciCommandHeader = (PAHCI_COMMAND_HEADER)(
                        (ULONG_PTR)CmdListBaseAddress +
                        ix * sizeof(AHCI_COMMAND_HEADER));

    AhciCommandTable = (PAHCI_COMMAND_TABLE)(
                        AhciBuffer + 
                        ix * sizeof(AHCI_COMMAND_TABLE));

    PhAhciCommandTable = MmGetPhysicalAddress(AhciCommandTable);

    AhciCommandHeader->CmdTableDescriptorBase = (PAHCI_COMMAND_TABLE)
                                                PhAhciCommandTable.LowPart;

    AhciCommandHeader->CmdTableDescriptorBaseU = PhAhciCommandTable.HighPart;

    if ( ix == 0 )  // 0 slot - Command slot
    {
      // Setup Command Fis
      ChannelPdoExtension->AhciInterface.CommandFIS = (PFIS_REGISTER_H2D)
                                                      (&AhciCommandTable->CommandFIS);

      DPRINT("AhciXChannelInit: ChannelPdoExtension->AhciInterface.CommandFIS - %p\n", ChannelPdoExtension->AhciInterface.CommandFIS);
    }

    DPRINT("AhciXChannelInit: AhciCommandHeader[%d]                      - %p\n", ix, AhciCommandHeader);
    DPRINT("AhciXChannelInit: AhciCommandTable[%d]                       - %p\n", ix, AhciCommandTable);
    DPRINT("AhciXChannelInit: AhciCommandHeader->CmdTableDescriptorBase  - %p\n", AhciCommandHeader->CmdTableDescriptorBase);
    DPRINT("AhciXChannelInit: AhciCommandHeader->CmdTableDescriptorBaseU - %p\n", AhciCommandHeader->CmdTableDescriptorBaseU);
  }

  AhciXPortStart(Port);

  GlobalControl.InterruptEnable = 1;
  WRITE_REGISTER_ULONG((PULONG)&Abar->GlobalHostControl, GlobalControl.AsULONG);
  GlobalControl.AsULONG = READ_REGISTER_ULONG((PULONG)&Abar->GlobalHostControl);

  DPRINT("AhciXChannelInit: GlobalControl                 - %p\n", GlobalControl);
  DPRINT("AhciXChannelInit: GlobalControl.InterruptEnable - %d\n", GlobalControl.InterruptEnable);

  InterruptStatus |= 1 << ChannelNumber;
  WRITE_REGISTER_ULONG((PULONG)&Abar->InterruptStatus, InterruptStatus);

  DPRINT("AhciXChannelInit: InterruptStatus   - %p\n", READ_REGISTER_ULONG((PULONG)&Abar->InterruptStatus));
  DPRINT("\n");

  /*
  DPRINT("AhciXChannelInit: ChannelPdoExtension->AhciRegisters        - %p\n", ChannelPdoExtension->AhciRegisters);
  DPRINT("AhciXChannelInit: ChannelPdoExtension->AhciChannel          - %d\n", ChannelPdoExtension->AhciChannel);
  DPRINT("AhciXChannelInit: ChannelPdoExtension->AhciPortControl      - %p\n", ChannelPdoExtension->AhciPortControl);
  DPRINT("AhciXChannelInit: ChannelPdoExtension->AhciBuffer           - %p\n", ChannelPdoExtension->AhciBuffer);
  //DPRINT("AhciXChannelInit: ChannelPdoExtension->AhciCommandTable[0]  - %p\n", ChannelPdoExtension->AhciCommandTable[0]);
  DPRINT("AhciXChannelInit: ChannelPdoExtension->CmdListBaseAddress   - %p\n", ChannelPdoExtension->CmdListBaseAddress);
  DPRINT("AhciXChannelInit: ChannelPdoExtension->PhCmdListBaseAddress - %p\n", ChannelPdoExtension->PhCmdListBaseAddress);
  DPRINT("AhciXChannelInit: ChannelPdoExtension->FISBaseAddress       - %p\n", ChannelPdoExtension->FISBaseAddress);
  DPRINT("AhciXChannelInit: ChannelPdoExtension->PhFISBaseAddress     - %p\n", ChannelPdoExtension->PhFISBaseAddress);

  DPRINT("\n");

  //DPRINT("AhciXChannelInit: Port->CmdListBaseAddress      - %p\n", READ_REGISTER_ULONG((PULONG)&Port->CmdListBaseAddress));
  //DPRINT("AhciXChannelInit: Port->CmdListBaseAddressUpper - %p\n", READ_REGISTER_ULONG((PULONG)&Port->CmdListBaseAddressUpper));
  //DPRINT("AhciXChannelInit: Port->FISBaseAddress          - %p\n", READ_REGISTER_ULONG((PULONG)&Port->FISBaseAddress));
  //DPRINT("AhciXChannelInit: Port->FISBaseAddressUpper     - %p\n", READ_REGISTER_ULONG((PULONG)&Port->FISBaseAddressUpper));
  DPRINT("AhciXChannelInit: Port->InterruptStatus         - %p\n", READ_REGISTER_ULONG((PULONG)&Port->InterruptStatus));
  DPRINT("AhciXChannelInit: Port->InterruptEnable         - %p\n", READ_REGISTER_ULONG((PULONG)&Port->InterruptEnable));
  DPRINT("AhciXChannelInit: Port->Command                 - %p\n", READ_REGISTER_ULONG((PULONG)&Port->Command));
  DPRINT("AhciXChannelInit: Port->Reserved1               - %p\n", READ_REGISTER_ULONG((PULONG)&Port->Reserved1));
  DPRINT("AhciXChannelInit: Port->TaskFileData            - %p\n", READ_REGISTER_ULONG((PULONG)&Port->TaskFileData));
  //DPRINT("AhciXChannelInit: Port->Signature               - %p\n", READ_REGISTER_ULONG((PULONG)&Port->Signature));
  DPRINT("AhciXChannelInit: Port->SataStatus              - %p\n", READ_REGISTER_ULONG((PULONG)&Port->SataStatus));
  DPRINT("AhciXChannelInit: Port->SataControl             - %p\n", READ_REGISTER_ULONG((PULONG)&Port->SataControl));
  DPRINT("AhciXChannelInit: Port->SataError               - %p\n", READ_REGISTER_ULONG((PULONG)&Port->SataError));
  DPRINT("AhciXChannelInit: Port->SataActive              - %p\n", READ_REGISTER_ULONG((PULONG)&Port->SataActive));
  DPRINT("AhciXChannelInit: Port->CommandIssue            - %p\n", READ_REGISTER_ULONG((PULONG)&Port->CommandIssue));
  DPRINT("AhciXChannelInit: Port->SataNotification        - %p\n", READ_REGISTER_ULONG((PULONG)&Port->SataNotification));
  DPRINT("AhciXChannelInit: Port->FISSwitchingControl     - %p\n", READ_REGISTER_ULONG((PULONG)&Port->FISSwitchingControl));
  */

  DPRINT("AhciXChannelInit return - %x \n", Status);
  return Status;
}

AHCI_DEVICE_TYPE
AhciSignatureCheck(IN PAHCI_PORT_REGISTERS PortControl)
{
  AHCI_SATA_STATUS  SataStatus;
  UCHAR             DeviceDetection;
  //UCHAR           Ipm;

  SataStatus = PortControl->SataStatus;
  DeviceDetection = SataStatus.DeviceDetection;
  //Ipm = (SataStatus.AsULONG >> 8) & 0x0F;

  DPRINT("AhciSignatureCheck: PortControl     - %p\n", PortControl);
  DPRINT("AhciSignatureCheck: SataStatus      - %p\n", SataStatus);
  DPRINT("AhciSignatureCheck: DeviceDetection - %p\n", DeviceDetection);

  // Check drive status
  if ( DeviceDetection == 0 )
    return NoDrive;

  //if (Ipm != 1)
  //  return 0;

  DPRINT("AhciSignatureCheck: PortControl->Signature - %p\n", PortControl->Signature);

  switch ( PortControl->Signature )
  {
    case 0x00000101:			// 1  SATA ATA
      return AtaDrive;

    case 0xEB140101:			// 2  SATA ATAPI
      return AtapiDrive;

    case 0xC33C0101:			// 3  Enclosure management bridge
      return SEMBdrive;

    case 0x96690101:			// 4  Port multiplier
      return PortMultiplier;

    default:				// 5
      return DriveNotResponded;
  }
}

NTSTATUS
AhciXFdoQueryBusRelations(
    IN PDEVICE_OBJECT DeviceObject,
    OUT PDEVICE_RELATIONS* pDeviceRelations)
{
  PFDO_CONTROLLER_EXTENSION  ControllerFdoExtension;
  PDEVICE_RELATIONS          DeviceRelations = NULL;
  PAHCI_MEMORY_REGISTERS     Abar;
  ULONG                      PortsImplemented;
  ULONG                      ix, jx;
  ULONG                      PDOs = 0;
  AHCI_DEVICE_TYPE           DeviceType;
  PDEVICE_OBJECT             Pdo;
  PPDO_CHANNEL_EXTENSION     ChannelPdoExtension;
  PAHCI_INTERFACE            AhciInterface;
  NTSTATUS                   Status;

  DPRINT("AhciXFdoQueryBusRelations(%p %p)\n", DeviceObject, pDeviceRelations);

  ControllerFdoExtension = (PFDO_CONTROLLER_EXTENSION)DeviceObject->DeviceExtension;
  ASSERT(ControllerFdoExtension);
  ASSERT(ControllerFdoExtension->Common.IsFDO);

  Abar = ControllerFdoExtension->AhciRegisters;
  PortsImplemented = Abar->PortsImplemented;

  if ( Abar && PortsImplemented )
  {
    for ( ix = 0, jx = 0; ix < 32; ix++ )
    {
      if ( PortsImplemented & 1 )
      {
        DPRINT("AhciXFdoQueryBusRelations: ix - %x\n", ix);

        DeviceType = AhciSignatureCheck((PAHCI_PORT_REGISTERS)&Abar->PortControl[ix]);
        ControllerFdoExtension->DeviceType[ix] = DeviceType;
        DPRINT("AhciXFdoQueryBusRelations: AhciSignatureCheck return - %x\n", DeviceType);

        if ( DeviceType == AtaDrive )
        {
          DPRINT("SATA ATA drive found at port %d\n", ix);
        }
        else if ( DeviceType == AtapiDrive )
        {
          DPRINT("SATA ATAPI drive found at port %d\n", ix);
        }
        else if ( DeviceType == SEMBdrive ) // Enclosure management bridge
        {
          DPRINT("SEMB drive found at port %d\n", ix);
          //ControllerFdoExtension->DeviceType[ix] = DriveNotResponded;
        }
        else if ( DeviceType == PortMultiplier )
        {
          DPRINT("PM drive found at port %d\n", ix);
          //ControllerFdoExtension->DeviceType[ix] = DriveNotResponded;
        }
        else
        {
          DPRINT("No drive found at port %d\n", ix);
          PortsImplemented >>= 1;
          continue;
        }

        if ( ControllerFdoExtension->ChannelPdo[ix] ) //уже есть PDO, поэтому пропускаем
        {
          PDOs++;
          continue;
        }

        /* Need to create a PDO */
        Status = IoCreateDevice(
                   DeviceObject->DriverObject,
                   sizeof(PDO_CHANNEL_EXTENSION),
                   NULL,
                   FILE_DEVICE_CONTROLLER,
                   FILE_AUTOGENERATED_DEVICE_NAME,
                   FALSE,
                   &Pdo);

        if ( !NT_SUCCESS(Status) ) /* FIXME: handle error */
          continue;

        ChannelPdoExtension = (PPDO_CHANNEL_EXTENSION)Pdo->DeviceExtension;

        RtlZeroMemory(ChannelPdoExtension, sizeof(PDO_CHANNEL_EXTENSION));

        ChannelPdoExtension->Common.IsFDO  = FALSE;
        ChannelPdoExtension->ControllerFdo = DeviceObject;

        AhciInterface = &ChannelPdoExtension->AhciInterface;

        AhciInterface->Channel             = jx++;
        AhciInterface->AhciChannel         = ix;
        AhciInterface->DeviceType          = DeviceType;
        AhciInterface->ChannelPdoExtension = ChannelPdoExtension;
        AhciInterface->Abar                = Abar;
        AhciInterface->InterruptResource   = &ControllerFdoExtension->InterruptResource;

        Status = AhciXChannelInit(ChannelPdoExtension, ix);

        if ( !NT_SUCCESS(Status) )
        {
          IoDeleteDevice(Pdo);
          continue;
        }

        Pdo->Flags |= DO_BUS_ENUMERATED_DEVICE;
        Pdo->Flags &= ~DO_DEVICE_INITIALIZING;

        ControllerFdoExtension->ChannelPdo[ix] = Pdo;

        PDOs++;
        PortsImplemented >>= 1;
      }
      else
      {
        ControllerFdoExtension->DeviceType[ix] = NoDrive;
      }
    }
  }

  if ( PDOs == 0 )
  {
    DeviceRelations = (PDEVICE_RELATIONS)
                      ExAllocatePool(PagedPool, sizeof(DEVICE_RELATIONS));
  }
  else
  {
    DeviceRelations = (PDEVICE_RELATIONS)
                      ExAllocatePool(PagedPool,
                                     sizeof(DEVICE_RELATIONS) + 
                                     sizeof(PDEVICE_OBJECT) * (PDOs - 1));

    ControllerFdoExtension->ChannelsCount = PDOs;
  }

  if ( !DeviceRelations )
    return STATUS_INSUFFICIENT_RESOURCES;

  DeviceRelations->Count = PDOs;

  if ( PDOs )
  {
    for ( ix = 0, jx = 0; ix < 32; ix++ )
    {
      if ( ControllerFdoExtension->ChannelPdo[ix] )
      {
        ObReferenceObject(ControllerFdoExtension->ChannelPdo[ix]);
        DeviceRelations->Objects[jx++] = ControllerFdoExtension->ChannelPdo[ix];
      }
    }
  }

  *pDeviceRelations = DeviceRelations;

  return STATUS_SUCCESS;
}

NTSTATUS
AhciXParseTranslatedResources(
    IN PFDO_CONTROLLER_EXTENSION ControllerFdoExtension,
    IN PCM_RESOURCE_LIST ResourcesTranslated)
{
  PAHCI_MEMORY_REGISTERS    AhciRegisters = 0;
  AHCI_HOST_CAPABILITIES    HostCapabilities;
  PAHCI_INTERRUPT_RESOURCE  InterruptResource;
  PVOID                     ResourceBase;
  ULONG                     jx;
  NTSTATUS                  Status = STATUS_INSUFFICIENT_RESOURCES;

  DPRINT("AhciXParseTranslatedResources: ControllerFdoExtension->ControllerMode - %x \n", ControllerFdoExtension->ControllerMode );

  InterruptResource = &ControllerFdoExtension->InterruptResource;

  if ( ResourcesTranslated->Count > 1 )
    DPRINT1("ERROR: ResourcesTranslated->Count > 1!\n");

  // WDM драйвер использует только первый CM_FULL_RESOURCE_DESCRIPTOR
  for ( jx = 0; jx < ResourcesTranslated->List[0].PartialResourceList.Count; ++jx )  // CM_PARTIAL_RESOURCE_DESCRIPTOR jx
  {
    PCM_PARTIAL_RESOURCE_DESCRIPTOR  Descriptor;

    Descriptor = ResourcesTranslated->List[0].PartialResourceList.PartialDescriptors + jx;
    ASSERT(Descriptor->Type != CmResourceTypeDeviceSpecific);  // ResType_ClassSpecific (0xFFFF)

    switch ( Descriptor->Type )
    {
      case CmResourceTypePort:       /* 1 */
      {
        DPRINT("CmResourceTypePort\n");
        DPRINT("AhciXParseTranslatedResources: Descriptor->u.Port.Start.LowPart   - %p \n", Descriptor->u.Port.Start.LowPart );
        DPRINT("AhciXParseTranslatedResources: Descriptor->u.Port.Length          - %p \n", Descriptor->u.Port.Length );
        break;
      }

      case CmResourceTypeMemory:     /* 3 */
      {
        DPRINT("CmResourceTypeMemory\n");
        if ( ControllerFdoExtension->ControllerMode == CONTROLLER_AHCI_MODE  &&
             Descriptor->u.Memory.Length >= sizeof(AHCI_MEMORY_REGISTERS) )
        {
          ResourceBase = MmMapIoSpace(
                           Descriptor->u.Memory.Start,
                           sizeof(AHCI_MEMORY_REGISTERS),
                           MmNonCached);

          if ( !ResourceBase )
          {
            DPRINT1("MmMapIoSpace failed\n");
            return STATUS_INSUFFICIENT_RESOURCES;
          }

          AhciRegisters = (PAHCI_MEMORY_REGISTERS)ResourceBase;

          DPRINT("AhciXParseTranslatedResources: &AhciRegisters - %p\n", &AhciRegisters);
          DPRINT("AhciXParseTranslatedResources: AhciRegisters  - %p\n", AhciRegisters);

          HostCapabilities.AsULONG = READ_REGISTER_ULONG((PULONG)((ULONG_PTR)AhciRegisters + 0));
          DPRINT("AhciRegisters->HostCapabilities - %p\n", HostCapabilities);

          DPRINT("HostCapabilities.NumberPorts                    - %x\n", HostCapabilities.NumberPorts);
          DPRINT("HostCapabilities.SupportsExternalSATA           - %x\n", HostCapabilities.SupportsExternalSATA);
          DPRINT("HostCapabilities.EnclosureManagement            - %x\n", HostCapabilities.EnclosureManagement);
          DPRINT("HostCapabilities.CmdCompletionCoalescing        - %x\n", HostCapabilities.CmdCompletionCoalescing);
          DPRINT("HostCapabilities.NumberCommandSlots             - %x\n", HostCapabilities.NumberCommandSlots);
          DPRINT("HostCapabilities.PartialStateCapable            - %x\n", HostCapabilities.PartialStateCapable);
          DPRINT("HostCapabilities.SlumberStateCapable            - %x\n", HostCapabilities.SlumberStateCapable);
          DPRINT("HostCapabilities.PioMultipleDrqBlock            - %x\n", HostCapabilities.PioMultipleDrqBlock);
          DPRINT("HostCapabilities.FISbasedSwitching              - %x\n", HostCapabilities.FISbasedSwitching);
          DPRINT("HostCapabilities.PortMultiplier                 - %x\n", HostCapabilities.PortMultiplier);
          DPRINT("HostCapabilities.AhciModeOnly                   - %x\n", HostCapabilities.AhciModeOnly);
          DPRINT("HostCapabilities.InterfaceSpeed                 - %x\n", HostCapabilities.InterfaceSpeed);
          DPRINT("HostCapabilities.CommandListOverride            - %x\n", HostCapabilities.CommandListOverride);
          DPRINT("HostCapabilities.ActivityLED                    - %x\n", HostCapabilities.ActivityLED);
          DPRINT("HostCapabilities.AggressiveLinkPowerManagement  - %x\n", HostCapabilities.AggressiveLinkPowerManagement);
          DPRINT("HostCapabilities.StaggeredSpinUp                - %x\n", HostCapabilities.StaggeredSpinUp);
          DPRINT("HostCapabilities.MechanicalPresenceSwitch       - %x\n", HostCapabilities.MechanicalPresenceSwitch);
          DPRINT("HostCapabilities.SNotificationRegister          - %x\n", HostCapabilities.SNotificationRegister);
          DPRINT("HostCapabilities.NativeCommandQueuing           - %x\n", HostCapabilities.NativeCommandQueuing);
          DPRINT("HostCapabilities.Addressing64bit                - %x\n", HostCapabilities.Addressing64bit);

          DPRINT("AhciRegisters->GlobalHostControl    - %p\n", READ_REGISTER_ULONG((PULONG)((ULONG_PTR)AhciRegisters + 4)));
          DPRINT("AhciRegisters->InterruptStatus      - %p\n", READ_REGISTER_ULONG((PULONG)((ULONG_PTR)AhciRegisters + 8)));
          DPRINT("AhciRegisters->PortsImplemented     - %p\n", READ_REGISTER_ULONG((PULONG)((ULONG_PTR)AhciRegisters + 12)));
          DPRINT("AhciRegisters->Version              - %p\n", READ_REGISTER_ULONG((PULONG)((ULONG_PTR)AhciRegisters + 16)));
          DPRINT("AhciRegisters->CmdCompletionControl - %p\n", READ_REGISTER_ULONG((PULONG)((ULONG_PTR)AhciRegisters + 20)));
          DPRINT("AhciRegisters->CmdCompletionPorts   - %p\n", READ_REGISTER_ULONG((PULONG)((ULONG_PTR)AhciRegisters + 24)));
          DPRINT("AhciRegisters->EMLocation           - %p\n", READ_REGISTER_ULONG((PULONG)((ULONG_PTR)AhciRegisters + 28)));
          DPRINT("AhciRegisters->EMControl            - %p\n", READ_REGISTER_ULONG((PULONG)((ULONG_PTR)AhciRegisters + 32)));
          DPRINT("AhciRegisters->HostCapabilitiesExt  - %p\n", READ_REGISTER_ULONG((PULONG)((ULONG_PTR)AhciRegisters + 36)));
          DPRINT("AhciRegisters->Handoff              - %p\n", READ_REGISTER_ULONG((PULONG)((ULONG_PTR)AhciRegisters + 40)));

          ControllerFdoExtension->AhciRegisters = AhciRegisters;
        }

        DPRINT("AhciXParseTranslatedResources: Descriptor->u.Memory.Start.LowPart - %p \n", Descriptor->u.Memory.Start.LowPart );
        DPRINT("AhciXParseTranslatedResources: Descriptor->u.Memory.Length        - %p \n", Descriptor->u.Memory.Length );
        break;
      }

      case CmResourceTypeInterrupt:  /* 2 */
      {
        DPRINT("Descriptor->Type / CmResourceTypeInterrupt\n");

        InterruptResource->InterruptShareDisposition = Descriptor->ShareDisposition;
        InterruptResource->InterruptFlags            = Descriptor->Flags;
        InterruptResource->InterruptLevel            = Descriptor->u.Interrupt.Level;
        InterruptResource->InterruptVector           = Descriptor->u.Interrupt.Vector;
        InterruptResource->InterruptAffinity         = Descriptor->u.Interrupt.Affinity;

        DPRINT("AhciXParseTranslatedResources: Descriptor->u.Interrupt.Level      - %x \n", Descriptor->u.Interrupt.Level );
        DPRINT("AhciXParseTranslatedResources: Descriptor->u.Interrupt.Vector     - %x \n", Descriptor->u.Interrupt.Vector );
        DPRINT("AhciXParseTranslatedResources: Descriptor->u.Interrupt.Affinity   - %x \n", Descriptor->u.Interrupt.Affinity );
        break;
      }

      default:
      {
        DPRINT1("Descriptor->Type / Unknownn - %x\n", Descriptor->Type);
      }
    }
  }

  if ( AhciRegisters                      && 
       InterruptResource->InterruptLevel  &&
       InterruptResource->InterruptVector)
  {
    Status = STATUS_SUCCESS;
  }

  return Status;
}

NTSTATUS
AhciXFdoStartDevice(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
  PFDO_CONTROLLER_EXTENSION  ControllerFdoExtension;
  PCM_RESOURCE_LIST          ResourcesTranslated;
  NTSTATUS                   Status;

  DPRINT("AhciXStartDevice(%p %p)\n", DeviceObject, Irp);

  ControllerFdoExtension = (PFDO_CONTROLLER_EXTENSION)DeviceObject->DeviceExtension;
  ASSERT(ControllerFdoExtension);
  ASSERT(ControllerFdoExtension->Common.IsFDO);

  // ƒл€ функции StartDevice в IRP передаетс€ указатель на структуру
  // PCM_RESOURCE_LIST с ресурсами (могут быть RAW и Translated. см. DDK)
  ResourcesTranslated = IoGetCurrentIrpStackLocation(Irp)->
                          Parameters.StartDevice.AllocatedResourcesTranslated;

  if ( !ResourcesTranslated )
    return STATUS_INVALID_PARAMETER;

  if ( !ResourcesTranslated->Count )
    return STATUS_INVALID_PARAMETER;

  // ќпредел€ем ресурсы
  Status = AhciXParseTranslatedResources(
                ControllerFdoExtension,
                ResourcesTranslated);

  DPRINT("AhciXStartDevice return - %x \n", Status);

  return Status;
}

NTSTATUS
AhciXFdoPnpDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
  PIO_STACK_LOCATION  Stack;
  ULONG_PTR           Information;
  ULONG               MinorFunction;
  NTSTATUS            Status;

  Information = Irp->IoStatus.Information;
  Stack = IoGetCurrentIrpStackLocation(Irp);
  MinorFunction = Stack->MinorFunction;

  switch ( MinorFunction )
  {
    case IRP_MN_START_DEVICE:                  /* 0x00 */
      DPRINT("IRP_MJ_PNP / IRP_MN_START_DEVICE\n");
      /* Call lower driver */
      Status = ForwardIrpAndWait(DeviceObject, Irp);
      if ( NT_SUCCESS(Status) )
        Status = AhciXFdoStartDevice(DeviceObject, Irp);
      break;

    case IRP_MN_QUERY_REMOVE_DEVICE:           /* 0x01 */
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_REMOVE_DEVICE\n");
      Status = STATUS_UNSUCCESSFUL;
      break;

    case IRP_MN_QUERY_DEVICE_RELATIONS:        /* 0x07 */
    {
      switch ( Stack->Parameters.QueryDeviceRelations.Type )
      {
        case BusRelations:
        {
          PDEVICE_RELATIONS DeviceRelations = NULL;
          DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_DEVICE_RELATIONS / BusRelations\n");
          Status = AhciXFdoQueryBusRelations(DeviceObject, &DeviceRelations);
          Information = (ULONG_PTR)DeviceRelations;
          break;
        }
  
        default:
        {
          DPRINT1("IRP_MJ_PNP / IRP_MN_QUERY_DEVICE_RELATIONS / Unknown type 0x%lx\n", Stack->Parameters.QueryDeviceRelations.Type);
          Status = STATUS_NOT_IMPLEMENTED;
          break;
        }
      }

      break;
    }

    case IRP_MN_QUERY_PNP_DEVICE_STATE:        /* 0x14 */
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_PNP_DEVICE_STATE\n");
      Information |= PNP_DEVICE_NOT_DISABLEABLE;
      Status = STATUS_SUCCESS;
      break;

    case IRP_MN_FILTER_RESOURCE_REQUIREMENTS:  /* 0x0d */
      DPRINT("IRP_MJ_PNP / IRP_MN_FILTER_RESOURCE_REQUIREMENTS\n");
      return ForwardIrpAndForget(DeviceObject, Irp);

    default:
      DPRINT1("IRP_MJ_PNP / Unknown minor function 0x%lx\n", MinorFunction);
      return ForwardIrpAndForget(DeviceObject, Irp);
  }

  Irp->IoStatus.Information = Information;
  Irp->IoStatus.Status = Status;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);

  return Status;
}
