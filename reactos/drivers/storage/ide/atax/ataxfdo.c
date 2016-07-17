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

  // HACK!!! (��������� ������������ ���� InterfaceType � GUID_***)
  // Size - ��� ��� ���������� (� ������� �� InterfaceType)
  // 1: QueryControllerProperties
  // 3: QueryPciBusInterface
  // 5: QueryBusMasterInterface
  // 7: QueryAhciInterface
  // 9: QuerySataInterface
  IoStack->Parameters.QueryInterface.Size = 3; // QueryPciBusInterface

  //����� GUID, ������ ��������� � GUID � �������������� ������� ������� PDO (PciIdeXPdoPnpDispatch)
  IoStack->Parameters.QueryInterface.InterfaceType = (LPGUID)&GUID_BUS_INTERFACE_STANDARD;

  IoStack->Parameters.QueryInterface.Version = 1; //�� ������������
  IoStack->Parameters.QueryInterface.Interface = (PINTERFACE)&AtaXChannelFdoExtension->BusInterface;
  IoStack->Parameters.QueryInterface.InterfaceSpecificData = NULL; //�� ������������

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

static NTSTATUS NTAPI 
AtaXParseTranslatedResources(IN PFDO_CHANNEL_EXTENSION AtaXChannelFdoExtension, IN PCM_RESOURCE_LIST ResourcesTranslated)
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

  // WDM ������� ���������� ������ ������ CM_FULL_RESOURCE_DESCRIPTOR
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

  // ��� ������� StartDevice � IRP ���������� ��������� �� ��������� PCM_RESOURCE_LIST � ���������
  // (����� ���� RAW � Translated. ��. DDK)
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

  // �������� ������ ������ (pciide � ��� ��������)
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

ASSERT(FALSE);
  Status = AtaXParseTranslatedResources(AtaXChannelFdoExtension, ResourcesTranslated);

  if ( NT_SUCCESS(Status) && AtaXChannelFdoExtension->InterruptLevel )
  {
ASSERT(FALSE);
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
      Status = 0;//AtaXChannelFdoRemoveDevice(AtaXChannelFdo, Irp);
      break;
    }

    case IRP_MN_STOP_DEVICE:                  /* 0x04 */  //AtaXChannelFdoStopDevice
    {
      DPRINT("IRP_MJ_PNP / IRP_MN_STOP_DEVICE\n");
ASSERT(FALSE);
      Status = 0;//AtaXChannelFdoStopDevice(AtaXChannelFdo, Irp);
      break;
    }

    case IRP_MN_QUERY_DEVICE_RELATIONS:       /* 0x07 */  //AtaXChannelFdoQueryDeviceRelations
    {
      switch (Stack->Parameters.QueryDeviceRelations.Type)
      {
        case BusRelations:
          DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_DEVICE_RELATIONS / BusRelations\n");
ASSERT(FALSE);
//          Status = AtaXChannelFdoQueryDeviceRelations(AtaXChannelFdo, Irp);
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
ASSERT(FALSE);
      Status = 0;//AtaXChannelFdoQueryPnPDeviceState(AtaXChannelFdo, Irp);
      break;
    }

    case IRP_MN_DEVICE_USAGE_NOTIFICATION:    /* 0x16 */  //AtaXChannelFdoUsageNotification
    {
      DPRINT("IRP_MJ_PNP / IRP_MN_DEVICE_USAGE_NOTIFICATION\n");
ASSERT(FALSE);
      Status = 0;//AtaXChannelFdoUsageNotification(AtaXChannelFdo, Irp);
      break;
    }

    case IRP_MN_SURPRISE_REMOVAL:             /* 0x17 */  //AtaXChannelFdoSurpriseRemoveDevice
    {
      DPRINT("IRP_MJ_PNP / IRP_MN_SURPRISE_REMOVAL\n");
ASSERT(FALSE);
      Status = 0;//AtaXChannelFdoSurpriseRemoveDevice(AtaXChannelFdo, Irp);
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

  // ������� ����� �������������� ������ FDO ��� ������ (Primary ��� Secondary)
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

  // ��������� ���������� FDO ������
  AtaXChannelFdoExtension = (PFDO_CHANNEL_EXTENSION)AtaXChannelFdo->DeviceExtension;
  RtlZeroMemory(AtaXChannelFdoExtension, sizeof(FDO_CHANNEL_EXTENSION));
  
  DPRINT("AddChannelFdo: AtaXChannelFdo - %p, AtaXChannelFdoExtension - %p, ChannelPdo - %p\n", AtaXChannelFdo, AtaXChannelFdoExtension, ChannelPdo);
 
  AtaXChannelFdoExtension->CommonExtension.IsFDO      = TRUE;
  AtaXChannelFdoExtension->CommonExtension.LowerPdo   = ChannelPdo;
  AtaXChannelFdoExtension->CommonExtension.SelfDriver = DriverObject;
  AtaXChannelFdoExtension->CommonExtension.SelfDevice = AtaXChannelFdo;

  // ������������ ����� ��������� FDO � ����� �������� ���������
  AttachedDevice = IoAttachDeviceToDeviceStack(AtaXChannelFdo, ChannelPdo);

  if ( AttachedDevice )
  {
    // ������ � �������� �������������� (�� ����������� ��� ChannelPdo. ����� ���� � ����������� DO)
    AtaXChannelFdoExtension->CommonExtension.LowerDevice = AttachedDevice;

    AtaXChannelFdo->AlignmentRequirement = AttachedDevice->AlignmentRequirement;
    if ( AtaXChannelFdo->AlignmentRequirement < 1 )
      AtaXChannelFdo->AlignmentRequirement = 1;

    // ���� FDO � �����, �� ����������� ������� �������
    AtaXChannelFdoExtension->Channel = AtaXChannelCounter & 1;
    AtaXChannelCounter++;

    AtaXChannelFdo->Flags &= ~DO_DEVICE_INITIALIZING;
  }
  else
  {
    // �� ������ ...
    IoDeleteDevice(AtaXChannelFdo);
    Status = STATUS_UNSUCCESSFUL;
  }
  
  DPRINT("AddChannelFdo: Status - %x \n", Status);
  DPRINT("AddChannelFdo ---------------------------------------------------------- \n"  );
  return Status;
}
