#ifndef _ATAX_PCH_
#define _ATAX_PCH_

#include <ntddk.h>
#include <stdio.h>
#include <ntifs.h>
#include <wdm.h>
#include <ide.h>
#include <srb.h>
#include <scsi.h>
#include <ntddscsi.h>
#include <ntdddisk.h>
#include <initguid.h>
#include <wdmguid.h>
#include <..\bmaster.h>


//
// Определениe глобальных переменных
//

extern ULONG AtaXDeviceCounter;  // Нумерация устройств
extern ULONG AtaXChannelCounter; // Нумерация каналов

//
// SRB Flag Bits (not best variant - FIXME)
//
#define SRB_FLAGS_USE_DMA            0x00001000

//
// Определение выбора IDE устройства
//
#define IDE_DRIVE_SELECT             0xA0

//
// Flags
//
#define ATAX_DEVICE_BUSY             0x00000001
#define ATAX_LU_ACTIVE               0x00000002
#define ATAX_NOTIFICATION_NEEDED     0x00000004
#define ATAX_NEXT_REQUEST_READY      0x00000008
#define ATAX_FLUSH_ADAPTERS          0x00000010
#define ATAX_MAP_TRANSFER            0x00000020
#define ATAX_RESET                   0x00000080
#define ATAX_RESET_REQUEST           0x00000100
#define ATAX_RESET_REPORTED          0x00000200
#define ATAX_REQUEST_PENDING         0x00000800
#define ATAX_DISCONNECT_ALLOWED      0x00001000
#define ATAX_DISABLE_INT_REQUESET    0x00002000
#define ATAX_DISABLE_INTERRUPTS      0x00004000
#define ATAX_ENABLE_INT_REQUEST      0x00008000
#define ATAX_TIMER_NEEDED            0x00010000

//
// Определения флагов для расширения IDE устройств
//
#define DFLAGS_DEVICE_PRESENT        0x0001    // Indicates that some device is present.
#define DFLAGS_ATAPI_DEVICE          0x0002    // Indicates whether Atapi commands can be used.
#define DFLAGS_TAPE_DEVICE           0x0004    // Indicates whether this is a tape device.
#define DFLAGS_INT_DRQ               0x0008    // Indicates whether device interrupts as DRQ is set after receiving Atapi Packet Command
#define DFLAGS_REMOVABLE_DRIVE       0x0010    // Indicates that the drive has the 'removable' bit set in identify data (offset 128)
#define DFLAGS_MEDIA_STATUS_ENABLED  0x0020    // Media status notification enabled
#define DFLAGS_ATAPI_CHANGER         0x0040    // Indicates atapi 2.5 changer present.
#define DFLAGS_SANYO_ATAPI_CHANGER   0x0080    // Indicates multi-platter device, not conforming to the 2.5 spec.
#define DFLAGS_CHANGER_INITED        0x0100    // Indicates that the init path for changers has already been done.
#define DFLAGS_USE_DMA               0x0200    // Indicates that device use DMA mode (if FALSE - PIO mode)

//
// Определения команд для ATA/ATAPI устройств 
//
#define IDE_COMMAND_READ             0x20
#define IDE_COMMAND_WRITE            0x30
#define IDE_COMMAND_READ_MULTIPLE    0xC4
#define IDE_COMMAND_WRITE_MULTIPLE   0xC5
#define IDE_COMMAND_SET_MULTIPLE     0xC6
#define IDE_COMMAND_READ_DMA         0xC8
#define IDE_COMMAND_WRITE_DMA        0xCA

#define IDE_COMMAND_GET_MEDIA_STATUS 0xDA

#define IDE_COMMAND_ATAPI_RESET      0x08
#define IDE_COMMAND_ATAPI_PACKET     0xA0
#define IDE_COMMAND_ATAPI_IDENTIFY   0xA1
#define IDE_COMMAND_IDENTIFY         0xEC

#define IDE_COMMAND_SET_FEATURES     0xEF

//
// Определения команд для ATAPI устройств
//
#define ATAPI_MODE_SENSE             0x5A
#define ATAPI_MODE_SELECT            0x55
#define ATAPI_FORMAT_UNIT            0x24

