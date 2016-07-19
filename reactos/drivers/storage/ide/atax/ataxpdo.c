#include "atax.h"

//#define NDEBUG
#include <debug.h>


BOOLEAN
AtaXQueueAddIrp(
    IN PPDO_DEVICE_EXTENSION AtaXDevicePdoExtension,
    IN PIRP Irp,
    IN ULONG SortKey)
{
  BOOLEAN  Result = FALSE;
  KIRQL    OldIrql;

  DPRINT("AtaXQueueAddIrp: AtaXDevicePdoExtension - %p, Irp - %p, SortKey - %x\n", AtaXDevicePdoExtension, Irp, SortKey);

  KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

  Result = KeInsertByKeyDeviceQueue(
               &AtaXDevicePdoExtension->DeviceQueue,
               &Irp->Tail.Overlay.DeviceQueueEntry,
               SortKey);

  KeLowerIrql(OldIrql);

  DPRINT("AtaXQueueAddIrp return - %x \n", Result);
  return Result;
}

NTSTATUS
AtaXDevicePdoDispatchScsi(
    IN PDEVICE_OBJECT AtaXDevicePdo,
    IN PIRP Irp)
{
  PPDO_DEVICE_EXTENSION   AtaXDevicePdoExtension;
  PFDO_CHANNEL_EXTENSION  AtaXChannelFdoExtension;
  PIO_STACK_LOCATION      IoStack;
  PSCSI_REQUEST_BLOCK     Srb;
  BOOLEAN                 QueueIsNotEmpty;
  NTSTATUS                Status;

  IoStack = IoGetCurrentIrpStackLocation(Irp);
  Srb = (PSCSI_REQUEST_BLOCK)IoStack->Parameters.Scsi.Srb;  //Srb = (PSCSI_REQUEST_BLOCK)IoStack->Parameters.Others.Argument1;

  if ( Srb == NULL )
  {
    DPRINT("AtaXDevicePdoDispatchScsi: Srb = NULL!\n");
    Status = STATUS_UNSUCCESSFUL;
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return(Status);
  }

  DPRINT(" AtaXDevicePdoDispatchScsi: AtaXDevicePdo - %p, Irp - %p, Srb - %p\n", AtaXDevicePdo, Irp, Srb);

  AtaXDevicePdoExtension = AtaXDevicePdo->DeviceExtension;
  ASSERT(AtaXDevicePdoExtension);
  ASSERT(AtaXDevicePdoExtension->CommonExtension.IsFDO == FALSE);

  AtaXChannelFdoExtension = (PFDO_CHANNEL_EXTENSION)AtaXDevicePdoExtension->AtaXChannelFdoExtension;
  ASSERT(AtaXChannelFdoExtension);
  ASSERT(AtaXChannelFdoExtension->CommonExtension.IsFDO);

  // "Классовые" драйвера ни в какую не хотят использовать в SRBs эти поля:
  Srb->PathId   = AtaXDevicePdoExtension->PathId;
  Srb->TargetId = AtaXDevicePdoExtension->TargetId;
  Srb->Lun      = AtaXDevicePdoExtension->Lun;

  switch( Srb->Function )
  {
    case SRB_FUNCTION_EXECUTE_SCSI:           /* 0x00 */

      if ( AtaXChannelFdoExtension->DeviceFlags[Srb->TargetId] & DFLAGS_USE_DMA )
      {
        Srb->SrbFlags |= SRB_FLAGS_USE_DMA;
        DPRINT("AtaXDevicePdoDispatchScsi: SRB_FUNCTION_EXECUTE_SCSI. DMA mode\n");
      }
      else
      {
        Srb->SrbFlags &= ~SRB_FLAGS_USE_DMA;
        DPRINT("AtaXDevicePdoDispatchScsi: SRB_FUNCTION_EXECUTE_SCSI. PIO mode\n");
      }

      switch( Srb->Cdb[0] )
      {
        case SCSIOP_READ:               /* 0x28 */
          DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_EXECUTE_SCSI / SCSIOP_READ\n");
          break; // --> IoStartPacket

        case SCSIOP_WRITE:              /* 0x2A */
          DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_EXECUTE_SCSI / SCSIOP_WRITE\n");
          break; // --> IoStartPacket

        case SCSIOP_TEST_UNIT_READY:    /* 0x00 */
          DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_EXECUTE_SCSI / SCSIOP_TEST_UNIT_READY \n");
ASSERT(FALSE);

        case SCSIOP_REQUEST_SENSE:      /* 0x03 */
          DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_EXECUTE_SCSI / SCSIOP_REQUEST_SENSE (PIO mode)\n");
ASSERT(FALSE);

        case SCSIOP_INQUIRY:            /* 0x12 */
          DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_EXECUTE_SCSI / SCSIOP_INQUIRY (PIO mode)\n");
          Srb->SrbFlags &= ~SRB_FLAGS_USE_DMA;  // PIO mode
          if ( AtaXChannelFdoExtension->DeviceFlags[Srb->TargetId] & DFLAGS_ATAPI_DEVICE ) //if ATAPI
          {
            break;// --> IoStartPacket
          }
          else
          {
            DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_EXECUTE_SCSI / SCSIOP_INQUIRY - FIXME non ATAPI\n");
            //Status = AtaXSendAtaInquiry(AtaXChannelFdoExtension, Srb);
            ASSERT(FALSE);
            Status = STATUS_NOT_SUPPORTED;
            return Status;
          }

        case SCSIOP_MODE_SELECT:        /* 0x15 */
          DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_EXECUTE_SCSI / SCSIOP_MODE_SELECT FIXME\n");
ASSERT(FALSE);

        case SCSIOP_MODE_SELECT10:      /* 0x55 */
          DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_EXECUTE_SCSI / SCSIOP_MODE_SELECT10 (PIO mode) \n");
ASSERT(FALSE);

        case SCSIOP_MODE_SENSE:         /* 0x1A */
          DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_EXECUTE_SCSI / SCSIOP_MODE_SENSE FIXME AhciModeSense\n");
ASSERT(FALSE);

        case SCSIOP_MODE_SENSE10:       /* 0x5A */
          DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_EXECUTE_SCSI / SCSIOP_MODE_SENSE10 (PIO mode) \n");
ASSERT(FALSE);

        case SCSIOP_READ_CAPACITY:      /* 0x25 */
          DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_EXECUTE_SCSI / SCSIOP_READ_CAPACITY\n");
ASSERT(FALSE);

        case SCSIOP_READ_TOC:           /* 0x43 */
          DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_EXECUTE_SCSI / SCSIOP_READ_TOC\n");
          break;  // --> IoStartPacket

        case SCSIOP_GET_CONFIGURATION:  /* 0x46 */
          DPRINT("IRP_MJ_SCSI / SCSIOP_GET_CONFIGURATION (PIO mode)\n");
          Srb->SrbFlags &= ~SRB_FLAGS_USE_DMA;  // PIO mode
          break; // --> IoStartPacket

        case SCSIOP_GET_EVENT_STATUS:   /* 0x4A */
          DPRINT("IRP_MJ_SCSI / SCSIOP_GET_EVENT_STATUS (PIO mode) FIXME\n");
          Srb->SrbFlags &= ~SRB_FLAGS_USE_DMA;  // PIO mode
          break; // --> IoStartPacket

        default:
          DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_EXECUTE_SCSI / FIXME SCSIOP_ - %x\n", (UCHAR)Srb->Cdb[0]);
          break;  // --> IoStartPacket
      }

      IoMarkIrpPending(Irp);  // mark IRP as pending in all cases

      if ( Srb->SrbFlags & SRB_FLAGS_BYPASS_FROZEN_QUEUE )
      {
        DPRINT(" AtaXDevicePdoDispatchScsi: SRB_FLAGS_BYPASS_FROZEN_QUEUE\n");
      }

      if ( !(Srb->SrbFlags & SRB_FLAGS_BYPASS_FROZEN_QUEUE) )
      {
        QueueIsNotEmpty = AtaXQueueAddIrp(AtaXDevicePdoExtension, Irp, Srb->QueueSortKey) == TRUE;
  
        if ( QueueIsNotEmpty )
        {
          DPRINT(" AtaXDevicePdoDispatchScsi: AtaXQueueAddIrp return TRUE (queue not empty). Return STATUS_PENDING\n");
          DPRINT("\n");
          return STATUS_PENDING;
        }
      }
 
      DPRINT(" AtaXDevicePdoDispatchScsi: device queue is empty  or bypass frozen queue\n");
      /* Start IO directly */
      IoStartPacket(AtaXChannelFdoExtension->CommonExtension.SelfDevice, Irp, NULL, NULL);

      DPRINT(" AtaXDevicePdoDispatchScsi: return STATUS_PENDING\n");
      DPRINT("\n");
      return STATUS_PENDING;

    case SRB_FUNCTION_CLAIM_DEVICE:           /* 0x01 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_CLAIM_DEVICE \n");
ASSERT(FALSE);
      Status = STATUS_SUCCESS;
      break;

    case SRB_FUNCTION_IO_CONTROL:             /* 0x02 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_IO_CONTROL\n");
      Status = STATUS_NOT_SUPPORTED;
      Srb->SrbStatus = SRB_STATUS_ERROR;
      break;

    case SRB_FUNCTION_RECEIVE_EVENT:          /* 0x03 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_RECEIVE_EVENT FIXME\n");
      Status = STATUS_NOT_SUPPORTED;
      Srb->SrbStatus = SRB_STATUS_ERROR;
      break;

    case SRB_FUNCTION_RELEASE_QUEUE:          /* 0x04 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_RELEASE_QUEUE FIXME\n");
      Status = STATUS_NOT_SUPPORTED;
      Srb->SrbStatus = SRB_STATUS_ERROR;
      break;

    case SRB_FUNCTION_ATTACH_DEVICE:          /* 0x05 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_ATTACH_DEVICE FIXME\n");
      Status = STATUS_NOT_SUPPORTED;
      Srb->SrbStatus = SRB_STATUS_ERROR;
      break;

    case SRB_FUNCTION_RELEASE_DEVICE:         /* 0x06 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_RELEASE_DEVICE\n");
ASSERT(FALSE);
      Status = STATUS_SUCCESS;
      break;

    case SRB_FUNCTION_SHUTDOWN:               /* 0x07 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_SHUTDOWN FIXME\n");
      Status = STATUS_NOT_SUPPORTED;
      Srb->SrbStatus = SRB_STATUS_ERROR;
      break;

    case SRB_FUNCTION_FLUSH:                  /* 0x08 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_FLUSH FIXME\n");
      Status = STATUS_NOT_SUPPORTED;
      Srb->SrbStatus = SRB_STATUS_ERROR;
      break;

    case SRB_FUNCTION_ABORT_COMMAND:          /* 0x10 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_ABORT_COMMAND FIXME\n");
      Status = STATUS_NOT_SUPPORTED;
      Srb->SrbStatus = SRB_STATUS_ERROR;
      break;

    case SRB_FUNCTION_RELEASE_RECOVERY:       /* 0x11 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_RELEASE_RECOVERY FIXME\n");
      Status = STATUS_NOT_SUPPORTED;
      Srb->SrbStatus = SRB_STATUS_ERROR;
      break;

    case SRB_FUNCTION_RESET_BUS:              /* 0x12 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_RESET_BUS FIXME\n");
      Status = STATUS_NOT_SUPPORTED;
      Srb->SrbStatus = SRB_STATUS_ERROR;
      break;

    case SRB_FUNCTION_RESET_DEVICE:           /* 0x13 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_RESET_DEVICE FIXME\n");
      Status = STATUS_NOT_SUPPORTED;
      Srb->SrbStatus = SRB_STATUS_ERROR;
      break;

    case SRB_FUNCTION_TERMINATE_IO:           /* 0x14 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_TERMINATE_IO FIXME\n");
      Status = STATUS_NOT_SUPPORTED;
      Srb->SrbStatus = SRB_STATUS_ERROR;
      break;

    case SRB_FUNCTION_FLUSH_QUEUE:            /* 0x15 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_FLUSH_QUEUE \n");
ASSERT(FALSE);

    case SRB_FUNCTION_REMOVE_DEVICE:          /* 0x16 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_REMOVE_DEVICE FIXME\n");
      Status = STATUS_NOT_SUPPORTED;
      Srb->SrbStatus = SRB_STATUS_ERROR;
      break;

    case SRB_FUNCTION_WMI:                    /* 0x17 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_WMI FIXME\n");
      Status = STATUS_NOT_SUPPORTED;
      Srb->SrbStatus = SRB_STATUS_ERROR;
      break;

    case SRB_FUNCTION_LOCK_QUEUE:             /* 0x18 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_LOCK_QUEUE FIXME\n");
      Status = STATUS_NOT_SUPPORTED;
      Srb->SrbStatus = SRB_STATUS_ERROR;
      break;

    case SRB_FUNCTION_UNLOCK_QUEUE:           /* 0x19 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_UNLOCK_QUEUE FIXME\n");
      Status = STATUS_NOT_SUPPORTED;
      Srb->SrbStatus = SRB_STATUS_ERROR;
      break;

    case SRB_FUNCTION_RESET_LOGICAL_UNIT:     /* 0x20 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_RESET_LOGICAL_UNIT FIXME\n");
      Status = STATUS_NOT_SUPPORTED;
      Srb->SrbStatus = SRB_STATUS_ERROR;
      break;

    case SRB_FUNCTION_SET_LINK_TIMEOUT:       /* 0x21 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_SET_LINK_TIMEOUT FIXME\n");
      Status = STATUS_NOT_SUPPORTED;
      Srb->SrbStatus = SRB_STATUS_ERROR;
      break;

    case SRB_FUNCTION_LINK_TIMEOUT_OCCURRED:  /* 0x22 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_LINK_TIMEOUT_OCCURRED FIXME\n");
      Status = STATUS_NOT_SUPPORTED;
      Srb->SrbStatus = SRB_STATUS_ERROR;
      break;

    case SRB_FUNCTION_LINK_TIMEOUT_COMPLETE:  /* 0x23 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_LINK_TIMEOUT_COMPLETE FIXME\n");
      Status = STATUS_NOT_SUPPORTED;
      Srb->SrbStatus = SRB_STATUS_ERROR;
      break;

    case SRB_FUNCTION_POWER:                  /* 0x24 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_POWER FIXME\n");
      Status = STATUS_NOT_SUPPORTED;
      Srb->SrbStatus = SRB_STATUS_ERROR;
      break;

    case SRB_FUNCTION_PNP:                    /* 0x25 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_PNP FIXME\n");
      Status = STATUS_NOT_SUPPORTED;
      Srb->SrbStatus = SRB_STATUS_ERROR;
      break;

    case SRB_FUNCTION_DUMP_POINTERS:          /* 0x26 */
      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_DUMP_POINTERS FIXME\n");
      Status = STATUS_NOT_SUPPORTED;
      Srb->SrbStatus = SRB_STATUS_ERROR;
      break;

    default:
      DPRINT("IRP_MJ_SCSI / Unknown SRB function FIXME%x\n", Srb->Function);
      ASSERT(FALSE);
      Status = STATUS_NOT_SUPPORTED;
      Srb->SrbStatus = SRB_STATUS_ERROR;
      break;
  }

  DPRINT("AtaXDevicePdoDispatchScsi: return - %x\n", Status);
  DPRINT("\n");

  Irp->IoStatus.Status = Status;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return Status;
}

