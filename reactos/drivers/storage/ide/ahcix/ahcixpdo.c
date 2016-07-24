
#include "ahcix.h"

//#define NDEBUG
#include <debug.h>


NTSTATUS
AhciXPdoPnpDispatch(
    IN PDEVICE_OBJECT ChannelPdo,
    IN PIRP Irp)
{
  ULONG_PTR                  Information;
  PIO_STACK_LOCATION         Stack;
  ULONG                      MinorFunction;
  NTSTATUS                   Status;

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
ASSERT(FALSE);

    case IRP_MN_QUERY_RESOURCES:               /* 0x0a */
      /* This IRP is optional; do nothing */
      Information = Irp->IoStatus.Information;
      Status = Irp->IoStatus.Status;
      break;

    case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:   /* 0x0b */
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_RESOURCE_REQUIREMENTS\n");
ASSERT(FALSE);
      break;

    case IRP_MN_QUERY_DEVICE_TEXT:             /* 0x0c */
ASSERT(FALSE);
      break;

    case IRP_MN_FILTER_RESOURCE_REQUIREMENTS:  /* 0x0d */
      DPRINT("IRP_MJ_PNP / IRP_MN_FILTER_RESOURCE_REQUIREMENTS\n");
      Information = Irp->IoStatus.Information;
      Status = Irp->IoStatus.Status;
      break;

    case IRP_MN_QUERY_ID:                      /* 0x13 */
ASSERT(FALSE);
      break;

    case IRP_MN_QUERY_PNP_DEVICE_STATE:        /* 0x14 */
       DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_PNP_DEVICE_STATE\n");
       Information |= PNP_DEVICE_NOT_DISABLEABLE;
       Status = STATUS_SUCCESS;
       break;

    case IRP_MN_QUERY_BUS_INFORMATION:         /* 0x15 */
ASSERT(FALSE);

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