//
// Определения для управления ATA/ATAPI устройствами
//
#define IDE_DC_REENABLE_CONTROLLER   0x00  // Разрешить прерывания для канала
#define IDE_DC_DISABLE_INTERRUPTS    0x02  // Запретить прерывания для канала
#define IDE_DC_RESET_CONTROLLER      0x04  // Сбросить канал

//
// Определения статуса для ATA/ATAPI устройств
//
#define IDE_STATUS_BUSY              0x80
#define IDE_STATUS_IDLE              0x50
#define IDE_STATUS_DRDY              0x40
#define IDE_STATUS_DSC               0x10
#define IDE_STATUS_DRQ               0x08
#define IDE_STATUS_CORRECTED_ERROR   0x04
#define IDE_STATUS_INDEX             0x02
#define IDE_STATUS_ERROR             0x01

//
// LUN Extension flags
//
#define LUNEX_FROZEN_QUEUE           0x0001
#define LUNEX_NEED_REQUEST_SENSE     0x0004
#define LUNEX_BUSY                   0x0008
#define LUNEX_FULL_QUEUE             0x0010
#define LUNEX_REQUEST_PENDING        0x0020

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

typedef struct _SCSI_REQUEST_BLOCK_INFO {

  LIST_ENTRY                        Requests;
  PSCSI_REQUEST_BLOCK               Srb;
  PCHAR                             DataOffset;
  PVOID                             SaveSenseRequest;
  ULONG                             SequenceNumber;

  struct _SCSI_REQUEST_BLOCK_INFO * CompletedRequests;

} SCSI_REQUEST_BLOCK_INFO, *PSCSI_REQUEST_BLOCK_INFO;

typedef struct _MODE_SENSE_10 {

  UCHAR OperationCode;
  UCHAR Reserved1;
  UCHAR PageCode : 6;
  UCHAR Pc       : 2;
  UCHAR Reserved2[4];
  UCHAR ParameterListLengthMsb;
  UCHAR ParameterListLengthLsb;
  UCHAR Reserved3[3];

} MODE_SENSE_10, *PMODE_SENSE_10;

typedef struct _MODE_SELECT_10 {

  UCHAR OperationCode;
  UCHAR Reserved1 : 4;
  UCHAR PFBit     : 1;
  UCHAR Reserved2 : 3;
  UCHAR Reserved3[5];
  UCHAR ParameterListLengthMsb;
  UCHAR ParameterListLengthLsb;
  UCHAR Reserved4[3];

} MODE_SELECT_10, *PMODE_SELECT_10;

typedef struct _ATAX_INTERRUPT_DATA {

  ULONG                     Flags;                           // Interrupt Флаги
  PSCSI_REQUEST_BLOCK_INFO  CompletedRequests;               // Список Srb Info 

} ATAX_INTERRUPT_DATA, *PATAX_INTERRUPT_DATA;

typedef struct _HW_DEVICE_EXTENSION {                     //// Аппаратное расширение устройств канала

  PCIIDE_TRANSFER_MODE_SELECT  TransferInfo;               // Информация о режимах пересылки данных

  // Controller properties
  PIDE_CONTROLLER_PROPERTIES   ControllerProperties;       // !!! 1.Указатель на конфигурационную информацию IDE контроллера 
  PUCHAR                       MiniControllerExtension;    // !!! 2.Указатель на расширение устройства минидрайвера IDE контроллера (должен стоять сразу после ControllerProperties)

} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;

typedef struct _COMMON_ATAX_DEVICE_EXTENSION { 

  PDEVICE_OBJECT  LowerDevice; // Если ниже в стеке есть Filter DO, то указатель на Filter DO, если нет, то указатель на нижний PDO. (Только для FDO. Для PDO будет NULL)
  PDEVICE_OBJECT  LowerPdo;    // Указатель на нижний (в стеке) объект PDO
  PDRIVER_OBJECT  SelfDriver;  // Указатель на собственный объект драйвера (на AtaX)
  PDEVICE_OBJECT  SelfDevice;  // Указатель на собственный объект устройства (PDO либо FDO)
  BOOLEAN         IsFDO;       // TRUE - если это расширение FDO, FALSE - PDO

} COMMON_ATAX_DEVICE_EXTENSION, *PCOMMON_ATAX_DEVICE_EXTENSION;

