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
      DPRINT("AtaXChannelFdoStartDevice: ResourcesTranslated->Count - %p\n", ResourcesTranslated->Count);
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

ASSERT(FALSE);
  Status = 0;//AtaXParseTranslatedResources(AtaXChannelFdoExtension, ResourcesTranslated);

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
