
#include "ahcix.h"

//#define NDEBUG
#include <debug.h>


DRIVER_UNLOAD AhciXUnload;
VOID NTAPI 
AhciXUnload(IN PDRIVER_OBJECT DriverObject)
{
  DPRINT1("AhciX Unload ... \n");
}

NTSTATUS NTAPI
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath)
{
  DPRINT("AhciX DriverEntry (%p '%wZ')\n", DriverObject, RegistryPath);

  DriverObject->DriverExtension->AddDevice                    = AhciXAddDevice;
  DriverObject->DriverUnload                                  = AhciXUnload;
  DriverObject->DriverStartIo                                 = 0;  // AhciXStartIo;

  DriverObject->MajorFunction[IRP_MJ_CREATE]                  = AhciXForwardOrIgnore;
  DriverObject->MajorFunction[IRP_MJ_CLOSE]                   = AhciXForwardOrIgnore;
  DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]          = AhciXForwardOrIgnore;  // AhciXDispatchDeviceControl;
  DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = AhciXForwardOrIgnore;  // AhciXDispatchScsi;

  DriverObject->MajorFunction[IRP_MJ_POWER]                   = AhciXForwardOrIgnore;  // AhciXDispatchPower;
  DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL]          = AhciXForwardOrIgnore;  // AhciXDispatchSystemControl;
  DriverObject->MajorFunction[IRP_MJ_PNP]                     = AhciXPnpDispatch;

  return STATUS_SUCCESS;
}