typedef struct _FDO_CHANNEL_EXTENSION {                   //// FDO расширение AtaXChannel

  COMMON_ATAX_DEVICE_EXTENSION   CommonExtension;          // Общее и для PDO и для FDO расширений

  ULONG                    Flags;                             // Флаги FDO расширения 
  ULONG                    Channel;                           // Номер канала
  IDE_CHANNEL_STATE        ChannelState;                      // Состояние канала
  ATAX_REGISTERS_1         BaseIoAddress1;                    // Список адресов (или портов) для блока командных регистров
  ATAX_REGISTERS_2         BaseIoAddress2;                    // Список адресов (или портов) для блока регистров управления (используется только первый)
  ULONG                    SequenceNumber;                    // глобальный счётчик для пакетов
  KDPC                     Dpc;
  KSPIN_LOCK               SpinLock;
  LONG                     TimerCount;
  ATAX_INTERRUPT_DATA      InterruptData;

  // Interfaces
  PBUS_INTERFACE_STANDARD  BusInterface;
  BUS_MASTER_INTERFACE     BusMasterInterface;

  // IoConnectInterrupt() 
  PKINTERRUPT              InterruptObject;
  ULONG                    InterruptLevel;
  ULONG                    InterruptVector;
  USHORT                   InterruptFlags;
  UCHAR                    InterruptShareDisposition;
  UCHAR                    Padded1;
  KAFFINITY                InterruptAffinity;

  PSCSI_REQUEST_BLOCK      CurrentSrb;                        // Текущий SCSI_REQUEST_BLOCK
  ULONG                    SrbFlags;

  BOOLEAN                  ExpectingInterrupt;                // Ожидаемое прерывание
  BOOLEAN                  RDP;                               // Indicate last tape command was DSC Restrictive.
  BOOLEAN                  DWordIO;                           // 32-bit PIO
  BOOLEAN                  BusMaster;                         // Текущая операция - Bus master операция
  PUSHORT                  DataBuffer;                        // Указатель на буфер данных
  ULONG                    WordsLeft;                         // Осталось передать слов (по 2 байта)
  UCHAR                    SmartCommand;                      // Значения подкоманды последней команды SMART
  UCHAR                    ReturningMediaStatus;              // Значение регистра Status после команды GET_MEDIA_STATUS

  PDEVICE_OBJECT           AtaXDevicePdo[MAX_IDE_DEVICE];     // Указатели на дочерние PDO
  IDENTIFY_DATA            FullIdentifyData[MAX_IDE_DEVICE];  // Identify данные для каждого устройства ("паспорт" устройства)
  USHORT                   DeviceFlags[MAX_IDE_DEVICE];       // Флаги устройства
  UCHAR                    MaximumBlockXfer[MAX_IDE_DEVICE];  // Кол-во блоков пересылаемых за одно прерывание согласно "паспорта" устройства
  INQUIRYDATA              InquiryData[MAX_IDE_DEVICE];       // Inquiry данные для каждого устройства

  HW_DEVICE_EXTENSION      HwDeviceExtension;                 // Параметры контроллера

} FDO_CHANNEL_EXTENSION, *PFDO_CHANNEL_EXTENSION;

