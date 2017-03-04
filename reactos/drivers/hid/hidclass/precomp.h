#ifndef _HIDCLASS_PCH_
#define _HIDCLASS_PCH_

#define _HIDPI_NO_FUNCTION_MACROS_
#include <wdm.h>
#include <hidpddi.h>
#include <stdio.h>
#include <hidport.h>

#define HIDCLASS_TAG 'CdiH'
#define HIDCLASS_NULL_POINTER (PVOID)0xFFFFFFB0

#define HIDCLASS_STATE_NOT_INIT  1
#define HIDCLASS_STATE_STARTING  2
#define HIDCLASS_STATE_STARTED   3
#define HIDCLASS_STATE_FAILED    4
#define HIDCLASS_STATE_STOPPING  5
#define HIDCLASS_STATE_DISABLED  6
#define HIDCLASS_STATE_REMOVED   7

typedef struct
{
    PDRIVER_OBJECT DriverObject;
    ULONG DeviceExtensionSize;
    BOOLEAN DevicesArePolled;
    UCHAR Reserved[3];
    PDRIVER_DISPATCH MajorFunction[IRP_MJ_MAXIMUM_FUNCTION + 1];
    PDRIVER_ADD_DEVICE AddDevice;
    PDRIVER_UNLOAD DriverUnload;
    KSPIN_LOCK Lock;
    LONG RefCount;
    LIST_ENTRY DriverExtLink;

} HIDCLASS_DRIVER_EXTENSION, *PHIDCLASS_DRIVER_EXTENSION;

typedef struct
{
    //
    // hid device extension
    //
    HID_DEVICE_EXTENSION HidDeviceExtension;

    //
    // if it is a pdo
    //
    BOOLEAN IsFDO;

    //
    // driver extension
    //
    PHIDCLASS_DRIVER_EXTENSION DriverExtension;

    //
    // device description
    //
    HIDP_DEVICE_DESC DeviceDescription;

    //
    // hid attributes
    //
    HID_DEVICE_ATTRIBUTES Attributes;

} HIDCLASS_COMMON_DEVICE_EXTENSION, *PHIDCLASS_COMMON_DEVICE_EXTENSION;

typedef struct
{
    //
    // parts shared by fdo and pdo
    //
    HIDCLASS_COMMON_DEVICE_EXTENSION Common;

    //
    // device capabilities
    //
    DEVICE_CAPABILITIES Capabilities;

    //
    // hid descriptor
    //
    HID_DESCRIPTOR HidDescriptor;

    //
    // report descriptor
    //
    PUCHAR ReportDescriptor;

    //
    // device relations
    //
    PDEVICE_RELATIONS DeviceRelations;

    /* FDO PnP state */
    ULONG HidFdoState;

} HIDCLASS_FDO_EXTENSION, *PHIDCLASS_FDO_EXTENSION;

typedef struct
{
    //
    // parts shared by fdo and pdo
    //
    HIDCLASS_COMMON_DEVICE_EXTENSION Common;

    //
    // device capabilities
    //
    DEVICE_CAPABILITIES Capabilities;

    //
    // collection index
    //
    ULONG CollectionNumber;

    //
    // device interface
    //
    UNICODE_STRING DeviceInterface;

    //
    // FDO device object
    //
    PDEVICE_OBJECT FDODeviceObject;

    //
    // fdo device extension
    //
    PHIDCLASS_FDO_EXTENSION FDODeviceExtension;

    /* PDO PnP state */
    ULONG HidPdoState;

} HIDCLASS_PDO_DEVICE_EXTENSION, *PHIDCLASS_PDO_DEVICE_EXTENSION;

typedef struct __HIDCLASS_FILEOP_CONTEXT__
{
    //
    // device extension
    //
    PHIDCLASS_PDO_DEVICE_EXTENSION DeviceExtension;

    //
    // spin lock
    //
    KSPIN_LOCK Lock;

    //
    // read irp pending list
    //
    LIST_ENTRY ReadPendingIrpListHead;

    //
    // completed irp list
    //
    LIST_ENTRY IrpCompletedListHead;

    //
    // stop in progress indicator
    //
    BOOLEAN StopInProgress;

    //
    // read complete event
    //
    KEVENT IrpReadComplete;

} HIDCLASS_FILEOP_CONTEXT, *PHIDCLASS_FILEOP_CONTEXT;

typedef struct
{
    //
    // original request
    //
    PIRP OriginalIrp;

    //
    // file op
    //
    PHIDCLASS_FILEOP_CONTEXT FileOp;

    //
    // buffer for reading report
    //
    PVOID InputReportBuffer;

    //
    // buffer length
    //
    ULONG InputReportBufferLength;

    //
    // work item
    //
    PIO_WORKITEM CompletionWorkItem;

    /* for HACK: use instead Irp->Tail.Overlay.ListEntry (use usbport)
       link with pending IRPs list 
    */
    LIST_ENTRY ReadIrpLink;
} HIDCLASS_IRP_CONTEXT, *PHIDCLASS_IRP_CONTEXT;

/* hidclass.c */
PHIDCLASS_DRIVER_EXTENSION
NTAPI
RefDriverExt(
    IN PDRIVER_OBJECT DriverObject);

PHIDCLASS_DRIVER_EXTENSION
NTAPI
DerefDriverExt(
    IN PDRIVER_OBJECT DriverObject);

/* fdo.c */
NTSTATUS
HidClassFDO_PnP(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp);

NTSTATUS
HidClassFDO_DispatchRequest(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp);

NTSTATUS
HidClassFDO_DispatchRequestSynchronous(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp);

/* pdo.c */
NTSTATUS
HidClassPDO_CreatePDO(
    IN PDEVICE_OBJECT DeviceObject,
    OUT PDEVICE_RELATIONS *OutDeviceRelations);

NTSTATUS
HidClassPDO_PnP(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp);

PHIDP_COLLECTION_DESC
HidClassPDO_GetCollectionDescription(
    PHIDP_DEVICE_DESC DeviceDescription,
    ULONG CollectionNumber);

PHIDP_REPORT_IDS
HidClassPDO_GetReportDescription(
    PHIDP_DEVICE_DESC DeviceDescription,
    ULONG CollectionNumber);

#endif /* _HIDCLASS_PCH_ */
