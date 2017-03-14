/*
 * PROJECT:     ReactOS Universal Serial Bus Human Interface Device Driver
 * LICENSE:     GPL - See COPYING in the top level directory
 * FILE:        drivers/hid/hidclass/hidclass.c
 * PURPOSE:     HID Class Driver
 * PROGRAMMERS:
 *              Michael Martin (michael.martin@reactos.org)
 *              Johannes Anderwald (johannes.anderwald@reactos.org)
 */

#include "precomp.h"

#define NDEBUG
#include <debug.h>

static LPWSTR ClientIdentificationAddress = L"HIDCLASS";
static ULONG HidClassDeviceNumber;
LIST_ENTRY DriverExtList;
FAST_MUTEX DriverExtListMutex;

NTSTATUS
NTAPI
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath)
{
    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
DllInitialize(
    IN PUNICODE_STRING RegistryPath)
{
    InitializeListHead(&DriverExtList);
    ExInitializeFastMutex(&DriverExtListMutex);
    HidClassDeviceNumber = 0;

    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
DllUnload(VOID)
{
    return STATUS_SUCCESS;
}

PHIDCLASS_DRIVER_EXTENSION
NTAPI
RefDriverExt(
    IN PDRIVER_OBJECT DriverObject)
{
    PHIDCLASS_DRIVER_EXTENSION DriverExtension = NULL;
    PLIST_ENTRY Entry;
    PDRIVER_OBJECT driverObject;
    PHIDCLASS_DRIVER_EXTENSION driverExtension;

    DPRINT("RefDriverExt: DriverObject - %p\n", DriverObject);

    /* increments the given driver object extension's reference count */

    ExAcquireFastMutex(&DriverExtListMutex);

    Entry = DriverExtList.Flink;

    if (!IsListEmpty(&DriverExtList))
    {
        driverExtension = CONTAINING_RECORD(Entry,
                                            HIDCLASS_DRIVER_EXTENSION,
                                            DriverExtLink.Flink);

        driverObject = driverExtension->DriverObject;

        while (driverObject != DriverObject)
        {
            Entry = Entry->Flink;

            if (Entry == &DriverExtList)
            {
                goto Exit;
            }
        }

        DriverExtension = CONTAINING_RECORD(Entry,
                                            HIDCLASS_DRIVER_EXTENSION,
                                            DriverExtLink.Flink);

        ++DriverExtension->RefCount;
    }

Exit:
    ExReleaseFastMutex(&DriverExtListMutex);
    DPRINT("RefDriverExt: return DriverExtension - %p\n", DriverExtension);
    return DriverExtension;
}

PHIDCLASS_DRIVER_EXTENSION
NTAPI
DerefDriverExt(
    IN PDRIVER_OBJECT DriverObject)
{
    PHIDCLASS_DRIVER_EXTENSION Result = NULL;
    PLIST_ENTRY Entry;
    PHIDCLASS_DRIVER_EXTENSION DriverExtension;
    BOOLEAN IsRemoveEntry;

    DPRINT("DerefDriverExt: DriverObject - %p\n", DriverObject);

    /* decrements the given driver object extension's reference count */

    ExAcquireFastMutex(&DriverExtListMutex);

    Entry = DriverExtList.Flink;

    if (!IsListEmpty(&DriverExtList))
    {
        while (TRUE)
        {
            DriverExtension = CONTAINING_RECORD(Entry,
                                                HIDCLASS_DRIVER_EXTENSION,
                                                DriverExtLink.Flink);

            if (DriverExtension->DriverObject == DriverObject)
            {
                break;
            }

            Entry = Entry->Flink;

            if (Entry == &DriverExtList)
            {
                goto Exit;
            }
        }

        --DriverExtension->RefCount;
        IsRemoveEntry = DriverExtension->RefCount < 0;

        /* if reference count < 0 then remove given driver object extension's link */
        if (IsRemoveEntry)
        {
            RemoveEntryList(Entry);
        }

        Result = DriverExtension;
    }

Exit:

    ExReleaseFastMutex(&DriverExtListMutex);
    DPRINT("DerefDriverExt: Result - %p\n", Result);
    return Result;
}

NTSTATUS
NTAPI
HidClassAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject)
{
    WCHAR CharDeviceName[64];
    NTSTATUS Status;
    UNICODE_STRING DeviceName;
    PDEVICE_OBJECT NewDeviceObject;
    PHIDCLASS_FDO_EXTENSION FDODeviceExtension;
    ULONG DeviceExtensionSize;
    PHIDCLASS_DRIVER_EXTENSION DriverExtension;
    PHID_DEVICE_EXTENSION HidDeviceExtension;

    /* Increment device number */
    InterlockedIncrement((PLONG)&HidClassDeviceNumber);

    /* Construct device name */
    swprintf(CharDeviceName, L"\\Device\\_HID%08x", HidClassDeviceNumber);

    /* Initialize device name */
    RtlInitUnicodeString(&DeviceName, CharDeviceName);

    /* Get driver object extension */
    DriverExtension = RefDriverExt(DriverObject);

    if (!DriverExtension)
    {
        /* Device removed */
        ASSERT(FALSE);
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    ASSERT(DriverObject == DriverExtension->DriverObject);

    /* Calculate device extension size */
    DeviceExtensionSize = sizeof(HIDCLASS_FDO_EXTENSION) +
                                 DriverExtension->DeviceExtensionSize;

    /* Now create the device */
    Status = IoCreateDevice(DriverObject,
                            DeviceExtensionSize,
                            &DeviceName,
                            FILE_DEVICE_UNKNOWN,
                            0,
                            FALSE,
                            &NewDeviceObject);

    if (!NT_SUCCESS(Status))
    {
        /* Failed to create device object */
        DPRINT1("IoCreateDevice failed. Status - %x", Status);
        ASSERT(FALSE);
        DerefDriverExt(DriverObject);
        return Status;
    }

    DPRINT("HidClassAddDevice: added FDO IoCreateDevice (%p)\n", NewDeviceObject);
    ObReferenceObject(NewDeviceObject);

    ASSERT(DriverObject->DeviceObject == NewDeviceObject);
    ASSERT(NewDeviceObject->DriverObject == DriverObject);

    /* Get device extension */
    FDODeviceExtension = NewDeviceObject->DeviceExtension;

    /* Zero device extension */
    RtlZeroMemory(FDODeviceExtension, DeviceExtensionSize);

    HidDeviceExtension = &FDODeviceExtension->Common.HidDeviceExtension;

    /* Initialize device extension */
    FDODeviceExtension->Common.IsFDO = TRUE;
    FDODeviceExtension->Common.DriverExtension = DriverExtension;
    HidDeviceExtension->PhysicalDeviceObject = PhysicalDeviceObject;
    FDODeviceExtension->OutstandingRequests = 0;

    /* Initialize FDO flags */
    FDODeviceExtension->IsNotifyPresence = TRUE;
    FDODeviceExtension->IsRelationsOn = TRUE;
    FDODeviceExtension->IsDeviceResourcesAlloceted = FALSE;

    /* Initialize SpinLocks */
    KeInitializeSpinLock(&FDODeviceExtension->HidRelationSpinLock);
    KeInitializeSpinLock(&FDODeviceExtension->HidRemoveDeviceSpinLock);

    /* Calculate and save pointer to minidriver-specific portion device extension */
    HidDeviceExtension->MiniDeviceExtension = (PVOID)((ULONG_PTR)FDODeviceExtension +
                                               sizeof(HIDCLASS_FDO_EXTENSION));

    ASSERT((PhysicalDeviceObject->Flags & DO_DEVICE_INITIALIZING) == 0);

    /* Attach new FDO to stack */
    HidDeviceExtension->NextDeviceObject = IoAttachDeviceToDeviceStack(NewDeviceObject,
                                                                       PhysicalDeviceObject);

    if (HidDeviceExtension->NextDeviceObject == NULL)
    {
        DPRINT1("HidClassAddDevice: Attach failed. IoDeleteDevice (%p)\n",
                NewDeviceObject);

        IoDeleteDevice(NewDeviceObject);
        return STATUS_DEVICE_REMOVED;
    }

    /* Increment stack size */
    NewDeviceObject->StackSize++;

    /* FDO state is not initialized */
    FDODeviceExtension->HidFdoState = HIDCLASS_STATE_NOT_INIT;

    /* Init device object */
    NewDeviceObject->Flags |= DO_DIRECT_IO;
    NewDeviceObject->Flags |= PhysicalDeviceObject->Flags & DO_POWER_PAGABLE;
    NewDeviceObject->Flags  &= ~DO_DEVICE_INITIALIZING;

    /* Now call driver provided add device routine */
    ASSERT(DriverExtension->AddDevice != 0);
    Status = DriverExtension->AddDevice(DriverObject, NewDeviceObject);

    if (NT_SUCCESS(Status))
    {
        return STATUS_SUCCESS;
    }

    DPRINT1("HidClassAddDevice: Failed with %x, IoDeleteDevice(%p)\n",
            Status,
            NewDeviceObject);

    IoDetachDevice(HidDeviceExtension->NextDeviceObject);
    ObDereferenceObject(NewDeviceObject);
    IoDeleteDevice(NewDeviceObject);

    DerefDriverExt(DriverObject);

    return Status;
}

VOID
NTAPI
HidClassDriverUnload(
    IN PDRIVER_OBJECT DriverObject)
{
    PHIDCLASS_DRIVER_EXTENSION DriverExtension;

    DPRINT("HidClassDriverUnload: ... \n");

    DriverExtension = DerefDriverExt(DriverObject);
    DriverExtension->DriverUnload(DriverObject);
}

BOOLEAN
NTAPI
HidClassPrivilegeCheck(
    IN PIRP Irp)
{
    LUID PrivilegeValue;

    PrivilegeValue = RtlConvertLongToLuid(SE_TCB_PRIVILEGE);
    return SeSinglePrivilegeCheck(PrivilegeValue, Irp->RequestorMode);
}

NTSTATUS
NTAPI
HidClass_Create(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
    NTSTATUS Status;
    PIO_STACK_LOCATION IoStack;
    PHIDCLASS_COMMON_DEVICE_EXTENSION CommonDeviceExtension;
    PHIDCLASS_PDO_DEVICE_EXTENSION PDODeviceExtension;
    PHIDCLASS_FDO_EXTENSION FDODeviceExtension;
    PHIDCLASS_FILEOP_CONTEXT FileContext;
    PHIDCLASS_COLLECTION HidCollection;
    //ULONG SessionId;
    ULONG HidFdoState;
    ULONG HidPdoState;
    BOOLEAN IsPrivilege;
    KIRQL OldIrql;

    DPRINT("HidClass_Create: Irp - %p\n", Irp);

    Status = STATUS_SUCCESS;// IoGetRequestorSessionId(Irp, &SessionId); FIXME

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("HidClass_Create: unable to get requestor SessionId\n");
        goto Exit;
    }

    /* Get common extension */
    CommonDeviceExtension = DeviceObject->DeviceExtension;

    if (CommonDeviceExtension->IsFDO)
    {
        DPRINT1("HidClass_Create: only supported for PDO\n");
        Status = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    /* Get device extensions */
    PDODeviceExtension = DeviceObject->DeviceExtension;
    FDODeviceExtension = PDODeviceExtension->FDODeviceExtension;

    /* Get stack location */
    IoStack = IoGetCurrentIrpStackLocation(Irp);
    ASSERT(IoStack->FileObject);

    Irp->IoStatus.Information = 0;

    /* Get collection */
    HidCollection = GetHidclassCollection(FDODeviceExtension,
                                          PDODeviceExtension->CollectionNumber);

    if (!HidCollection)
    {
        DPRINT1("HidClass_Create: couldn't find collection\n");
        Status = STATUS_DEVICE_NOT_CONNECTED;
        goto Exit;
    }

    /* Check privilege */
    IsPrivilege = HidClassPrivilegeCheck(Irp);

    DPRINT("ShareAccess %x\n", IoStack->Parameters.Create.ShareAccess);
    DPRINT("Options %x\n", IoStack->Parameters.Create.Options);
    DPRINT("DesiredAccess %x\n", IoStack->Parameters.Create.SecurityContext->DesiredAccess);

    KeAcquireSpinLock(&HidCollection->CollectSpinLock, &OldIrql);

    /* Validate parameters */

    if (PDODeviceExtension->RestrictionsForAnyOpen ||
        (PDODeviceExtension->OpenCount && !IoStack->Parameters.Create.ShareAccess))
    {
        DPRINT1("HidClass_Create: STATUS_SHARING_VIOLATION \n");
        Status = STATUS_SHARING_VIOLATION;
        goto UnlockExit;
    }

    if (((IoStack->Parameters.Create.SecurityContext->DesiredAccess & FILE_READ_DATA) &&
          PDODeviceExtension->RestrictionsForRead) ||
        ((IoStack->Parameters.Create.SecurityContext->DesiredAccess & FILE_WRITE_DATA) &&
          PDODeviceExtension->RestrictionsForWrite))
    {
        DPRINT1("HidClass_Create: STATUS_SHARING_VIOLATION \n");
        Status = STATUS_SHARING_VIOLATION;
        goto UnlockExit;
    }

    if ((PDODeviceExtension->OpensForRead > 0 &&
         !(IoStack->Parameters.Create.ShareAccess & FILE_SHARE_READ)) ||
        (PDODeviceExtension->OpensForWrite > 0 &&
         !(IoStack->Parameters.Create.ShareAccess & FILE_SHARE_WRITE)))
    {
        DPRINT1("HidClass_Create: STATUS_SHARING_VIOLATION \n");
        Status = STATUS_SHARING_VIOLATION;
        goto UnlockExit;
    }

    if (IoStack->Parameters.Create.Options & 1) // FIXME const.
    {
        DPRINT1("HidClass_Create: STATUS_NOT_A_DIRECTORY \n");
        Status = STATUS_NOT_A_DIRECTORY;
        goto UnlockExit;
    }

    /* Get PnP states */
    HidFdoState = FDODeviceExtension->HidFdoState;
    HidPdoState = PDODeviceExtension->HidPdoState;

    DPRINT("HidClass_Create: HidFdoState - %p, HidPdoState - %p\n",
           HidFdoState,
           HidPdoState);

    /* Validate PnP states */
    if ((HidFdoState != HIDCLASS_STATE_STARTED &&
         HidFdoState != HIDCLASS_STATE_STOPPING &&
         HidFdoState != HIDCLASS_STATE_DISABLED) ||
        (HidPdoState != HIDCLASS_STATE_STARTED &&
         HidPdoState != HIDCLASS_STATE_FAILED &&
         HidPdoState != HIDCLASS_STATE_STOPPING))
    {
        DPRINT1("HidClass_Create: STATUS_DEVICE_NOT_CONNECTED \n");
        Status = STATUS_DEVICE_NOT_CONNECTED;
        goto UnlockExit;
    }

    /* Allocate file context */
    FileContext = ExAllocatePoolWithTag(NonPagedPool,
                                        sizeof(HIDCLASS_FILEOP_CONTEXT),
                                        HIDCLASS_TAG);

    if (!FileContext)
    {
        DPRINT1("HidClass_Create: Allocate context failed\n");
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto UnlockExit;
    }

    /* Initialize file context */

    RtlZeroMemory(FileContext, sizeof(HIDCLASS_FILEOP_CONTEXT));

    FileContext->DeviceExtension = PDODeviceExtension;
    FileContext->FileObject = IoStack->FileObject;

    KeInitializeSpinLock(&FileContext->Lock);
    InitializeListHead(&FileContext->InterruptReadIrpList);
    InitializeListHead(&FileContext->ReportList);

//InitializeListHead(&FileContext->ReadPendingIrpListHead);
//InitializeListHead(&FileContext->IrpCompletedListHead);
//KeInitializeEvent(&FileContext->IrpReadComplete, NotificationEvent, FALSE);

    FileContext->MaxReportQueueSize = HIDCLASS_MAX_REPORT_QUEUE_SIZE;
    FileContext->PendingReports = 0;
    FileContext->RetryReads = 0;

    InsertTailList(&HidCollection->InterruptReportList,
                   &FileContext->InterruptReportLink);

    FileContext->FileAttributes = IoStack->Parameters.Create.FileAttributes;
    FileContext->DesiredAccess = IoStack->Parameters.Create.SecurityContext->DesiredAccess;
    FileContext->ShareAccess = IoStack->Parameters.Create.ShareAccess;

    FileContext->IsMyPrivilegeTrue = IsPrivilege;
    //FileContext->SessionId = SessionId; FIXME

    /* Store pointer to file context */
    IoStack->FileObject->FsContext = FileContext;

    /* Increment open counters */
    InterlockedExchangeAdd(&FDODeviceExtension->OpenCount, 1);
    InterlockedExchangeAdd(&PDODeviceExtension->OpenCount, 1);

    if (IoStack->Parameters.Create.SecurityContext->DesiredAccess & FILE_READ_DATA)
    {
        PDODeviceExtension->OpensForRead++;
    }

    if (IoStack->Parameters.Create.SecurityContext->DesiredAccess & FILE_WRITE_DATA)
    {
        PDODeviceExtension->OpensForWrite++;
    }

    if (!(IoStack->Parameters.Create.ShareAccess & FILE_SHARE_READ))
    {
        PDODeviceExtension->RestrictionsForRead++;
    }

    if (!(IoStack->Parameters.Create.ShareAccess & FILE_SHARE_WRITE))
    {
        PDODeviceExtension->RestrictionsForWrite++;
    }

    if (!IoStack->Parameters.Create.ShareAccess)
    {
        PDODeviceExtension->RestrictionsForAnyOpen++;
    }

UnlockExit:
    KeReleaseSpinLock(&HidCollection->CollectSpinLock, OldIrql);

Exit:
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    DPRINT("HidClass_Create: exit - %x\n", Status);
    return Status;
}

NTSTATUS
NTAPI
HidClass_Close(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
    NTSTATUS Status;
    PIO_STACK_LOCATION IoStack;
    PHIDCLASS_COMMON_DEVICE_EXTENSION CommonDeviceExtension;
    PHIDCLASS_PDO_DEVICE_EXTENSION PDODeviceExtension;
    PHIDCLASS_FDO_EXTENSION FDODeviceExtension;
    PHIDCLASS_FILEOP_CONTEXT IrpContext;
    PHIDCLASS_COLLECTION HidCollection;
    KIRQL OldIrql;

    DPRINT("HidClass_Close: Irp - %p\n", Irp);

    //
    // get device extension
    //
    CommonDeviceExtension = DeviceObject->DeviceExtension;

    //
    // is it a FDO request
    //
    if (CommonDeviceExtension->IsFDO)
    {
        DPRINT1("HidClass_Close: Error ... \n");
        // how did the request get there
        Status = STATUS_INVALID_PARAMETER_1;
        goto Exit;
    }

    Irp->IoStatus.Information = 0;

    /* get device extensions */
    PDODeviceExtension = DeviceObject->DeviceExtension;
    FDODeviceExtension = PDODeviceExtension->FDODeviceExtension;

    //
    // get stack location
    //
    IoStack = IoGetCurrentIrpStackLocation(Irp);

    //
    // sanity checks
    //
    ASSERT(IoStack->FileObject);
    ASSERT(IoStack->FileObject->FsContext);

    //
    // get irp context
    //
    IrpContext = IoStack->FileObject->FsContext;

    if (!IrpContext)
    {
        DPRINT1("HidClass_Close: Error ... \n");
        Status = STATUS_DEVICE_NOT_CONNECTED;
        goto Exit;
    }

    InterlockedExchangeAdd(&FDODeviceExtension->OpenCount, -1);

    HidCollection = GetHidclassCollection(FDODeviceExtension,
                                          PDODeviceExtension->CollectionNumber);

    if (!HidCollection)
    {
        DPRINT1("HidClass_Close: Error ... \n");
        Status = STATUS_DATA_ERROR;
        goto Exit;
    }

    if (FDODeviceExtension->HidFdoState == HIDCLASS_STATE_DELETED)
    {
        KeAcquireSpinLock(&HidCollection->CollectSpinLock, &OldIrql);
        RemoveEntryList(&IrpContext->InterruptReportLink);
        KeReleaseSpinLock(&HidCollection->CollectSpinLock, OldIrql);

        if (IrpContext->IsMyPrivilegeTrue)
        {
            KeAcquireSpinLock(&HidCollection->CollectCloseSpinLock, &OldIrql);

            while (IrpContext->CloseCounter)
            {
                IrpContext->CloseCounter--;
                HidCollection->CloseFlag--;
            }

            IrpContext->CloseCounter--;

            KeReleaseSpinLock(&HidCollection->CollectCloseSpinLock, OldIrql);
        }

        HidClassDestroyFileContext(HidCollection, IrpContext);

        //FIXME RemoveLock support
        ASSERT(FALSE);

        if (0)//IoAcquireRemoveLock(&FDODeviceExtension->HidRemoveLock, 0))
        {
            HidClassCleanUpFDO(FDODeviceExtension);
        }
    }
    else
    {
        KeAcquireSpinLock(&HidCollection->CollectSpinLock, &OldIrql);
        InterlockedExchangeAdd(&PDODeviceExtension->OpenCount, -1);

        if (IrpContext->DesiredAccess & FILE_READ_DATA)
        {
            PDODeviceExtension->OpensForRead--;
        }

        if (IrpContext->DesiredAccess & FILE_WRITE_DATA)
        {
            PDODeviceExtension->OpensForWrite--;
        }

        if (!(IrpContext->ShareAccess & FILE_SHARE_READ))
        {
            PDODeviceExtension->RestrictionsForRead--;
        }

        if (!(IrpContext->ShareAccess & FILE_SHARE_WRITE))
        {
            PDODeviceExtension->RestrictionsForWrite--;
        }

        if (!IrpContext->ShareAccess)
        {
            PDODeviceExtension->RestrictionsForAnyOpen--;
        }

        RemoveEntryList(&IrpContext->InterruptReportLink);
        KeReleaseSpinLock(&HidCollection->CollectSpinLock, OldIrql);

        if (IrpContext->IsMyPrivilegeTrue)
        {
            KeAcquireSpinLock(&HidCollection->CollectCloseSpinLock, &OldIrql);

            while (IrpContext->CloseCounter)
            {
                IrpContext->CloseCounter--;
                HidCollection->CloseFlag--;
            }

            IrpContext->CloseCounter--;

            KeReleaseSpinLock(&HidCollection->CollectCloseSpinLock, OldIrql);
        }

        HidClassDestroyFileContext(HidCollection, IrpContext);
    }

    Status = STATUS_SUCCESS;

Exit:
    /* complete request */
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
NTAPI
HidClass_ReadCompleteIrp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Ctx)
{
    PHIDCLASS_IRP_CONTEXT IrpContext;
    KIRQL OldLevel;
    PUCHAR Address;
    ULONG Offset;
    PHIDP_COLLECTION_DESC CollectionDescription;
    PHIDP_REPORT_IDS ReportDescription;
    BOOLEAN IsEmpty;

    //
    // get irp context
    //
    IrpContext = Ctx;

    DPRINT("HidClass_ReadCompleteIrp Irql %lu\n", KeGetCurrentIrql());
    DPRINT("HidClass_ReadCompleteIrp Status %lx\n", Irp->IoStatus.Status);
    DPRINT("HidClass_ReadCompleteIrp Length %lu\n", Irp->IoStatus.Information);
    DPRINT("HidClass_ReadCompleteIrp Irp %p\n", Irp);
    DPRINT("HidClass_ReadCompleteIrp InputReportBuffer %p\n", IrpContext->InputReportBuffer);
    DPRINT("HidClass_ReadCompleteIrp InputReportBufferLength %li\n", IrpContext->InputReportBufferLength);
    DPRINT("HidClass_ReadCompleteIrp OriginalIrp %p\n", IrpContext->OriginalIrp);

    //
    // copy result
    //
    if (Irp->IoStatus.Information)
    {
        //
        // get address
        //
        Address = MmGetSystemAddressForMdlSafe(IrpContext->OriginalIrp->MdlAddress, NormalPagePriority);
        if (Address)
        {
            //
            // reports may have a report id prepended
            //
            Offset = 0;

            //
            // get collection description
            //
            CollectionDescription = HidClassPDO_GetCollectionDescription(&IrpContext->FileOp->DeviceExtension->Common.DeviceDescription,
                                                                         IrpContext->FileOp->DeviceExtension->CollectionNumber);
            ASSERT(CollectionDescription);

            //
            // get report description
            //
            ReportDescription = HidClassPDO_GetReportDescription(&IrpContext->FileOp->DeviceExtension->Common.DeviceDescription,
                                                                 IrpContext->FileOp->DeviceExtension->CollectionNumber);
            ASSERT(ReportDescription);

            if (CollectionDescription && ReportDescription)
            {
                //
                // calculate offset
                //
                ASSERT(CollectionDescription->InputLength >= ReportDescription->InputLength);
                Offset = CollectionDescription->InputLength - ReportDescription->InputLength;
            }

            //
            // copy result
            //
            RtlCopyMemory(&Address[Offset], IrpContext->InputReportBuffer, IrpContext->InputReportBufferLength);
        }
    }

    //
    // copy result status
    //
    IrpContext->OriginalIrp->IoStatus.Status = Irp->IoStatus.Status;
    IrpContext->OriginalIrp->IoStatus.Information = Irp->IoStatus.Information;

    //
    // free input report buffer
    //
    ExFreePoolWithTag(IrpContext->InputReportBuffer, HIDCLASS_TAG);

    //
    // remove us from pending list
    //
    KeAcquireSpinLock(&IrpContext->FileOp->Lock, &OldLevel);

    //
    // remove from pending list
    //
    //RemoveEntryList(&Irp->Tail.Overlay.ListEntry);
    /* HACK: use Context->ReadIrpLink instead Irp->Tail.Overlay.ListEntry (use usbport) */
    RemoveEntryList(&IrpContext->ReadIrpLink);

    if (IrpContext->OriginalIrp->IoStatus.Status == STATUS_DEVICE_NOT_CONNECTED)
    {
        DPRINT("HidClass_ReadCompleteIrp with STATUS_DEVICE_NOT_CONNECTED\n");
    }

    //
    // is list empty
    //
    IsEmpty = IsListEmpty(&IrpContext->FileOp->ReadPendingIrpListHead);

    //
    // insert into completed list
    //
    InsertTailList(&IrpContext->FileOp->IrpCompletedListHead, &Irp->Tail.Overlay.ListEntry);

    //
    // release lock
    //
    KeReleaseSpinLock(&IrpContext->FileOp->Lock, OldLevel);

    //
    // complete original request
    //
    IoCompleteRequest(IrpContext->OriginalIrp, IO_NO_INCREMENT);

    DPRINT("StopInProgress %x IsEmpty %x\n",
           IrpContext->FileOp->StopInProgress,
           IsEmpty);

    if (IrpContext->FileOp->StopInProgress && IsEmpty)
    {
        //
        // last pending irp
        //
        DPRINT1("[HIDCLASS] LastPendingTransfer Signalling\n");
        KeSetEvent(&IrpContext->FileOp->IrpReadComplete, 0, FALSE);
    }

    //
    // free irp context
    //
    ExFreePoolWithTag(IrpContext, HIDCLASS_TAG);

    //
    // done
    //
    return STATUS_MORE_PROCESSING_REQUIRED;
}

PIRP
HidClass_GetIrp(
    IN PHIDCLASS_FILEOP_CONTEXT Context)
{
   KIRQL OldLevel;
   PIRP Irp = NULL;
   PLIST_ENTRY ListEntry;

    //
    // acquire lock
    //
    KeAcquireSpinLock(&Context->Lock, &OldLevel);

    //
    // is list empty?
    //
    if (!IsListEmpty(&Context->IrpCompletedListHead))
    {
        //
        // grab first entry
        //
        ListEntry = RemoveHeadList(&Context->IrpCompletedListHead);

        //
        // get irp
        //
        Irp = CONTAINING_RECORD(ListEntry, IRP, Tail.Overlay.ListEntry);
    }

    //
    // release lock
    //
    KeReleaseSpinLock(&Context->Lock, OldLevel);

    //
    // done
    //
    return Irp;
}

NTSTATUS
HidClass_BuildIrp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP RequestIrp,
    IN PHIDCLASS_FILEOP_CONTEXT Context,
    IN ULONG DeviceIoControlCode,
    IN ULONG BufferLength,
    OUT PIRP *OutIrp,
    OUT PHIDCLASS_IRP_CONTEXT *OutIrpContext)
{
    PIRP Irp;
    PIO_STACK_LOCATION IoStack;
    PHIDCLASS_IRP_CONTEXT IrpContext;
    PHIDCLASS_PDO_DEVICE_EXTENSION PDODeviceExtension;
    PHIDP_COLLECTION_DESC CollectionDescription;
    PHIDP_REPORT_IDS ReportDescription;

    //
    // get an irp from fresh list
    //
    Irp = HidClass_GetIrp(Context);
    if (!Irp)
    {
        //
        // build new irp
        //
        Irp = IoAllocateIrp(DeviceObject->StackSize, FALSE);
        if (!Irp)
        {
            //
            // no memory
            //
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }
    else
    {
        //
        // re-use irp
        //
        IoReuseIrp(Irp, STATUS_SUCCESS);
    }

    //
    // allocate completion context
    //
    IrpContext = ExAllocatePoolWithTag(NonPagedPool, sizeof(HIDCLASS_IRP_CONTEXT), HIDCLASS_TAG);
    if (!IrpContext)
    {
        //
        // no memory
        //
        IoFreeIrp(Irp);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // get device extension
    //
    PDODeviceExtension = DeviceObject->DeviceExtension;
    ASSERT(PDODeviceExtension->Common.IsFDO == FALSE);

    //
    // init irp context
    //
    RtlZeroMemory(IrpContext, sizeof(HIDCLASS_IRP_CONTEXT));
    IrpContext->OriginalIrp = RequestIrp;
    IrpContext->FileOp = Context;

    //
    // get collection description
    //
    CollectionDescription = HidClassPDO_GetCollectionDescription(&IrpContext->FileOp->DeviceExtension->Common.DeviceDescription,
                                                                 IrpContext->FileOp->DeviceExtension->CollectionNumber);
    ASSERT(CollectionDescription);

    //
    // get report description
    //
    ReportDescription = HidClassPDO_GetReportDescription(&IrpContext->FileOp->DeviceExtension->Common.DeviceDescription,
                                                         IrpContext->FileOp->DeviceExtension->CollectionNumber);
    ASSERT(ReportDescription);

    //
    // sanity check
    //
    ASSERT(CollectionDescription->InputLength >= ReportDescription->InputLength);

    if (Context->StopInProgress)
    {
         //
         // stop in progress
         //
         DPRINT1("[HIDCLASS] Stop In Progress\n");
         Irp->IoStatus.Status = STATUS_CANCELLED;
         IoCompleteRequest(Irp, IO_NO_INCREMENT);
         return STATUS_CANCELLED;

    }

    //
    // store report length
    //
    IrpContext->InputReportBufferLength = ReportDescription->InputLength;

    //
    // allocate buffer
    //
    IrpContext->InputReportBuffer = ExAllocatePoolWithTag(NonPagedPool, IrpContext->InputReportBufferLength, HIDCLASS_TAG);
    if (!IrpContext->InputReportBuffer)
    {
        //
        // no memory
        //
        IoFreeIrp(Irp);
        ExFreePoolWithTag(IrpContext, HIDCLASS_TAG);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // get stack location
    //
    IoStack = IoGetNextIrpStackLocation(Irp);

    //
    // init stack location
    //
    IoStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    IoStack->Parameters.DeviceIoControl.IoControlCode = DeviceIoControlCode;
    IoStack->Parameters.DeviceIoControl.OutputBufferLength = IrpContext->InputReportBufferLength;
    IoStack->Parameters.DeviceIoControl.InputBufferLength = 0;
    IoStack->Parameters.DeviceIoControl.Type3InputBuffer = NULL;
    Irp->UserBuffer = IrpContext->InputReportBuffer;
    IoStack->DeviceObject = DeviceObject;

    //
    // store result
    //
    *OutIrp = Irp;
    *OutIrpContext = IrpContext;

    //
    // done
    //
    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
HidClass_Read(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
    PIO_STACK_LOCATION IoStack;
    PHIDCLASS_FILEOP_CONTEXT Context;
    KIRQL OldLevel;
    NTSTATUS Status;
    PIRP NewIrp;
    PHIDCLASS_IRP_CONTEXT NewIrpContext;
    PHIDCLASS_COMMON_DEVICE_EXTENSION CommonDeviceExtension;

    //
    // get current stack location
    //
    IoStack = IoGetCurrentIrpStackLocation(Irp);

    //
    // get device extension
    //
    CommonDeviceExtension = DeviceObject->DeviceExtension;
    ASSERT(CommonDeviceExtension->IsFDO == FALSE);

    //
    // sanity check
    //
    ASSERT(IoStack->FileObject);
    ASSERT(IoStack->FileObject->FsContext);

    //
    // get context
    //
    Context = IoStack->FileObject->FsContext;
    ASSERT(Context);

    //
    // FIXME support polled devices
    //
    ASSERT(Context->DeviceExtension->Common.DriverExtension->DevicesArePolled == FALSE);

    if (Context->StopInProgress)
    {
        //
        // stop in progress
        //
        DPRINT1("[HIDCLASS] Stop In Progress\n");
        Irp->IoStatus.Status = STATUS_CANCELLED;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_CANCELLED;
    }

    //
    // build irp request
    //
    Status = HidClass_BuildIrp(DeviceObject,
                               Irp,
                               Context,
                               IOCTL_HID_READ_REPORT,
                               IoStack->Parameters.Read.Length,
                               &NewIrp,
                               &NewIrpContext);
    if (!NT_SUCCESS(Status))
    {
        //
        // failed
        //
        DPRINT1("HidClass_BuildIrp failed with %x\n", Status);
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    //
    // acquire lock
    //
    KeAcquireSpinLock(&Context->Lock, &OldLevel);

    /* insert irp into pending list
       HACK: use Context->ReadIrpLink instead Irp->Tail.Overlay.ListEntry (use usbport)
    */
    //InsertTailList(&Context->ReadPendingIrpListHead, &NewIrp->Tail.Overlay.ListEntry);
    InsertTailList(&Context->ReadPendingIrpListHead, &NewIrpContext->ReadIrpLink);

    //
    // set completion routine
    //
    IoSetCompletionRoutine(NewIrp, HidClass_ReadCompleteIrp, NewIrpContext, TRUE, TRUE, TRUE);

    //
    // make next location current
    //
    IoSetNextIrpStackLocation(NewIrp);

    //
    // release spin lock
    //
    KeReleaseSpinLock(&Context->Lock, OldLevel);

    //
    // mark irp pending
    //
    IoMarkIrpPending(Irp);

    //
    // let's dispatch the request
    //
    ASSERT(Context->DeviceExtension);
    Status = Context->DeviceExtension->Common.DriverExtension->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL](Context->DeviceExtension->FDODeviceObject, NewIrp);

    //
    // complete
    //
    return STATUS_PENDING;
}

NTSTATUS
NTAPI
HidClass_Write(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
    UNIMPLEMENTED;
    ASSERT(FALSE);
    Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
NTAPI
HidClass_DeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
    PIO_STACK_LOCATION IoStack;
    PHIDCLASS_COMMON_DEVICE_EXTENSION CommonDeviceExtension;
    PHID_COLLECTION_INFORMATION CollectionInformation;
    PHIDP_COLLECTION_DESC CollectionDescription;
    PHIDCLASS_PDO_DEVICE_EXTENSION PDODeviceExtension;

    //
    // get device extension
    //
    CommonDeviceExtension = DeviceObject->DeviceExtension;

    //
    // only PDO are supported
    //
    if (CommonDeviceExtension->IsFDO)
    {
        //
        // invalid request
        //
        DPRINT1("[HIDCLASS] DeviceControl Irp for FDO arrived\n");
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER_1;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_INVALID_PARAMETER_1;
    }

    ASSERT(CommonDeviceExtension->IsFDO == FALSE);

    //
    // get pdo device extension
    //
    PDODeviceExtension = DeviceObject->DeviceExtension;

    //
    // get stack location
    //
    IoStack = IoGetCurrentIrpStackLocation(Irp);

    switch (IoStack->Parameters.DeviceIoControl.IoControlCode)
    {
        case IOCTL_HID_GET_COLLECTION_INFORMATION:
        {
            //
            // check if output buffer is big enough
            //
            if (IoStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(HID_COLLECTION_INFORMATION))
            {
                //
                // invalid buffer size
                //
                Irp->IoStatus.Status = STATUS_INVALID_BUFFER_SIZE;
                IoCompleteRequest(Irp, IO_NO_INCREMENT);
                return STATUS_INVALID_BUFFER_SIZE;
            }

            //
            // get output buffer
            //
            CollectionInformation = Irp->AssociatedIrp.SystemBuffer;
            ASSERT(CollectionInformation);

            //
            // get collection description
            //
            CollectionDescription = HidClassPDO_GetCollectionDescription(&CommonDeviceExtension->DeviceDescription,
                                                                         PDODeviceExtension->CollectionNumber);
            ASSERT(CollectionDescription);

            //
            // init result buffer
            //
            CollectionInformation->DescriptorSize = CollectionDescription->PreparsedDataLength;
            CollectionInformation->Polled = CommonDeviceExtension->DriverExtension->DevicesArePolled;
            CollectionInformation->VendorID = CommonDeviceExtension->Attributes.VendorID;
            CollectionInformation->ProductID = CommonDeviceExtension->Attributes.ProductID;
            CollectionInformation->VersionNumber = CommonDeviceExtension->Attributes.VersionNumber;

            //
            // complete request
            //
            Irp->IoStatus.Information = sizeof(HID_COLLECTION_INFORMATION);
            Irp->IoStatus.Status = STATUS_SUCCESS;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_SUCCESS;
        }
        case IOCTL_HID_GET_COLLECTION_DESCRIPTOR:
        {
            //
            // get collection description
            //
            CollectionDescription = HidClassPDO_GetCollectionDescription(&CommonDeviceExtension->DeviceDescription,
                                                                         PDODeviceExtension->CollectionNumber);
            ASSERT(CollectionDescription);

            //
            // check if output buffer is big enough
            //
            if (IoStack->Parameters.DeviceIoControl.OutputBufferLength < CollectionDescription->PreparsedDataLength)
            {
                //
                // invalid buffer size
                //
                Irp->IoStatus.Status = STATUS_INVALID_BUFFER_SIZE;
                IoCompleteRequest(Irp, IO_NO_INCREMENT);
                return STATUS_INVALID_BUFFER_SIZE;
            }

            //
            // copy result
            //
            ASSERT(Irp->UserBuffer);
            RtlCopyMemory(Irp->UserBuffer, CollectionDescription->PreparsedData, CollectionDescription->PreparsedDataLength);

            //
            // complete request
            //
            Irp->IoStatus.Information = CollectionDescription->PreparsedDataLength;
            Irp->IoStatus.Status = STATUS_SUCCESS;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_SUCCESS;
        }
        case IOCTL_GET_SYS_BUTTON_CAPS:
        {
            DPRINT1("[HIDCLASS] IOCTL_GET_SYS_BUTTON_CAPS not implemented\n");
            Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_NOT_IMPLEMENTED;
        }
        default:
        {
            DPRINT1("[HIDCLASS] DeviceControl IoControlCode 0x%x not implemented\n",
                    IoStack->Parameters.DeviceIoControl.IoControlCode);

            Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_NOT_IMPLEMENTED;
        }
    }
}

NTSTATUS
NTAPI
HidClass_InternalDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
    UNIMPLEMENTED;
    ASSERT(FALSE);
    Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
NTAPI
HidClass_Power(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
    PHIDCLASS_COMMON_DEVICE_EXTENSION CommonDeviceExtension;
    CommonDeviceExtension = DeviceObject->DeviceExtension;
 
    if (CommonDeviceExtension->IsFDO)
    {
        IoCopyCurrentIrpStackLocationToNext(Irp);
        return HidClassFDO_DispatchRequest(DeviceObject, Irp);
    }
    else
    {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        PoStartNextPowerIrp(Irp);
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }
}

NTSTATUS
NTAPI
HidClass_PnP(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
    PHIDCLASS_COMMON_DEVICE_EXTENSION CommonDeviceExtension;

    //
    // get common device extension
    //
    CommonDeviceExtension = DeviceObject->DeviceExtension;

    //
    // check type of device object
    //
    if (CommonDeviceExtension->IsFDO)
    {
        //
        // handle request
        //
        return HidClassFDO_PnP(DeviceObject, Irp);
    }
    else
    {
        //
        // handle request
        //
        return HidClassPDO_PnP(DeviceObject, Irp);
    }
}

NTSTATUS
NTAPI
HidClass_SystemControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
    DPRINT("HidClass_SystemControl: FIXME. DeviceObject - %p, Irp - %p\n",
           DeviceObject,
           Irp);

    UNIMPLEMENTED;
    //WmiSystemControl()
    return 0;
}

NTSTATUS
NTAPI
HidClass_DispatchDefault(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
    PHIDCLASS_COMMON_DEVICE_EXTENSION CommonDeviceExtension;
    PHIDCLASS_FDO_EXTENSION FDODeviceExtension;
    PHID_DEVICE_EXTENSION HidDeviceExtension;
    NTSTATUS Status;

    DPRINT("DispatchDefault: DeviceObject - %p, Irp - %p\n", DeviceObject, Irp);

    /* Get common device extension */
    CommonDeviceExtension = DeviceObject->DeviceExtension;

    if ( CommonDeviceExtension->IsFDO )
    {
        /* Get device extensions */
        FDODeviceExtension = DeviceObject->DeviceExtension;
        HidDeviceExtension = &FDODeviceExtension->Common.HidDeviceExtension;

        /* Copy current IRP stack location to next*/
        IoCopyCurrentIrpStackLocationToNext(Irp);

        /* Dispatch to lower PDO */
        Status = HidClassFDO_DispatchRequest(HidDeviceExtension->PhysicalDeviceObject,
                                             Irp);
    }
    else
    {
        Status = Irp->IoStatus.Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }

    return Status;
}

NTSTATUS
NTAPI
HidClassDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
    PIO_STACK_LOCATION IoStack;

    //
    // get current stack location
    //
    IoStack = IoGetCurrentIrpStackLocation(Irp);

    DPRINT("[HIDCLASS] Dispatch Major %x Minor %x\n",
           IoStack->MajorFunction,
           IoStack->MinorFunction);

    //
    // dispatch request based on major function
    //
    switch (IoStack->MajorFunction)
    {
        case IRP_MJ_CREATE:
            return HidClass_Create(DeviceObject, Irp);

        case IRP_MJ_CLOSE:
            return HidClass_Close(DeviceObject, Irp);

        case IRP_MJ_READ:
            return HidClass_Read(DeviceObject, Irp);

        case IRP_MJ_WRITE:
            return HidClass_Write(DeviceObject, Irp);

        case IRP_MJ_DEVICE_CONTROL:
            return HidClass_DeviceControl(DeviceObject, Irp);

        case IRP_MJ_INTERNAL_DEVICE_CONTROL:
           return HidClass_InternalDeviceControl(DeviceObject, Irp);

        case IRP_MJ_POWER:
            return HidClass_Power(DeviceObject, Irp);

        case IRP_MJ_PNP:
            return HidClass_PnP(DeviceObject, Irp);

        case IRP_MJ_SYSTEM_CONTROL:
            return HidClass_SystemControl(DeviceObject, Irp);

        default:
            return HidClass_DispatchDefault(DeviceObject, Irp);
    }
}

BOOLEAN
NTAPI
InsertDriverExtList(
    IN PHIDCLASS_DRIVER_EXTENSION DriverExtension)
{
    BOOLEAN Result = TRUE;
    PLIST_ENTRY Entry;
    PHIDCLASS_DRIVER_EXTENSION driverExtension = NULL;

    DPRINT("InsertDriverExtList: DriverExtension - %p\n", DriverExtension);

    ExAcquireFastMutex(&DriverExtListMutex);

    /* Add link for DriverExtension to end list */
    for (Entry = DriverExtList.Flink; ; Entry = Entry->Flink)
    {
        if (Entry == &DriverExtList)
        {
            InsertTailList(&DriverExtList, &DriverExtension->DriverExtLink);
            goto Exit;
        }

        driverExtension = CONTAINING_RECORD(Entry,
                                            HIDCLASS_DRIVER_EXTENSION,
                                            DriverExtLink.Flink);

        if (driverExtension == DriverExtension)
        {
            break;
        }
    }

    Result = FALSE;

Exit:
    ExReleaseFastMutex(&DriverExtListMutex);
    return Result;
}

NTSTATUS
NTAPI
HidRegisterMinidriver(
    IN PHID_MINIDRIVER_REGISTRATION MinidriverRegistration)
{
    NTSTATUS Status;
    PHIDCLASS_DRIVER_EXTENSION DriverExtension;
    PDRIVER_OBJECT MiniDriver;

    /* Check if the version matches */
    if (MinidriverRegistration->Revision > HID_REVISION)
    {
        /* Revision mismatch */
        DPRINT1("HIDCLASS revision is %d. Should be HID_REVISION (1)\n",
                MinidriverRegistration->Revision);

        ASSERT(FALSE);
        return STATUS_REVISION_MISMATCH;
    }

    MiniDriver = MinidriverRegistration->DriverObject;
    DPRINT("HidRegisterMinidriver: MiniDriver - %p\n", MiniDriver);

    /* Now allocate the driver object extension */
    Status = IoAllocateDriverObjectExtension(MiniDriver,
                                             ClientIdentificationAddress,
                                             sizeof(HIDCLASS_DRIVER_EXTENSION),
                                             (PVOID *)&DriverExtension);
    if (!NT_SUCCESS(Status))
    {
        /* Failed to allocate driver extension */
        DPRINT1("HidRegisterMinidriver: IoAllocateDriverObjectExtension failed %x\n",
                Status);

        ASSERT(FALSE);
        return Status;
    }

    /* Zero driver extension */
    RtlZeroMemory(DriverExtension, sizeof(HIDCLASS_DRIVER_EXTENSION));

    /* Initialize driver extension */
    DriverExtension->DriverObject = MiniDriver;
    DriverExtension->DeviceExtensionSize = MinidriverRegistration->DeviceExtensionSize;
    DriverExtension->DevicesArePolled = MinidriverRegistration->DevicesArePolled;

    /* Copy driver dispatch routines */
    RtlCopyMemory(DriverExtension->MajorFunction,
                  MiniDriver->MajorFunction,
                  sizeof(PDRIVER_DISPATCH) * (IRP_MJ_MAXIMUM_FUNCTION + 1));

    MiniDriver->MajorFunction[IRP_MJ_CREATE] = HidClassDispatch;
    MiniDriver->MajorFunction[IRP_MJ_CLOSE] = HidClassDispatch;
    MiniDriver->MajorFunction[IRP_MJ_READ] = HidClassDispatch;
    MiniDriver->MajorFunction[IRP_MJ_WRITE] = HidClassDispatch;
    MiniDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = HidClassDispatch;
    MiniDriver->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = HidClassDispatch;
    MiniDriver->MajorFunction[IRP_MJ_POWER] = HidClassDispatch;
    MiniDriver->MajorFunction[IRP_MJ_PNP] = HidClassDispatch;

    ASSERT(MiniDriver->DriverExtension->AddDevice);
    DriverExtension->AddDevice = MiniDriver->DriverExtension->AddDevice;
    MiniDriver->DriverExtension->AddDevice = HidClassAddDevice;

    ASSERT(MiniDriver->DriverUnload);
    DriverExtension->DriverUnload = MiniDriver->DriverUnload;
    MiniDriver->DriverUnload = HidClassDriverUnload;

    /* Initialize reference counter */
    DriverExtension->RefCount = 0;

    /* Add  driver extension to list */
    if (!InsertDriverExtList(DriverExtension))
    {
        DPRINT1("HidRegisterMinidriver: InsertDriverExtList failed\n");
        Status = STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    return STATUS_SUCCESS;
}