typedef struct _PDO_DEVICE_EXTENSION {                    //// PDO расширение AtaXDevice

  COMMON_ATAX_DEVICE_EXTENSION  CommonExtension;                // Общее и для PDO и для FDO расширение

  ULONG                    Flags;                          // Флаги PDO расширения 
  PFDO_CHANNEL_EXTENSION   AtaXChannelFdoExtension;        // Указатель на расширение родительского FDO
  UCHAR                    PathId;                         // Номер канала (Primari или Secondary)
  UCHAR                    TargetId;                       // Номер девайса (Master или Slave)
  UCHAR                    Lun;                            // Не используется в AtaX (0)
  LONG                     RequestTimeout;                 // Тайм-аут запроса
  ULONG                    AttemptCount;
  PIRP                     PendingRequest;
  PIRP                     BusyRequest;

  // Очередь IRPs для устройства
  KDEVICE_QUEUE            DeviceQueue;                    // Структура для поддержки очереди из IRPs
  ULONG                    MaxQueueCount;                  // Максимальное количество IRPs в очереди
  ULONG                    QueueCount;                     // Счётчик IRPs в очереди
  ULONG                    SortKey;                        // Индекс сортировки

  SCSI_REQUEST_BLOCK_INFO  SrbInfo;

} PDO_DEVICE_EXTENSION, *PPDO_DEVICE_EXTENSION; 

typedef struct _ATAX_SAVE_INTERRUPT {

  PATAX_INTERRUPT_DATA    InterruptData;
  PFDO_CHANNEL_EXTENSION  DeviceExtension;

} ATAX_SAVE_INTERRUPT, *PATAX_SAVE_INTERRUPT;

//
// Определения функций
//

// atax.c
VOID 
AtaXWaitOnBusy(IN PATAX_REGISTERS_2 AtaXRegisters2);

VOID 
AtaXWaitOnBaseBusy(IN PATAX_REGISTERS_1 AtaXRegisters1);

VOID
AtaXWaitForDrq(IN PATAX_REGISTERS_2 AtaXRegisters2);

VOID 
AtaXSoftReset(
    IN PATAX_REGISTERS_1 AtaXRegisters1,
    IN PATAX_REGISTERS_2 AtaXRegisters2,
    IN ULONG DeviceNumber);

ULONG 
AtaXMapError(
    IN PFDO_CHANNEL_EXTENSION AtaXChannelFdoExtension,
    IN PSCSI_REQUEST_BLOCK Srb);

VOID
AtaXNotification(
    IN SCSI_NOTIFICATION_TYPE NotificationType,
    IN PFDO_CHANNEL_EXTENSION AtaXChannelFdoExtension, ...);

VOID NTAPI
AtaXStartIo(
    IN PDEVICE_OBJECT AtaXChannelFdo,
    IN PIRP Irp);

// ataxfdo.c
NTSTATUS
AtaXChannelFdoDispatchPnp(
    IN PDEVICE_OBJECT AtaXChannelFdo,
    IN PIRP Irp);

NTSTATUS NTAPI
AddChannelFdo(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT ChannelPdo);

// ataxinit.c
BOOLEAN
AtaXDetectDevices(IN PFDO_CHANNEL_EXTENSION AtaXChannelFdoExtension);

BOOLEAN
AtaXIssueIdentify(
    IN PFDO_CHANNEL_EXTENSION AtaXChannelFdoExtension,
    IN PIDENTIFY_DATA Identify,
    IN ULONG DeviceNumber,
    IN UCHAR Command);

VOID
AtaXGetNextRequest(
    IN PFDO_CHANNEL_EXTENSION AtaXChannelFdoExtension,
    IN PPDO_DEVICE_EXTENSION  AtaXDevicePdoExtension);

// ataxpdo.c
NTSTATUS
AtaXDevicePdoDispatchScsi(
    IN PDEVICE_OBJECT AtaXDevicePdo,
    IN PIRP Irp);

NTSTATUS
AtaXDevicePdoDispatchPnp(
    IN PDEVICE_OBJECT AtaXDevicePdo,
    IN PIRP Irp);

//
// Определения макросов
//
#define IS_RDP(OperationCode)\
    ((OperationCode == SCSIOP_ERASE)||\
    (OperationCode == SCSIOP_LOAD_UNLOAD)||\
    (OperationCode == SCSIOP_LOCATE)||\
    (OperationCode == SCSIOP_REWIND) ||\
    (OperationCode == SCSIOP_SPACE)||\
    (OperationCode == SCSIOP_SEEK)||\
    (OperationCode == SCSIOP_WRITE_FILEMARKS))

#endif /* _ATAX_PCH_ */
