#include "atax.h"               

#define NDEBUG
#include <debug.h>


ULONG  AtaXDeviceCounter = 0;


NTSTATUS NTAPI
AddChannelFdo(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT ChannelPdo)
{
  PDEVICE_OBJECT          AtaXChannelFdo;
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

  
  DPRINT("AddChannelFdo: Status - %x \n", Status);
  DPRINT("AddChannelFdo ---------------------------------------------------------- \n"  );
  return Status;
}