NTSTATUS
AtaXDevicePdoDispatchPnp(
    IN PDEVICE_OBJECT AtaXDevicePdo,
    IN PIRP Irp)
{
  PIO_STACK_LOCATION    Stack;
  ULONG                 MinorFunction;
  PDEVICE_CAPABILITIES  DeviceCapabilities;
  ULONG                 ix;
  NTSTATUS              Status;
  
  Stack = IoGetCurrentIrpStackLocation(Irp);
  MinorFunction = Stack->MinorFunction;

  switch ( MinorFunction )
  {
    case IRP_MN_START_DEVICE:                 /* 0x00 */  //AtaXDevicePdoStartDevice
      DPRINT("IRP_MJ_PNP / IRP_MN_START_DEVICE\n");
ASSERT(FALSE);
      Status = 0;//AtaXDevicePdoStartDevice(AtaXDevicePdo, Irp);
      break;

    case IRP_MN_QUERY_REMOVE_DEVICE:          /* 0x01 */
    case IRP_MN_QUERY_STOP_DEVICE:            /* 0x05 */  //AtaXDevicePdoQueryStopRemoveDevice
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_REMOVE(STOP)_DEVICE\n");
      Irp->IoStatus.Status = 0;
ASSERT(FALSE);
      return 0;//AtaXDevicePdoQueryStopRemoveDevice(AtaXDevicePdo, Irp);

    case IRP_MN_REMOVE_DEVICE:                /* 0x02 */
    case IRP_MN_SURPRISE_REMOVAL:             /* 0x17 */  //AtaXDevicePdoRemoveDevice
      DPRINT("IRP_MJ_PNP / IRP_MN_REMOVE_DEVICE\n");
ASSERT(FALSE);
      Status = 0;//AtaXDevicePdoRemoveDevice(AtaXDevicePdo, Irp);
      break;

    case IRP_MN_STOP_DEVICE:                  /* 0x04 */  //AtaXDevicePdoStopDevice
      DPRINT("IRP_MJ_PNP / IRP_MN_STOP_DEVICE\n");
ASSERT(FALSE);
      Status = 0;//AtaXDevicePdoStopDevice(AtaXDevicePdo, Irp);
      break;

    case IRP_MN_CANCEL_REMOVE_DEVICE:         /* 0x03 */
    case IRP_MN_CANCEL_STOP_DEVICE:           /* 0x06 */
      DPRINT("IRP_MJ_PNP / IRP_MN_CANCEL_REMOVE(STOP)_DEVICE\n");
      Irp->IoStatus.Status = 0;
      break;

    case IRP_MN_QUERY_DEVICE_RELATIONS:       /* 0x07 */  //AtaXDevicePdoQueryDeviceRelations
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_DEVICE_RELATIONS\n");
ASSERT(FALSE);
      Status = 0;//AtaXDevicePdoQueryDeviceRelations(AtaXDevicePdo, Irp);
      break;

    case IRP_MN_QUERY_CAPABILITIES:           /* 0x09 */  //AtaXDevicePdoQueryCapabilities
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
      DeviceCapabilities->HardwareDisabled  = FALSE;            /* FIXME */
      //DeviceCapabilities->NoDisplayInUI   = FALSE;            /* FIXME */
      DeviceCapabilities->DeviceState[0]    = PowerDeviceD0;    /* FIXME */

      for (ix = 0; ix < PowerSystemMaximum; ix++)
      	DeviceCapabilities->DeviceState[ix] = PowerDeviceD3;    /* FIXME */
      //DeviceCapabilities->DeviceWake = PowerDeviceUndefined;  /* FIXME */

      DeviceCapabilities->D1Latency = 0;                        /* FIXME */
      DeviceCapabilities->D2Latency = 0;                        /* FIXME */
      DeviceCapabilities->D3Latency = 0;                        /* FIXME */

      Status = STATUS_SUCCESS;
      break;

    case IRP_MN_QUERY_DEVICE_TEXT:            /* 0x0C */  //AtaXDevicePdoQueryDeviceText
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_DEVICE_TEXT\n");
ASSERT(FALSE);
      Status = 0;//AtaXDevicePdoQueryDeviceText(AtaXDevicePdo, Irp);
      break;

    case IRP_MN_QUERY_ID:                     /* 0x13 */  //AtaXDevicePdoQueryId
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_ID\n");
ASSERT(FALSE);
      Status = 0;//AtaXDevicePdoQueryId(AtaXDevicePdo, Irp);
      break;

    case IRP_MN_QUERY_PNP_DEVICE_STATE:       /* 0x14 */  //AtaXDevicePdoQueryPnPDeviceState
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_PNP_DEVICE_STATE\n");
ASSERT(FALSE);
      Status = 0;//AtaXDevicePdoQueryPnPDeviceState(AtaXDevicePdo, Irp);
      break;

    case IRP_MN_DEVICE_USAGE_NOTIFICATION:    /* 0x16 */  //AtaXDevicePdoUsageNotification
      DPRINT("IRP_MJ_PNP / IRP_MN_DEVICE_USAGE_NOTIFICATION\n");
ASSERT(FALSE);
      Status = 0;//AtaXDevicePdoUsageNotification(AtaXDevicePdo, Irp);
      break;

    default:
      DPRINT("IRP_MJ_PNP / Unknown minor function 0x%lx\n", MinorFunction);
      IoCompleteRequest(Irp, IO_NO_INCREMENT);
      return Irp->IoStatus.Status;
  }

  Irp->IoStatus.Status = Status;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return Status;
}
