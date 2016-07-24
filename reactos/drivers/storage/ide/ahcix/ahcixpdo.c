
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
AhciXPdoPnpDispatch(
    IN PDEVICE_OBJECT ChannelPdo,
    IN PIRP Irp)
{
  PPDO_CHANNEL_EXTENSION     ChannelPdoExtension;
  ULONG_PTR                  Information;
  PIO_STACK_LOCATION         Stack;
  ULONG                      MinorFunction;
  NTSTATUS                   Status;

  ChannelPdoExtension = (PPDO_CHANNEL_EXTENSION)ChannelPdo->DeviceExtension;
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
ASSERT(FALSE);
      break;

    case IRP_MN_QUERY_REMOVE_DEVICE:           /* 0x01 */
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_REMOVE_DEVICE\n");
      Status = STATUS_UNSUCCESSFUL;
      break;

    case IRP_MN_QUERY_DEVICE_RELATIONS:        /* 0x07 */
ASSERT(FALSE);
      break;

    case IRP_MN_QUERY_INTERFACE:               /* 0x08 */
ASSERT(FALSE);

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
