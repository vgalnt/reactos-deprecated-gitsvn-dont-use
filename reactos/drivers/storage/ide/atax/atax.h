#ifndef _ATAX_PCH_
#define _ATAX_PCH_

#include <ntddk.h>
#include <stdio.h>
#include <wdm.h>
#include <ide.h>
#include <wdmguid.h>


//
// Определениe глобальных переменных
//

extern ULONG AtaXDeviceCounter;  // Нумерация устройств
extern ULONG AtaXChannelCounter; // Нумерация каналов

//
// Определениe структур
//

typedef struct _COMMON_ATAX_DEVICE_EXTENSION { 

  PDEVICE_OBJECT  LowerDevice; // Если ниже в стеке есть Filter DO, то указатель на Filter DO, если нет, то указатель на нижний PDO. (Только для FDO. Для PDO будет NULL)
  PDEVICE_OBJECT  LowerPdo;    // Указатель на нижний (в стеке) объект PDO
  PDRIVER_OBJECT  SelfDriver;  // Указатель на собственный объект драйвера (на AtaX)
  PDEVICE_OBJECT  SelfDevice;  // Указатель на собственный объект устройства (PDO либо FDO)
  BOOLEAN         IsFDO;       // TRUE - если это расширение FDO, FALSE - PDO

} COMMON_ATAX_DEVICE_EXTENSION, *PCOMMON_ATAX_DEVICE_EXTENSION;

typedef struct _FDO_CHANNEL_EXTENSION {                   //// FDO расширение AtaXChannel

  COMMON_ATAX_DEVICE_EXTENSION   CommonExtension;          // Общее и для PDO и для FDO расширений

  ULONG                    Channel;                           // Номер канала

  // Interfaces
  PBUS_INTERFACE_STANDARD  BusInterface;

} FDO_CHANNEL_EXTENSION, *PFDO_CHANNEL_EXTENSION;

//
// Определения функций
//

// atax.c

// ataxfdo.c
NTSTATUS
AtaXChannelFdoDispatchPnp(
    IN PDEVICE_OBJECT AtaXChannelFdo,
    IN PIRP Irp);

NTSTATUS NTAPI
AddChannelFdo(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT ChannelPdo);

#endif /* _ATAX_PCH_ */
