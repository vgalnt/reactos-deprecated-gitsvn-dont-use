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
    Request = (PSCSI_REQUEST_BLOCK)IoStack->Parameters.Others.Argument1;

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
USBSTOR_CbwTransfer(
    IN PFDO_DEVICE_EXTENSION FDODeviceExtension,
    IN PIRP Irp)
{
    DPRINT1("USBSTOR_CbwTransfer: UNIMPLEMENTED. FIXME. \n");
    return 0;
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
