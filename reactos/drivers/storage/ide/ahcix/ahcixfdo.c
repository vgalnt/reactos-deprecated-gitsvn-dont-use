
#include "ahcix.h"

//#define NDEBUG
#include <debug.h>


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
ASSERT(FALSE);
          Status = 0;//AhciXFdoQueryBusRelations(DeviceObject, &DeviceRelations);
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
