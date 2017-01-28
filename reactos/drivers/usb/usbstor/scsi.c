/*
 * PROJECT:     ReactOS Universal Serial Bus Bulk Storage Driver
 * LICENSE:     GPL - See COPYING in the top level directory
 * FILE:        drivers/usb/usbstor/pdo.c
 * PURPOSE:     USB block storage device driver.
 * PROGRAMMERS:
 *              James Tabor
 *              Michael Martin (michael.martin@reactos.org)
 *              Johannes Anderwald (johannes.anderwald@reactos.org)
 */

#include "usbstor.h"

#define NDEBUG
#include <debug.h>

VOID
DumpCBW(
    PUCHAR Block)
{
    DPRINT("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
        Block[0] & 0xFF, Block[1] & 0xFF, Block[2] & 0xFF, Block[3] & 0xFF, Block[4] & 0xFF, Block[5] & 0xFF, Block[6] & 0xFF, Block[7] & 0xFF, Block[8] & 0xFF, Block[9] & 0xFF,
        Block[10] & 0xFF, Block[11] & 0xFF, Block[12] & 0xFF, Block[13] & 0xFF, Block[14] & 0xFF, Block[15] & 0xFF, Block[16] & 0xFF, Block[17] & 0xFF, Block[18] & 0xFF, Block[19] & 0xFF,
        Block[20] & 0xFF, Block[21] & 0xFF, Block[22] & 0xFF, Block[23] & 0xFF, Block[24] & 0xFF, Block[25] & 0xFF, Block[26] & 0xFF, Block[27] & 0xFF, Block[28] & 0xFF, Block[29] & 0xFF,
        Block[30] & 0xFF);

}

