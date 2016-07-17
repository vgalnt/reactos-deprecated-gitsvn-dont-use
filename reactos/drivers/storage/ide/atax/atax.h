#ifndef _ATAX_PCH_
#define _ATAX_PCH_

#include <ntddk.h>
#include <stdio.h>
#include <wdm.h>
#include <ide.h>
#include <initguid.h>
#include <wdmguid.h>


//
// Определениe глобальных переменных
//

extern ULONG AtaXDeviceCounter;  // Нумерация устройств
extern ULONG AtaXChannelCounter; // Нумерация каналов

//
// Определениe структур
//

typedef struct _ATAX_REGISTERS_1 {        //// Адреса блока командных регистров IDE устройства

  PUSHORT   Data;             //0    Read/Write data register is 16 bits PIO data transfer (if DWordIO - 32 bits - ?)

  union  {                    //1
    PUCHAR  Error;                // When read
    PUCHAR  Features;             // When written 
  };

  union  {                    //2
    PUCHAR  SectorCount;          // When read (non PACKET command). When written
    PUCHAR  InterruptReason;      // When read. PACKET command 
  };

  PUCHAR    LowLBA;           //3    Read/Write

  union  {                    //4
    PUCHAR  MidLBA;               // Read/Write non PACKET command
    PUCHAR  ByteCountLow;         // Read/Write PACKET command
  };

  union  {                    //5
    PUCHAR  HighLBA;              // Read/Write non PACKET command
    PUCHAR  ByteCountHigh;        // Read/Write PACKET command
  };

  PUCHAR    DriveSelect;      //6    Read/Write

  union  {                    //7
    PUCHAR  Status;               // When read
    PUCHAR  Command;              // When write 
  };

} ATAX_REGISTERS_1, *PATAX_REGISTERS_1;

typedef struct _ATAX_REGISTERS_2 {        //// Адреса блока регистров управления IDE устройства

  union  {                    //0
    PUCHAR  AlternateStatus;      // When read
    PUCHAR  DeviceControl;        // When write
  };

  PUCHAR    DriveAddress;     //1 (not used)

} ATAX_REGISTERS_2, *PATAX_REGISTERS_2;

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
  ATAX_REGISTERS_1         BaseIoAddress1;                    // Список адресов (или портов) для блока командных регистров
  ATAX_REGISTERS_2         BaseIoAddress2;                    // Список адресов (или портов) для блока регистров управления (используется только первый)

  // Interfaces
  PBUS_INTERFACE_STANDARD  BusInterface;

  // IoConnectInterrupt() 
  PKINTERRUPT              InterruptObject;
  ULONG                    InterruptLevel;
  ULONG                    InterruptVector;
  USHORT                   InterruptFlags;
  UCHAR                    InterruptShareDisposition;
  UCHAR                    Padded1;
  KAFFINITY                InterruptAffinity;


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
