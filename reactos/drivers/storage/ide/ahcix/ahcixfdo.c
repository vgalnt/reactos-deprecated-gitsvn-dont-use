
#include "ahcix.h"

//#define NDEBUG
#include <debug.h>


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

  DPRINT("AhciXFdoQueryBusRelations(%p %p)\n", DeviceObject, pDeviceRelations);

  ControllerFdoExtension = (PFDO_CONTROLLER_EXTENSION)DeviceObject->DeviceExtension;
  ASSERT(ControllerFdoExtension);
  ASSERT(ControllerFdoExtension->Common.IsFDO);

  Abar = ControllerFdoExtension->AhciRegisters;
  PortsImplemented = Abar->PortsImplemented;

  if ( Abar && PortsImplemented )
  {
ASSERT(FALSE);
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

  // Для функции StartDevice в IRP передается указатель на структуру
  // PCM_RESOURCE_LIST с ресурсами (могут быть RAW и Translated. см. DDK)
  ResourcesTranslated = IoGetCurrentIrpStackLocation(Irp)->
                          Parameters.StartDevice.AllocatedResourcesTranslated;

  if ( !ResourcesTranslated )
    return STATUS_INVALID_PARAMETER;

  if ( !ResourcesTranslated->Count )
    return STATUS_INVALID_PARAMETER;

  // Определяем ресурсы
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