NTSTATUS
USBSTOR_SendModeSense(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG RetryCount)
{
#if 0
    UFI_SENSE_CMD Cmd;
    NTSTATUS Status;
    PVOID Response;
    PCBW OutControl;
    PCDB pCDB;
    PUFI_MODE_PARAMETER_HEADER Header;
#endif
    PPDO_DEVICE_EXTENSION PDODeviceExtension;
    PIO_STACK_LOCATION IoStack;
    PSCSI_REQUEST_BLOCK Request;

    //
    // get PDO device extension
    //
    PDODeviceExtension = (PPDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    //
    // sanity check
    //
    ASSERT(PDODeviceExtension->Common.IsFDO == FALSE);

    //
    // get current stack location
    //
    IoStack = IoGetCurrentIrpStackLocation(Irp);

    //
    // get request block
    //
    Request = IoStack->Parameters.Scsi.Srb;

    RtlZeroMemory(Request->DataBuffer, Request->DataTransferLength);
    Request->SrbStatus = SRB_STATUS_SUCCESS;
    Irp->IoStatus.Information = Request->DataTransferLength;
    Irp->IoStatus.Status = STATUS_SUCCESS;
    USBSTOR_QueueTerminateRequest(PDODeviceExtension->LowerDeviceObject, Irp);
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    //
    // start next request
    //
    USBSTOR_QueueNextRequest(PDODeviceExtension->LowerDeviceObject);

    return STATUS_SUCCESS;

#if 0
    //
    // get SCSI command data block
    //
    pCDB = (PCDB)Request->Cdb;

    //
    // get PDO device extension
    //
    PDODeviceExtension = (PPDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    //
    // allocate sense response from non paged pool
    //
    Response = (PUFI_CAPACITY_RESPONSE)AllocateItem(NonPagedPool, Request->DataTransferLength);
    if (!Response)
    {
        //
        // no memory
        //
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // sanity check
    //


    // Supported pages
    // MODE_PAGE_ERROR_RECOVERY
    // MODE_PAGE_FLEXIBILE
    // MODE_PAGE_LUN_MAPPING
    // MODE_PAGE_FAULT_REPORTING
    // MODE_SENSE_RETURN_ALL

    //
    // initialize mode sense cmd
    //
    RtlZeroMemory(&Cmd, sizeof(UFI_INQUIRY_CMD));
    Cmd.Code = SCSIOP_MODE_SENSE;
    Cmd.LUN = (PDODeviceExtension->LUN & MAX_LUN);
    Cmd.PageCode = pCDB->MODE_SENSE.PageCode;
    Cmd.PC = pCDB->MODE_SENSE.Pc;
    Cmd.AllocationLength = HTONS(pCDB->MODE_SENSE.AllocationLength);

    DPRINT1("PageCode %x\n", pCDB->MODE_SENSE.PageCode);
    DPRINT1("PC %x\n", pCDB->MODE_SENSE.Pc);

    //
    // now send mode sense cmd
    //
    Status = USBSTOR_SendCBW(DeviceObject, UFI_SENSE_CMD_LEN, (PUCHAR)&Cmd, Request->DataTransferLength, &OutControl);
    if (!NT_SUCCESS(Status))
    {
        //
        // failed to send CBW
        //
        DPRINT1("USBSTOR_SendCapacityCmd> USBSTOR_SendCBW failed with %x\n", Status);
        FreeItem(Response);
        ASSERT(FALSE);
        return Status;
    }

    //
    // now send data block response
    //
    Status = USBSTOR_SendData(DeviceObject, Request->DataTransferLength, Response);
    if (!NT_SUCCESS(Status))
    {
        //
        // failed to send CBW
        //
        DPRINT1("USBSTOR_SendCapacityCmd> USBSTOR_SendData failed with %x\n", Status);
        FreeItem(Response);
        ASSERT(FALSE);
        return Status;
    }

    Header = (PUFI_MODE_PARAMETER_HEADER)Response;

    //
    // TODO: build layout
    //
    // first struct is the header
    // MODE_PARAMETER_HEADER / _MODE_PARAMETER_HEADER10
    //
    // followed by
    // MODE_PARAMETER_BLOCK
    //
    //
    UNIMPLEMENTED

    //
    // send csw
    //
    Status = USBSTOR_SendCSW(DeviceObject, OutControl, 512, &CSW);

    DPRINT1("------------------------\n");
    DPRINT1("CSW %p\n", &CSW);
    DPRINT1("Signature %x\n", CSW.Signature);
    DPRINT1("Tag %x\n", CSW.Tag);
    DPRINT1("DataResidue %x\n", CSW.DataResidue);
    DPRINT1("Status %x\n", CSW.Status);

    //
    // FIXME: handle error
    //
    ASSERT(CSW.Status == 0);
    ASSERT(CSW.DataResidue == 0);

    //
    // calculate transfer length
    //
    *TransferBufferLength = Request->DataTransferLength - CSW.DataResidue;

    //
    // copy buffer
    //
    RtlCopyMemory(Request->DataBuffer, Response, *TransferBufferLength);

    //
    // free item
    //
    FreeItem(OutControl);

    //
    // free response
    //
    FreeItem(Response);

    //
    // done
    //
    return Status;
#endif
}

NTSTATUS
NTAPI
USBSTOR_SrbStatusToNtStatus(
    IN PSCSI_REQUEST_BLOCK Srb)
{
    UCHAR SrbStatus;

    SrbStatus = SRB_STATUS(Srb->SrbStatus);

    DPRINT("USBSTOR_SrbStatusToNtStatus: Srb - %p, SrbStatus - %x\n",
           Srb,
           SrbStatus);

    switch (SrbStatus)
    {
        case SRB_STATUS_DATA_OVERRUN:
            return STATUS_BUFFER_OVERFLOW;

        case SRB_STATUS_BAD_FUNCTION:
        case SRB_STATUS_BAD_SRB_BLOCK_LENGTH:
            return STATUS_INVALID_DEVICE_REQUEST;

        case SRB_STATUS_INVALID_LUN:
        case SRB_STATUS_INVALID_TARGET_ID:
        case SRB_STATUS_NO_HBA:
        case SRB_STATUS_NO_DEVICE:
            return STATUS_DEVICE_DOES_NOT_EXIST;

        case SRB_STATUS_TIMEOUT:
            return STATUS_IO_TIMEOUT;

        case SRB_STATUS_BUS_RESET:
        case SRB_STATUS_COMMAND_TIMEOUT:
        case SRB_STATUS_SELECTION_TIMEOUT:
            return STATUS_DEVICE_NOT_CONNECTED;

        default:
            return STATUS_IO_DEVICE_ERROR;
    }
}

NTSTATUS
NTAPI
USBSTOR_IssueBulkOrInterruptRequest(
    IN PFDO_DEVICE_EXTENSION FDODeviceExtension,
    IN PIRP Irp,
    IN USBD_PIPE_HANDLE PipeHandle,
    IN ULONG TransferFlags,
    IN ULONG TransferBufferLength,
    IN PVOID TransferBuffer,
    IN PMDL TransferBufferMDL,
    IN PIO_COMPLETION_ROUTINE CompletionRoutine,
    IN PVOID Context)
{
    PIO_STACK_LOCATION NextStack;
    KIRQL OldIrql;

    DPRINT("USBSTOR_IssueBulkOrInterruptRequest: ... \n");

    RtlZeroMemory(&FDODeviceExtension->Urb,
                  sizeof (struct _URB_BULK_OR_INTERRUPT_TRANSFER));

    FDODeviceExtension->Urb.Hdr.Length = sizeof (struct _URB_BULK_OR_INTERRUPT_TRANSFER);
    FDODeviceExtension->Urb.Hdr.Function = URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER;

    FDODeviceExtension->Urb.PipeHandle = PipeHandle;
    FDODeviceExtension->Urb.TransferFlags = TransferFlags;
    FDODeviceExtension->Urb.TransferBufferLength = TransferBufferLength;
    FDODeviceExtension->Urb.TransferBuffer = TransferBuffer;
    FDODeviceExtension->Urb.TransferBufferMDL = TransferBufferMDL;

    NextStack = IoGetNextIrpStackLocation(Irp); 
    NextStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL; 
    NextStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_INTERNAL_USB_SUBMIT_URB; 
    NextStack->Parameters.Others.Argument1 = &FDODeviceExtension->Urb; 

    IoSetCompletionRoutine(Irp,
                           CompletionRoutine,
                           Context,
                           TRUE,
                           TRUE,
                           TRUE); 

    KeAcquireSpinLock(&FDODeviceExtension->StorSpinLock, &OldIrql);
    FDODeviceExtension->Flags |= USBSTOR_FDO_FLAGS_TRANSFER_FINISHED;
    FDODeviceExtension->CurrentIrp = Irp;
    KeReleaseSpinLock(&FDODeviceExtension->StorSpinLock, OldIrql);

    return IoCallDriver(FDODeviceExtension->LowerDeviceObject, Irp);
}

NTSTATUS
NTAPI
USBSTOR_CswTransfer(
    IN PFDO_DEVICE_EXTENSION FDODeviceExtension,
    IN PIRP Irp)
{
    PVOID TransferBuffer;
    PUSBD_PIPE_INFORMATION PipeIn;
    ULONG TransferBufferLength;
    ULONG TransferFlags;
    UCHAR PipeIndex;

    DPRINT("USBSTOR_CswTransfer: ... \n");

    TransferBuffer = &FDODeviceExtension->BulkBuffer.Csw;

    PipeIndex = FDODeviceExtension->BulkInPipeIndex;
    PipeIn = &FDODeviceExtension->InterfaceInformation->Pipes[PipeIndex];

    if (PipeIn->MaximumPacketSize == 512)
    {
        TransferFlags = USBD_SHORT_TRANSFER_OK;
        TransferBufferLength = 512;
    }
    else
    {
        TransferFlags = 0;
        TransferBufferLength = sizeof(CSW);
    }

    return USBSTOR_IssueBulkOrInterruptRequest(FDODeviceExtension,
                                               Irp,
                                               PipeIn->PipeHandle,
                                               TransferFlags,
                                               TransferBufferLength,
                                               TransferBuffer,
                                               NULL,//TransferBufferMDL
                                               USBSTOR_CswCompletion,
                                               NULL);//Context
}

NTSTATUS
NTAPI
USBSTOR_DataCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context)
{
    PPDO_DEVICE_EXTENSION PDODeviceExtension;
    PSCSI_REQUEST_BLOCK Srb;
    PDEVICE_OBJECT FdoDevice;
    PFDO_DEVICE_EXTENSION FDODeviceExtension;
    PSCSI_REQUEST_BLOCK CurrentSrb;
    PIO_STACK_LOCATION IoStack;
    NTSTATUS Status;

    DPRINT("USBSTOR_DataCompletion: ... \n");

    PDODeviceExtension = (PPDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    FdoDevice = PDODeviceExtension->LowerDeviceObject;
    FDODeviceExtension = (PFDO_DEVICE_EXTENSION)FdoDevice->DeviceExtension;

    IoStack = Irp->Tail.Overlay.CurrentStackLocation;
    Srb = IoStack->Parameters.Scsi.Srb;

    if (Srb == FDODeviceExtension->CurrentSrb &&
        FDODeviceExtension->Urb.TransferBufferMDL != Irp->MdlAddress)
    {
        IoFreeMdl(FDODeviceExtension->Urb.TransferBufferMDL);
    }

    if (USBSTOR_IsRequestTimeOut(FdoDevice, Irp, &Status))
    {
        return Status;
    }

    if (NT_SUCCESS(Irp->IoStatus.Status))
    {
        if (FDODeviceExtension->Urb.TransferBufferLength < Srb->DataTransferLength)
        {
            Srb->SrbStatus = SRB_STATUS_DATA_OVERRUN;
        }
        else
        {
            Srb->SrbStatus = SRB_STATUS_SUCCESS;
        }

        Srb->DataTransferLength = FDODeviceExtension->Urb.TransferBufferLength;

        USBSTOR_CswTransfer(FDODeviceExtension, Irp);

        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    if (USBD_STATUS(FDODeviceExtension->Urb.Hdr.Status) == 
        USBD_STATUS(USBD_STATUS_STALL_PID))
    {
        ++FDODeviceExtension->RetryCount;

        Srb->DataTransferLength = FDODeviceExtension->Urb.TransferBufferLength;
        Srb->SrbStatus = SRB_STATUS_DATA_OVERRUN;

        USBSTOR_BulkQueueResetPipe(FDODeviceExtension);

        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    CurrentSrb = FDODeviceExtension->CurrentSrb;
    IoStack->Parameters.Scsi.Srb = CurrentSrb;

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = STATUS_IO_DEVICE_ERROR;

    CurrentSrb->SrbStatus = SRB_STATUS_BUS_RESET;

    USBSTOR_HandleCDBComplete(FDODeviceExtension, Irp, CurrentSrb);
    USBSTOR_QueueResetDevice(FDODeviceExtension, PDODeviceExtension);

    return STATUS_IO_DEVICE_ERROR;
}

NTSTATUS
NTAPI
USBSTOR_DataTransfer(
    IN PFDO_DEVICE_EXTENSION FDODeviceExtension,
    IN PIRP Irp)
{
    PSCSI_REQUEST_BLOCK Srb;
    PUSBD_PIPE_INFORMATION Pipe;
    PMDL Mdl;
    ULONG TransferFlags;
    PVOID TransferBuffer;
    PIO_STACK_LOCATION IoStack;
    NTSTATUS Status;
    UCHAR PipeIndex;

    DPRINT("USBSTOR_DataTransfer: ... \n");

    IoStack = Irp->Tail.Overlay.CurrentStackLocation;
    Srb = IoStack->Parameters.Scsi.Srb;

    if ((Srb->SrbFlags & SRB_FLAGS_UNSPECIFIED_DIRECTION) == SRB_FLAGS_DATA_IN)
    {
        PipeIndex = FDODeviceExtension->BulkInPipeIndex;
        TransferFlags = USBD_SHORT_TRANSFER_OK;
    }
    else if ((Srb->SrbFlags & SRB_FLAGS_UNSPECIFIED_DIRECTION) == SRB_FLAGS_DATA_OUT)
    {
        PipeIndex = FDODeviceExtension->BulkOutPipeIndex;
        TransferFlags = 0;
    }
    else
    {
        return STATUS_INVALID_PARAMETER;
    }

    Mdl = NULL;
    TransferBuffer = NULL;

    Pipe = &FDODeviceExtension->InterfaceInformation->Pipes[PipeIndex];

    if (Srb == FDODeviceExtension->CurrentSrb)
    {
        if (MmGetMdlVirtualAddress(Irp->MdlAddress) == Srb->DataBuffer)
        {
            Mdl = Irp->MdlAddress;
        }
        else
        {
            Mdl = IoAllocateMdl(Srb->DataBuffer,
                                Srb->DataTransferLength,
                                FALSE,
                                FALSE,
                                NULL);
    
            if (Mdl)
            {
                IoBuildPartialMdl(Irp->MdlAddress,
                                  Mdl,
                                  Srb->DataBuffer,
                                  Srb->DataTransferLength);
            }
            else
            {
                Status = STATUS_INSUFFICIENT_RESOURCES;
            }
        }

        if (!Mdl)
        {
            DPRINT("USBSTOR_DataTransfer: Mdl - %p\n", Mdl);
            return Status;
        }
    }
    else
    {
        TransferBuffer = Srb->DataBuffer;

        if (!Srb->DataBuffer)
        {
            DPRINT("USBSTOR_DataTransfer: Srb->DataBuffer == NULL!!!\n");
            return STATUS_INVALID_PARAMETER;
        }
    }

    USBSTOR_IssueBulkOrInterruptRequest(FDODeviceExtension,
                                        Irp,
                                        Pipe->PipeHandle,
                                        TransferFlags,
                                        Srb->DataTransferLength,
                                        TransferBuffer,
                                        Mdl,
                                        USBSTOR_DataCompletion,
                                        NULL);//Context

    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
USBSTOR_CbwCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context)
{
    PIO_STACK_LOCATION IoStack;
    PDEVICE_OBJECT FdoDevice;
    PSCSI_REQUEST_BLOCK CurrentSrb;
    PFDO_DEVICE_EXTENSION FDODeviceExtension;
    NTSTATUS Status;
    PSCSI_REQUEST_BLOCK Srb;
    PPDO_DEVICE_EXTENSION PDODeviceExtension;

    DPRINT("USBSTOR_CbwCompletion: ... \n");

    PDODeviceExtension = (PPDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    FdoDevice = PDODeviceExtension->LowerDeviceObject;
    FDODeviceExtension = (PFDO_DEVICE_EXTENSION)FdoDevice->DeviceExtension;

    IoStack = Irp->Tail.Overlay.CurrentStackLocation;
    Srb = IoStack->Parameters.Scsi.Srb;

    if (USBSTOR_IsRequestTimeOut(FdoDevice, Irp, &Status))
    {
        return Status;
    }

    if (!NT_SUCCESS(Irp->IoStatus.Status))
    {
        CurrentSrb = FDODeviceExtension->CurrentSrb;
        IoStack->Parameters.Scsi.Srb = CurrentSrb;

        Irp->IoStatus.Status = STATUS_IO_DEVICE_ERROR;
        Irp->IoStatus.Information = 0;

        CurrentSrb->SrbStatus = SRB_STATUS_BUS_RESET;

        USBSTOR_HandleCDBComplete(FDODeviceExtension, Irp, CurrentSrb);
        USBSTOR_QueueResetDevice(FDODeviceExtension, PDODeviceExtension);

        return STATUS_IO_DEVICE_ERROR;
    }

    if (Irp->MdlAddress || Srb != FDODeviceExtension->CurrentSrb)
    {
        Status = USBSTOR_DataTransfer(FDODeviceExtension, Irp);

        if (!NT_SUCCESS(Status))
        {
            CurrentSrb = FDODeviceExtension->CurrentSrb;
            IoStack->Parameters.Scsi.Srb = CurrentSrb;

            Irp->IoStatus.Status = Status;
            Irp->IoStatus.Information = 0;

            CurrentSrb->SrbStatus = SRB_STATUS_ERROR;

            USBSTOR_HandleCDBComplete(FDODeviceExtension, Irp, CurrentSrb);
            USBSTOR_QueueResetDevice(FDODeviceExtension, PDODeviceExtension);

            return Status;
        }
    }
    else
    {
        Srb->SrbStatus = SRB_STATUS_SUCCESS;
        USBSTOR_CswTransfer(FDODeviceExtension, Irp);
    }

    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
NTAPI
USBSTOR_CbwTransfer(
    IN PFDO_DEVICE_EXTENSION FDODeviceExtension,
    IN PIRP Irp)
{
    PIO_STACK_LOCATION IoStack;
    PPDO_DEVICE_EXTENSION PDODeviceExtension;
    PSCSI_REQUEST_BLOCK Srb;
    PCBW Cbw;
    PUSBD_PIPE_INFORMATION Pipe;
    UCHAR PipeIndex;

    DPRINT("USBSTOR_CbwTransfer: ... \n");

    FDODeviceExtension->RetryCount = 0;

    IoStack = Irp->Tail.Overlay.CurrentStackLocation;
    PDODeviceExtension = IoStack->DeviceObject->DeviceExtension;
    Srb = IoStack->Parameters.Scsi.Srb;

    Cbw = &FDODeviceExtension->BulkBuffer.Cbw;

    Cbw->Signature = CBW_SIGNATURE;
    Cbw->Tag = (ULONG)Irp;
    Cbw->DataTransferLength = Srb->DataTransferLength;
    Cbw->Flags = ((UCHAR)Srb->SrbFlags & SRB_FLAGS_UNSPECIFIED_DIRECTION) << 1;
    Cbw->LUN = PDODeviceExtension->LUN;
    Cbw->CommandBlockLength = Srb->CdbLength;

    RtlCopyMemory(FDODeviceExtension->BulkBuffer.Cbw.CommandBlock,
                  Srb->Cdb,
                  sizeof(CDB));

    PipeIndex = FDODeviceExtension->BulkOutPipeIndex;
    Pipe = &FDODeviceExtension->InterfaceInformation->Pipes[PipeIndex];

    return USBSTOR_IssueBulkOrInterruptRequest(FDODeviceExtension,
                                               Irp,
                                               Pipe->PipeHandle,
                                               0,//TransferFlags
                                               sizeof(CBW),
                                               &FDODeviceExtension->BulkBuffer.Cbw,
                                               NULL,//TransferBufferMDL
                                               USBSTOR_CbwCompletion,
                                               NULL);//Context
}

VOID
USBSTOR_HandleExecuteSCSI(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG RetryCount)
{
    PCDB pCDB;
    NTSTATUS Status;
    PIO_STACK_LOCATION IoStack;
    PSCSI_REQUEST_BLOCK Srb;
    PPDO_DEVICE_EXTENSION PDODeviceExtension;
    PFDO_DEVICE_EXTENSION FDODeviceExtension;

    //
    // get PDO device extension
    //
    PDODeviceExtension = (PPDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    FDODeviceExtension = PDODeviceExtension->LowerDeviceObject->DeviceExtension;

    //
    // sanity check
    //
    ASSERT(PDODeviceExtension->Common.IsFDO == FALSE);

    //
    // get current stack location
    //
    IoStack = IoGetCurrentIrpStackLocation(Irp);

    //
    // get request block
    //
    Srb = IoStack->Parameters.Scsi.Srb;

    //
    // get SCSI command data block
    //
    pCDB = (PCDB)Srb->Cdb;

    DPRINT("USBSTOR_HandleExecuteSCSI Operation Code %x\n", pCDB->AsByte[0]);


    if (pCDB->MODE_SENSE.OperationCode == SCSIOP_MODE_SENSE)
    {
        //FIXME CheckModeSenseDataProtect

        DPRINT("SCSIOP_MODE_SENSE DataTransferLength %lu\n", Srb->DataTransferLength);
        ASSERT(pCDB->MODE_SENSE.AllocationLength == Srb->DataTransferLength);
        ASSERT(Srb->DataBuffer);

        //
        // send mode sense command
        //
        Status = USBSTOR_SendModeSense(DeviceObject, Irp, RetryCount);
        return;
    }

    if (FDODeviceExtension->DriverFlags == 1)
    {
        Status = USBSTOR_CbwTransfer(FDODeviceExtension, Irp);
        DPRINT("USBSTOR_HandleExecuteSCSI: Status - %08X\n", Status);
        return;
    }

    DPRINT1("USBSTOR_HandleExecuteSCSI: Not BulkOnly. UNIMPLEMENTED. FIXME. \n");

    ASSERT(FALSE);
}