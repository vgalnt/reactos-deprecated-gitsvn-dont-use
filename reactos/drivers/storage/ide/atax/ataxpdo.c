#include "atax.h"

#define NDEBUG
#include <debug.h>


NTSTATUS
DuplicateUnicodeString(
    IN ULONG Flags,
    IN PCUNICODE_STRING SourceString,
    OUT PUNICODE_STRING DestinationString)
{
  if (SourceString == NULL || DestinationString == NULL
    || SourceString->Length > SourceString->MaximumLength
    || (SourceString->Length == 0 && SourceString->MaximumLength > 0 && SourceString->Buffer == NULL)
    || Flags == RTL_DUPLICATE_UNICODE_STRING_ALLOCATE_NULL_STRING || Flags >= 4)
  {
    return STATUS_INVALID_PARAMETER;
  }

  if ((SourceString->Length == 0)
    && (Flags != (RTL_DUPLICATE_UNICODE_STRING_NULL_TERMINATE |
                  RTL_DUPLICATE_UNICODE_STRING_ALLOCATE_NULL_STRING)))
  {
    DestinationString->Length = 0;
    DestinationString->MaximumLength = 0;
    DestinationString->Buffer = NULL;
  }
  else
  {
    USHORT DestMaxLength = SourceString->Length;
    
    if (Flags & RTL_DUPLICATE_UNICODE_STRING_NULL_TERMINATE)
      DestMaxLength += sizeof(UNICODE_NULL);
    
    DestinationString->Buffer = ExAllocatePool(PagedPool, DestMaxLength);
    if (DestinationString->Buffer == NULL)
      return STATUS_NO_MEMORY;
    
    RtlCopyMemory(DestinationString->Buffer, SourceString->Buffer, SourceString->Length);
    DestinationString->Length = SourceString->Length;
    DestinationString->MaximumLength = DestMaxLength;
    
    if (Flags & RTL_DUPLICATE_UNICODE_STRING_NULL_TERMINATE)
      DestinationString->Buffer[DestinationString->Length / sizeof(WCHAR)] = 0;
  }
  
  return STATUS_SUCCESS;
}

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

ULONG
AtaXGetFieldLength(
    IN PUCHAR Name,
    IN ULONG MaxLength)
{
  ULONG ix;
  ULONG LastCharacterPosition = 0;
  
  // scan the field and return last positon which contains a valid character
  for( ix = 0; ix < MaxLength; ix++ )
  {
    // trim white spaces from field
    if ( Name[ix] != ' ' )
      LastCharacterPosition = ix;
  }

  // convert from zero based index to length
  return LastCharacterPosition + 1;
}

NTSTATUS
AtaXStorageQueryProperty(
    IN PPDO_DEVICE_EXTENSION AtaXDevicePdoExtension,
    IN PIRP Irp)
{
  NTSTATUS                     Status;
  PFDO_CHANNEL_EXTENSION       AtaXChannelFdoExtension;
  PIDENTIFY_DATA               IdentifyData;
  PINQUIRYDATA                 InquiryData;
  PSTORAGE_PROPERTY_QUERY      PropertyQuery;
  PSTORAGE_DESCRIPTOR_HEADER   DescriptorHeader;
  PSTORAGE_ADAPTER_DESCRIPTOR  AdapterDescriptor;
  PSTORAGE_DEVICE_DESCRIPTOR   DeviceDescriptor;
  ULONG                        FieldLengthProduct;
  ULONG                        FieldLengthRevision;
  ULONG                        FieldLengthSerialNumber;
  ULONG                        TotalLength;
  ULONG                        TargetId;
  PUCHAR                       Buffer;
  ULONG                        ix;
  PIO_STACK_LOCATION           IoStack = IoGetCurrentIrpStackLocation(Irp);

  DPRINT("AtaXStorageQueryProperty (%p %p)\n", AtaXDevicePdoExtension, Irp);

  ASSERT(IoStack->Parameters.DeviceIoControl.InputBufferLength >= sizeof(STORAGE_PROPERTY_QUERY));
  ASSERT(Irp->AssociatedIrp.SystemBuffer);

  PropertyQuery = (PSTORAGE_PROPERTY_QUERY)Irp->AssociatedIrp.SystemBuffer;

  // поддерживаются только DeviceProperty и AdapterProperty
  if (PropertyQuery->PropertyId != StorageDeviceProperty &&
      PropertyQuery->PropertyId != StorageAdapterProperty)
  {
    Status = STATUS_NOT_IMPLEMENTED;
    DPRINT(" AtaXStorageQueryProperty return - %p \n", Status);
    return Status;
  }

  // поддерживаются только StandardQuery и ExistsQuery
  if ( PropertyQuery->QueryType == PropertyExistsQuery )
  {
    Status = STATUS_SUCCESS;
    DPRINT(" AtaXStorageQueryProperty return - %p \n", Status);
    return Status;
  }

  if ( PropertyQuery->QueryType != PropertyStandardQuery )
  {
    Status = STATUS_NOT_IMPLEMENTED;
    DPRINT(" AtaXStorageQueryProperty return - %p \n", Status);
    return Status;
  }

  if ( PropertyQuery->PropertyId == StorageDeviceProperty )
  {
    // если PropertyQuery->PropertyId == StorageDeviceProperty
    DPRINT("AtaXStorageQueryProperty: PropertyQuery->PropertyId == StorageDeviceProperty\n");
    DPRINT("AtaXStorageQueryProperty: StorageDeviceProperty OutputBufferLength - %x\n", IoStack->Parameters.DeviceIoControl.OutputBufferLength);

    AtaXChannelFdoExtension = (PFDO_CHANNEL_EXTENSION)AtaXDevicePdoExtension->AtaXChannelFdoExtension;
    ASSERT(AtaXChannelFdoExtension);
    ASSERT(AtaXChannelFdoExtension->CommonExtension.IsFDO);

    TargetId = AtaXDevicePdoExtension->TargetId;

    IdentifyData = &AtaXChannelFdoExtension->FullIdentifyData[TargetId];
    InquiryData  = &AtaXChannelFdoExtension->InquiryData[TargetId];

    // подсчитываем размер полей

    if ( AtaXChannelFdoExtension->DeviceFlags[TargetId] & DFLAGS_ATAPI_DEVICE )
    {
      //FieldLengthVendor     = AtaXGetFieldLength(InquiryData->VendorId, 8);
      FieldLengthProduct      = AtaXGetFieldLength(InquiryData->ProductId, 16);
      FieldLengthRevision     = AtaXGetFieldLength(InquiryData->ProductRevisionLevel, 4);
      FieldLengthSerialNumber = AtaXGetFieldLength(IdentifyData->SerialNumber, 20);
    }
    else if ( AtaXChannelFdoExtension->DeviceFlags[TargetId] & DFLAGS_DEVICE_PRESENT )
    {
      //FieldLengthVendor     = AtaXGetFieldLength(InquiryData->VendorId, 0);
      FieldLengthProduct      = AtaXGetFieldLength(IdentifyData->ModelNumber, 16);
      FieldLengthRevision     = AtaXGetFieldLength(IdentifyData->FirmwareRevision, 4);
      FieldLengthSerialNumber = AtaXGetFieldLength(IdentifyData->SerialNumber, 20);
    }

    //DPRINT("AtaXStorageQueryProperty: FieldLengthVendor       - %x\n", FieldLengthVendor);
    DPRINT("AtaXStorageQueryProperty: FieldLengthProduct      - %x\n", FieldLengthProduct);
    DPRINT("AtaXStorageQueryProperty: FieldLengthRevision     - %x\n", FieldLengthRevision);
    DPRINT("AtaXStorageQueryProperty: FieldLengthSerialNumber - %x\n", FieldLengthSerialNumber);

    // sizeof(STORAGE_DEVICE_DESCRIPTOR) = 0x28, INQUIRYDATABUFFERSIZE = 0x24
    TotalLength = sizeof(STORAGE_DEVICE_DESCRIPTOR) + 
                  INQUIRYDATABUFFERSIZE   - 4 +  // UCHAR RawDeviceProperties[1] (+4) (занимает 4 байта, а не 1 (STORAGE_DEVICE_DESCRIPTOR))
                  //FieldLengthVendor     + 1 +  // не используется в ATA/ATAPI
                  FieldLengthProduct      + 1 +  // +1 - завершающий "нулевой" байт
                  FieldLengthRevision     + 1 +
                  FieldLengthSerialNumber + 1;
    DPRINT("AtaXStorageQueryProperty: TotalLength - %x\n", TotalLength);

    if ( IoStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(STORAGE_DEVICE_DESCRIPTOR) )
    {
      // запрос размера
      DescriptorHeader = (PSTORAGE_DESCRIPTOR_HEADER)Irp->AssociatedIrp.SystemBuffer;
      ASSERT(IoStack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(STORAGE_DESCRIPTOR_HEADER));

      // заполняем поля
      DescriptorHeader->Version = sizeof(STORAGE_DEVICE_DESCRIPTOR);
      DescriptorHeader->Size = TotalLength;

      Irp->IoStatus.Information = sizeof(STORAGE_DESCRIPTOR_HEADER);
      DPRINT("AtaXStorageQueryProperty: Irp->IoStatus.Information - %x\n", Irp->IoStatus.Information );
      Status = STATUS_SUCCESS;
      DPRINT(" AtaXStorageQueryProperty return - %p \n", Status);
      return Status;
    }

    // заполняем идентификационные поля
    // копируем данные из IDENTIFY в INQUIRY 

    if ( AtaXChannelFdoExtension->DeviceFlags[TargetId] & DFLAGS_DEVICE_PRESENT )
    {
      RtlZeroMemory(InquiryData, sizeof(INQUIRYDATA));

      InquiryData->DeviceType = DIRECT_ACCESS_DEVICE;

      if ( AtaXChannelFdoExtension->DeviceFlags[TargetId] & DFLAGS_REMOVABLE_DRIVE )
        InquiryData->RemovableMedia = 1;

      for ( ix = 0; ix < FieldLengthProduct; ix += 2 )
      {
        InquiryData->ProductId[ix]     = IdentifyData->ModelNumber[ix + 1];
        InquiryData->ProductId[ix + 1] = IdentifyData->ModelNumber[ix];
      }
      for ( ix = 0; ix < FieldLengthRevision; ix += 2 )
      {
        InquiryData->ProductRevisionLevel[ix]     = IdentifyData->FirmwareRevision[ix + 1];
        InquiryData->ProductRevisionLevel[ix + 1] = IdentifyData->FirmwareRevision[ix];
      }
      for ( ix = 0; ix < FieldLengthSerialNumber; ix += 2 )
      {
        InquiryData->VendorSpecific[ix]     = IdentifyData->SerialNumber[ix + 1];
        InquiryData->VendorSpecific[ix + 1] = IdentifyData->SerialNumber[ix];
      }
    }

    // берем указатель на дескриптор устройства в IRP
    DeviceDescriptor = (PSTORAGE_DEVICE_DESCRIPTOR)Irp->AssociatedIrp.SystemBuffer;
    RtlZeroMemory(DeviceDescriptor, TotalLength);

    // заполним дескриптор
    DeviceDescriptor->Version               = sizeof(STORAGE_DEVICE_DESCRIPTOR); //TotalLength;
    DeviceDescriptor->Size                  = TotalLength;
    DeviceDescriptor->DeviceType            = InquiryData->DeviceType;
    DeviceDescriptor->DeviceTypeModifier    = InquiryData->DeviceTypeModifier;
    DeviceDescriptor->RemovableMedia        = InquiryData->RemovableMedia & 1 ? TRUE : FALSE;
    DeviceDescriptor->CommandQueueing       = FALSE;
    DeviceDescriptor->VendorIdOffset        = 0;
    DeviceDescriptor->ProductIdOffset       = sizeof(STORAGE_DEVICE_DESCRIPTOR) + INQUIRYDATABUFFERSIZE - 4;
    DeviceDescriptor->ProductRevisionOffset = DeviceDescriptor->ProductIdOffset + FieldLengthProduct + 1;
    DeviceDescriptor->SerialNumberOffset    = DeviceDescriptor->ProductRevisionOffset + FieldLengthRevision + 1;
    DeviceDescriptor->RawPropertiesLength   = INQUIRYDATABUFFERSIZE;

   if ( AtaXChannelFdoExtension->DeviceFlags[TargetId] & DFLAGS_DEVICE_PRESENT )
     DeviceDescriptor->BusType              = BusTypeAta;
   if ( AtaXChannelFdoExtension->DeviceFlags[TargetId] & DFLAGS_ATAPI_DEVICE )
     DeviceDescriptor->BusType              = BusTypeAtapi;

    // копируем дескрипторы
    Buffer = (PUCHAR)((ULONG_PTR)DeviceDescriptor + DeviceDescriptor->ProductIdOffset);

    // копируем vendor
    //RtlCopyMemory(Buffer, InquiryData->VendorId, FieldLengthVendor);
    //Buffer[FieldLengthVendor] = '\0';
    //Buffer += FieldLengthVendor + 1;

    // копируем product
    RtlCopyMemory(Buffer, InquiryData->ProductId, FieldLengthProduct);
    Buffer[FieldLengthProduct] = '\0';
    Buffer += FieldLengthProduct + 1;

    // копируем revision
    RtlCopyMemory(Buffer, InquiryData->ProductRevisionLevel, FieldLengthRevision);
    Buffer[FieldLengthRevision] = '\0';
    Buffer += FieldLengthRevision + 1;

    // копируем serial number
    RtlCopyMemory(Buffer, InquiryData->VendorSpecific, FieldLengthSerialNumber);
    Buffer[FieldLengthSerialNumber] = '\0';
    //Buffer += FieldLengthSerialNumber + 1;

    //DPRINT("Vendor %s\n", (LPCSTR)((ULONG_PTR)DeviceDescriptor + DeviceDescriptor->VendorIdOffset));
    DPRINT("Product %s\n",  (LPCSTR)((ULONG_PTR)DeviceDescriptor + DeviceDescriptor->ProductIdOffset));
    DPRINT("Revision %s\n", (LPCSTR)((ULONG_PTR)DeviceDescriptor + DeviceDescriptor->ProductRevisionOffset));
    DPRINT("Serial %s\n",   (LPCSTR)((ULONG_PTR)DeviceDescriptor + DeviceDescriptor->SerialNumberOffset));

    // готово
    Irp->IoStatus.Information = TotalLength;
    DPRINT("return STATUS_SUCCESS\n");
    return STATUS_SUCCESS;

  }
  else
  {
    // если PropertyQuery->PropertyId != StorageDeviceProperty (StorageAdapterProperty)

    ULONG  SizeAdapterDescriptor = sizeof(STORAGE_ADAPTER_DESCRIPTOR);

    DPRINT("AtaXStorageQueryProperty: PropertyQuery->PropertyId == StorageAdapterProperty\n");
    DPRINT("AtaXStorageQueryProperty: StorageAdapterProperty OutputBufferLength - %lu\n", IoStack->Parameters.DeviceIoControl.OutputBufferLength);

    if (IoStack->Parameters.DeviceIoControl.OutputBufferLength < SizeAdapterDescriptor)
    {
      // запрос размера
      DescriptorHeader = (PSTORAGE_DESCRIPTOR_HEADER)Irp->AssociatedIrp.SystemBuffer;
      ASSERT(IoStack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(STORAGE_DESCRIPTOR_HEADER));

      // заполняем поля
      DescriptorHeader->Version = SizeAdapterDescriptor;
      DescriptorHeader->Size    = SizeAdapterDescriptor;

      Irp->IoStatus.Information = sizeof(STORAGE_DESCRIPTOR_HEADER);
      Status = STATUS_SUCCESS;
      DPRINT(" AtaXStorageQueryProperty return - %p \n", Status);
      return Status;
    }

    // берем указатель на дескриптор адаптера в IRP
    AdapterDescriptor = (PSTORAGE_ADAPTER_DESCRIPTOR)Irp->AssociatedIrp.SystemBuffer;

    // заполним дескриптор
    AdapterDescriptor->Version               = SizeAdapterDescriptor;
    AdapterDescriptor->Size                  = SizeAdapterDescriptor;
    AdapterDescriptor->MaximumTransferLength = 0x20000;      //FIXME compute some sane value
    AdapterDescriptor->MaximumPhysicalPages  = MAXULONG;     //FIXME compute some sane value
    AdapterDescriptor->AlignmentMask         = 1;
    AdapterDescriptor->AdapterUsesPio        = TRUE;
    AdapterDescriptor->AdapterScansDown      = FALSE;
    AdapterDescriptor->CommandQueueing       = FALSE;
    AdapterDescriptor->AcceleratedTransfer   = FALSE;
    AdapterDescriptor->BusType               = BusTypeAta;
    AdapterDescriptor->BusMajorVersion       = 1;           //FIXME verify
    AdapterDescriptor->BusMinorVersion       = 0;           //FIXME

    // размер ставим в IoStatus
    Irp->IoStatus.Information = SizeAdapterDescriptor;
    Status = STATUS_SUCCESS;
  }

  DPRINT(" AtaXStorageQueryProperty return - %p \n", Status);
  return Status;
}

NTSTATUS
AtaReadDriveCapacity(
    IN PPDO_DEVICE_EXTENSION AtaXDevicePdoExtension,
    IN PIRP Irp,
    IN PSCSI_REQUEST_BLOCK Srb)
{
  PFDO_CHANNEL_EXTENSION  AtaXChannelFdoExtension;
  ULONG                   LastSector;
  ULONG                   BytesPerSector;
  ULONG                   tmp;
  NTSTATUS                Status;

  DPRINT("AtaReadDriveCapacity: AtaXDevicePdoExtension - %p, Irp - %p, Srb - %p\n", AtaXDevicePdoExtension, Irp, Srb);

  AtaXChannelFdoExtension = AtaXDevicePdoExtension->AtaXChannelFdoExtension;

  DPRINT("device %x - #sectors %x, #heads %x, #cylinders %x\n",
          Srb->TargetId,
          AtaXChannelFdoExtension->FullIdentifyData[Srb->TargetId].NumSectorsPerTrack,
          AtaXChannelFdoExtension->FullIdentifyData[Srb->TargetId].NumHeads,
          AtaXChannelFdoExtension->FullIdentifyData[Srb->TargetId].NumCylinders);

  DPRINT("AtaReadDriveCapacity: UserAddressableSectors - %p \n", AtaXChannelFdoExtension->FullIdentifyData[Srb->TargetId].UserAddressableSectors);
  DPRINT("AtaReadDriveCapacity: Max48BitLBA[0]         - %p \n", AtaXChannelFdoExtension->FullIdentifyData[Srb->TargetId].Max48BitLBA[0]);

  //
  tmp = 0x200; //512

  ((PFOUR_BYTE)&BytesPerSector)->Byte0 = ((PFOUR_BYTE)&tmp)->Byte3;
  ((PFOUR_BYTE)&BytesPerSector)->Byte1 = ((PFOUR_BYTE)&tmp)->Byte2;
  ((PFOUR_BYTE)&BytesPerSector)->Byte2 = ((PFOUR_BYTE)&tmp)->Byte1;
  ((PFOUR_BYTE)&BytesPerSector)->Byte3 = ((PFOUR_BYTE)&tmp)->Byte0;

  ((PREAD_CAPACITY_DATA)Srb->DataBuffer)->BytesPerBlock = BytesPerSector;

  //
  tmp = AtaXChannelFdoExtension->FullIdentifyData[Srb->TargetId].UserAddressableSectors - 1;

  ((PFOUR_BYTE)&LastSector)->Byte0 = ((PFOUR_BYTE)&tmp)->Byte3;
  ((PFOUR_BYTE)&LastSector)->Byte1 = ((PFOUR_BYTE)&tmp)->Byte2;
  ((PFOUR_BYTE)&LastSector)->Byte2 = ((PFOUR_BYTE)&tmp)->Byte1;
  ((PFOUR_BYTE)&LastSector)->Byte3 = ((PFOUR_BYTE)&tmp)->Byte0;
  
  ((PREAD_CAPACITY_DATA)Srb->DataBuffer)->LogicalBlockAddress = LastSector;

  Srb->SrbStatus = SRB_STATUS_SUCCESS;
  Irp->IoStatus.Information = 8;

  Status = STATUS_SUCCESS;
  Irp->IoStatus.Status = Status;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);

  DPRINT("AtaReadDriveCapacity return - %p \n", Status);
  return Status;
}

NTSTATUS
AtaModeSense(
    IN PPDO_DEVICE_EXTENSION AtaXDevicePdoExtension,
    IN PSCSI_REQUEST_BLOCK Srb)
{
  PFDO_CHANNEL_EXTENSION  AtaXChannelFdoExtension;
  PATAX_REGISTERS_1       AtaXRegisters1;
  PATAX_REGISTERS_2       AtaXRegisters2;
  PIRP                    Irp;
  UCHAR                   StatusByte;
  UCHAR                   ErrorByte;
  PMODE_PARAMETER_HEADER  ModeData;
  NTSTATUS                Status;

  DPRINT("AtaModeSense: ...\n");

  ASSERT(AtaXDevicePdoExtension);

  AtaXChannelFdoExtension = AtaXDevicePdoExtension->AtaXChannelFdoExtension;
  AtaXRegisters1 = &AtaXChannelFdoExtension->BaseIoAddress1;
  AtaXRegisters2 = &AtaXChannelFdoExtension->BaseIoAddress2;

  // Это используется, чтобы определить является ли носитель защищенным от записи.
  // Так как ATA не поддерживает MODE_SENSE, то мы изменим только часть, которая нужна
  // так высокоуровневый драйвер может определить, защищен ли носитель.
  
  if ( AtaXChannelFdoExtension->DeviceFlags[Srb->TargetId] & DFLAGS_MEDIA_STATUS_ENABLED )
  {
    // выбираем устройство
    WRITE_PORT_UCHAR(AtaXRegisters1->DriveSelect, (UCHAR)(((Srb->TargetId & 1) << 4) | IDE_DRIVE_SELECT));
    WRITE_PORT_UCHAR(AtaXRegisters1->Command, IDE_COMMAND_GET_MEDIA_STATUS);

    AtaXWaitOnBusy(AtaXRegisters2);
    StatusByte = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus);
    DPRINT(" AtaModeSense: StatusByte - %x\n", StatusByte);
 
    if ( !(StatusByte & IDE_STATUS_ERROR) )
    {
      // ошибки нет, значит нет защиты носителя от записи
      AtaXChannelFdoExtension->ExpectingInterrupt = FALSE;
      Status = SRB_STATUS_SUCCESS;
    }
    else
    {
      // произошла ошибка, обрабатываем ее локально и сбрасываем прерывание

      ErrorByte = READ_PORT_UCHAR(AtaXRegisters1->Error);
      DPRINT(" AtaModeSense: ErrorByte - %x\n", ErrorByte);
  
      // cбрасываем прерывание, читая регистр состояние
      StatusByte = READ_PORT_UCHAR(AtaXRegisters1->Status);
      DPRINT(" AtaModeSense: StatusByte - %x\n", StatusByte);

      AtaXChannelFdoExtension->ExpectingInterrupt = FALSE;
      Status = SRB_STATUS_SUCCESS;
  
      if ( ErrorByte & IDE_ERROR_DATA_ERROR )
      {
        // носитель является защищенными от записи
        ModeData = (PMODE_PARAMETER_HEADER)Srb->DataBuffer;
   
        Srb->DataTransferLength = sizeof(MODE_PARAMETER_HEADER);
        ModeData->DeviceSpecificParameter |= MODE_DSP_WRITE_PROTECT; // устанавливаем бит в буфере MODE_SENSE
      }
    }

    Status = SRB_STATUS_SUCCESS;
  }
  else
  {
    Irp = Srb->OriginalRequest;
    ASSERT(Irp);
    Srb->DataTransferLength -= 4;
    Irp->IoStatus.Information = Srb->DataTransferLength;
    Status = STATUS_SUCCESS;
    Srb->SrbStatus = SRB_STATUS_SUCCESS;
  }

  Irp->IoStatus.Status = Status;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);

  DPRINT(" AtaModeSense: return - %x\n", Status);
  return Status;
}

NTSTATUS
AhciModeSense(
    IN PPDO_DEVICE_EXTENSION AtaXDevicePdoExtension,
    IN PSCSI_REQUEST_BLOCK Srb)
{
  PFDO_CHANNEL_EXTENSION  AtaXChannelFdoExtension;
  PATAX_REGISTERS_1       AtaXRegisters1;
  PATAX_REGISTERS_2       AtaXRegisters2;
  PIRP                    Irp;
  UCHAR                   StatusByte;
  UCHAR                   ErrorByte;
  PMODE_PARAMETER_HEADER  ModeData;
  NTSTATUS                Status;

  ASSERT(AtaXDevicePdoExtension);

  DPRINT("AhciModeSense: AtaXDevicePdoExtension - %p, Srb - %p\n", AtaXDevicePdoExtension, Srb);

  AtaXChannelFdoExtension = AtaXDevicePdoExtension->AtaXChannelFdoExtension;
  AtaXRegisters1 = &AtaXChannelFdoExtension->BaseIoAddress1;
  AtaXRegisters2 = &AtaXChannelFdoExtension->BaseIoAddress2;

  // Это используется, чтобы определить является ли носитель защищенным от записи.
  // Так как ATA не поддерживает MODE_SENSE, то мы изменим только часть, которая нужна
  // так высокоуровневый драйвер может определить, защищен ли носитель.
  
  if ( AtaXChannelFdoExtension->DeviceFlags[Srb->TargetId] & DFLAGS_MEDIA_STATUS_ENABLED )
  {
    WRITE_PORT_UCHAR(AtaXRegisters1->DriveSelect, (UCHAR)(((Srb->TargetId & 1) << 4) | IDE_DRIVE_SELECT)); // выбираем устройство
    WRITE_PORT_UCHAR(AtaXRegisters1->Command, IDE_COMMAND_GET_MEDIA_STATUS);

    AtaXWaitOnBusy(AtaXRegisters2);
    StatusByte = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus);
    DPRINT(" AhciModeSense: StatusByte - %x\n", StatusByte);
 
    if ( !(StatusByte & IDE_STATUS_ERROR) )
    {
      // ошибки нет, значит нет защиты носителя от записи
      AtaXChannelFdoExtension->ExpectingInterrupt = FALSE;
      Status = SRB_STATUS_SUCCESS;
    }
    else
    {
      // произошла ошибка, обрабатываем ее локально и сбрасываем прерывание
      ErrorByte = READ_PORT_UCHAR(AtaXRegisters1->Error);
      DPRINT(" AhciModeSense: ErrorByte - %x\n", ErrorByte);
  
      StatusByte = READ_PORT_UCHAR(AtaXRegisters1->Status);  // cбрасываем прерывание, читая регистр состояние
      DPRINT(" AhciModeSense: StatusByte - %x\n", StatusByte);

      AtaXChannelFdoExtension->ExpectingInterrupt = FALSE;
      Status = SRB_STATUS_SUCCESS;

//FIXME?  
      if ( 0)//ErrorByte & IDE_ERROR_DATA_ERROR )
      {
        // носитель является защищенными от записи
        ModeData = (PMODE_PARAMETER_HEADER)Srb->DataBuffer;
   
        Srb->DataTransferLength = sizeof(MODE_PARAMETER_HEADER);
        ModeData->DeviceSpecificParameter |= MODE_DSP_WRITE_PROTECT; // устанавливаем бит в буфере MODE_SENSE
      }
    }
    Status = SRB_STATUS_SUCCESS;
  }
  else
  {
    Irp = Srb->OriginalRequest;
    ASSERT(Irp);
    Srb->DataTransferLength -= 4;
    Irp->IoStatus.Information = Srb->DataTransferLength;
    Status = STATUS_SUCCESS;
    Srb->SrbStatus = SRB_STATUS_SUCCESS;
  }

  Irp->IoStatus.Status = Status;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);

  DPRINT(" AhciModeSense: return - %x\n", Status);
  return Status;
}

NTSTATUS
AtaXDevicePdoDeviceControl(
    IN PDEVICE_OBJECT AtaXDevicePdo,
    IN PIRP Irp)
{
  PPDO_DEVICE_EXTENSION   AtaXDevicePdoExtension;
  PFDO_CHANNEL_EXTENSION  AtaXChannelFdoExtension;
  ULONG                   IoctlCode;
  ULONG                   BaseCode;
  ULONG                   FunctionCode;
  NTSTATUS                Status;
  PIO_STACK_LOCATION      IoStack = IoGetCurrentIrpStackLocation(Irp);

  DPRINT("\n");
  DPRINT(" AtaXDevicePdoDeviceControl: AtaXDevicePdo - %p, Irp - %p\n", AtaXDevicePdo, Irp);

  AtaXDevicePdoExtension = AtaXDevicePdo->DeviceExtension;
  ASSERT(AtaXDevicePdoExtension);
  ASSERT(AtaXDevicePdoExtension->CommonExtension.IsFDO == FALSE);

  AtaXChannelFdoExtension = (PFDO_CHANNEL_EXTENSION)AtaXDevicePdoExtension->AtaXChannelFdoExtension;
  ASSERT(AtaXChannelFdoExtension);
  ASSERT(AtaXChannelFdoExtension->CommonExtension.IsFDO);

  IoctlCode    = IoStack->Parameters.DeviceIoControl.IoControlCode;;
  BaseCode     = IoctlCode >> 16;
  FunctionCode = (IoctlCode & (~0xFFFFC003)) >> 2;
  DPRINT("AtaXDevicePdoDeviceControl: Ioctl Code = %lx, Base Code = %lx, Function Code = %lx\n", IoctlCode, BaseCode, FunctionCode);

  Irp->IoStatus.Information = 0;

  switch( IoctlCode )
  {
    case IOCTL_STORAGE_QUERY_PROPERTY:                 /* 0x2d1400 */
      DPRINT("IRP_MJ_DEVICE_CONTROL / IOCTL_STORAGE_QUERY_PROPERTY\n");
      Status = AtaXStorageQueryProperty(AtaXDevicePdoExtension, Irp);
      break;

    case IOCTL_SCSI_GET_INQUIRY_DATA:                  /* 0x04100C */
    {
ASSERT(FALSE);
      Status = STATUS_SUCCESS;
    } 
    break;

    case IOCTL_SCSI_GET_CAPABILITIES:                  /* 0x041010 */
    {
      DPRINT("IRP_MJ_DEVICE_CONTROL / IOCTL_SCSI_GET_CAPABILITIES\n");
ASSERT(FALSE);
      Status = 0;
    } 
    break;

    case IOCTL_SCSI_GET_ADDRESS:                       /* 0x041018 */
    {
      PSCSI_ADDRESS Address = Irp->AssociatedIrp.SystemBuffer;

      DPRINT("IRP_MJ_DEVICE_CONTROL / IOCTL_SCSI_GET_ADDRESS\n");
      if ( IoStack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(SCSI_ADDRESS) )
      {
        Address->Length     = sizeof(SCSI_ADDRESS);
        Address->PortNumber = AtaXChannelFdoExtension->Channel;
        Address->PathId     = AtaXDevicePdoExtension->PathId;
        Address->TargetId   = AtaXDevicePdoExtension->TargetId;
        Address->Lun        = AtaXDevicePdoExtension->Lun;
  
        Irp->IoStatus.Information = sizeof(SCSI_ADDRESS);
        Irp->IoStatus.Status      = STATUS_SUCCESS;

        Status = STATUS_SUCCESS;
      }
      else
      {
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status      = STATUS_BUFFER_TOO_SMALL;

        Status = STATUS_BUFFER_TOO_SMALL;
      }

      break;
    }

    case IOCTL_VOLUME_GET_GPT_ATTRIBUTES:              /* 0x560038 */
      DPRINT("IRP_MJ_DEVICE_CONTROL / IOCTL_VOLUME_GET_GPT_ATTRIBUTES FIXME\n");
      Status = STATUS_NOT_SUPPORTED;
      break;

    case IOCTL_MOUNTDEV_QUERY_STABLE_GUID:             /* 0x4d0018 */
      DPRINT("IRP_MJ_DEVICE_CONTROL / IOCTL_MOUNTDEV_QUERY_STABLE_GUID FIXME\n");
      Status = STATUS_NOT_SUPPORTED;
      break;


    default:
      DPRINT("IRP_MJ_DEVICE_CONTROL / Unknown IoctlCode FIXME%x\n", IoctlCode);
      ASSERT(FALSE);
      Status = STATUS_NOT_SUPPORTED;
  }

  Irp->IoStatus.Status = Status;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  DPRINT(" AtaXDevicePdoDeviceControl return - %p \n", Status);
  return Status;
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
          if ( AtaXChannelFdoExtension->DeviceFlags[Srb->TargetId] & DFLAGS_ATAPI_DEVICE ) //if ATAPI
            break;// --> IoStartPacket
          Status = AtaXSendCommand(AtaXChannelFdoExtension, Srb);
          return Status;

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
          if ( AtaXChannelFdoExtension->DeviceFlags[Srb->TargetId] & DFLAGS_ATAPI_DEVICE ) //if ATAPI
          {
            break;// --> IoStartPacket
          }
          else if ( AtaXChannelFdoExtension->DeviceFlags[Srb->TargetId] & DFLAGS_DEVICE_PRESENT ) //if ATA
          {
            if ( AtaXChannelFdoExtension->AhciInterface )
              Status = AhciModeSense(AtaXDevicePdoExtension, Srb);
            else
              Status = AtaModeSense(AtaXDevicePdoExtension, Srb);
  
            DPRINT(" AtaXDevicePdoDispatchScsi: return - %p\n", Status);
            return Status;
          }
          else //Error
          {
            DPRINT(" AtaXDevicePdoDispatchScsi: Error. DeviceFlags - %p\n", AtaXChannelFdoExtension->DeviceFlags[Srb->TargetId]);
            ASSERT(FALSE);
            break;  // --> IoStartPacket
          }

        case SCSIOP_MODE_SENSE10:       /* 0x5A */
          DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_EXECUTE_SCSI / SCSIOP_MODE_SENSE10 (PIO mode) \n");
          Srb->SrbFlags &= ~SRB_FLAGS_USE_DMA;  // PIO mode
          break;// --> IoStartPacket

        case SCSIOP_READ_CAPACITY:      /* 0x25 */
          DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_EXECUTE_SCSI / SCSIOP_READ_CAPACITY\n");
          if ( AtaXChannelFdoExtension->DeviceFlags[Srb->TargetId] & DFLAGS_ATAPI_DEVICE ) //if ATAPI
          {
            break;  // --> IoStartPacket
          }
          else if ( AtaXChannelFdoExtension->DeviceFlags[Srb->TargetId] & DFLAGS_DEVICE_PRESENT ) //if ATA
          {
            Status = AtaReadDriveCapacity(AtaXDevicePdoExtension, Irp, Srb);
            DPRINT(" AtaXDevicePdoDispatchScsi: return - %p\n", Status);
            DPRINT("\n");
            return Status;
          }
          else //Error
          {
            DPRINT(" AtaXDevicePdoDispatchScsi: Error. DeviceFlags - %p\n", AtaXChannelFdoExtension->DeviceFlags[Srb->TargetId]);
            ASSERT(FALSE);
            break;  // --> IoStartPacket
          }

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
      DPRINT1("IRP_MJ_SCSI / SRB_FUNCTION_CLAIM_DEVICE \n");
      if ( AtaXDevicePdoExtension->Claimed )  // если устройство уже захвачено 
      {
        Status = STATUS_DEVICE_BUSY;
        Srb->SrbStatus = SRB_STATUS_BUSY;
        break;
      }
      AtaXDevicePdoExtension->Claimed = TRUE;  // устройство захвачено
      Srb->DataBuffer = AtaXDevicePdo;
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
    {
      PIO_STACK_LOCATION      Stack;
      PKDEVICE_QUEUE_ENTRY    Entry;
      PIRP                    NextIrp, IrpList;
      KIRQL                   Irql;

      DPRINT("IRP_MJ_SCSI / SRB_FUNCTION_FLUSH_QUEUE \n");

      KeAcquireSpinLock(&AtaXChannelFdoExtension->SpinLock, &Irql);

      if ( !(AtaXDevicePdoExtension->Flags & LUNEX_FROZEN_QUEUE) )
      {
        DPRINT("Queue is not frozen really\n");
        KeReleaseSpinLock(&AtaXChannelFdoExtension->SpinLock, Irql);
        Status = STATUS_INVALID_DEVICE_REQUEST;
        break;
      }

      ASSERT(AtaXDevicePdoExtension->SrbInfo.Srb == NULL); // нет активных запросов

      // список Irp из очереди устройства

      IrpList = NULL;

      while ( (Entry = KeRemoveDeviceQueue(&AtaXDevicePdoExtension->DeviceQueue)) != NULL )
      {
        NextIrp = CONTAINING_RECORD(Entry, IRP, Tail.Overlay.DeviceQueueEntry);

        Stack = IoGetCurrentIrpStackLocation(NextIrp);
        Srb = Stack->Parameters.Scsi.Srb;

        Srb->SrbStatus = SRB_STATUS_REQUEST_FLUSHED;
        NextIrp->IoStatus.Status = STATUS_UNSUCCESSFUL;

        NextIrp->Tail.Overlay.ListEntry.Flink = (PLIST_ENTRY)IrpList;
        IrpList = NextIrp;
      }

      AtaXDevicePdoExtension->Flags &= ~LUNEX_FROZEN_QUEUE;         // "разморозка" очереди
      KeReleaseSpinLock(&AtaXChannelFdoExtension->SpinLock, Irql);

      // завершим запросы
      while ( IrpList )
      {
        NextIrp = IrpList;
        IrpList = (PIRP)NextIrp->Tail.Overlay.ListEntry.Flink;
        IoCompleteRequest(NextIrp, 0);
      }

      Status = STATUS_SUCCESS;
      break;
    }

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
AtaXDevicePdoQueryId(
    IN PDEVICE_OBJECT AtaXDevicePdo,
    IN PIRP Irp)
{
  WCHAR                   Buffer[256];
  ULONG                   Index = 0;
  UNICODE_STRING          SourceString;
  UNICODE_STRING          String;
  NTSTATUS                Status;
  ULONG                   IdType;
  PPDO_DEVICE_EXTENSION   AtaXDevicePdoExtension;
  PFDO_CHANNEL_EXTENSION  AtaXChannelFdoExtension;

  DPRINT("AtaXDevicePdoQueryId: FIXME (%p %p)\n", AtaXDevicePdo, Irp);
  // FIXME - Id надо формировать из AtaXChannelFdoExtension->FullIdentifyData[DeviceNumber]

  AtaXDevicePdoExtension = (PPDO_DEVICE_EXTENSION)AtaXDevicePdo->DeviceExtension;
  AtaXChannelFdoExtension = (PFDO_CHANNEL_EXTENSION)AtaXDevicePdoExtension->AtaXChannelFdoExtension;

  DPRINT("AtaXDevicePdoQueryId: AtaXDevicePdoExtension->TargetId - %x\n", AtaXDevicePdoExtension->TargetId);
  DPRINT("AtaXDevicePdoQueryId: AtaXChannelFdoExtension->DeviceFlags[HwExtension->TargetId] - %p\n", AtaXChannelFdoExtension->DeviceFlags[AtaXDevicePdoExtension->TargetId]);

  if ( !(AtaXChannelFdoExtension->DeviceFlags[AtaXDevicePdoExtension->TargetId] & DFLAGS_DEVICE_PRESENT) )
    return STATUS_NOT_SUPPORTED;

  IdType = IoGetCurrentIrpStackLocation(Irp)->Parameters.QueryId.IdType;

  switch (IdType)
  {
    case BusQueryDeviceID:
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_ID / BusQueryDeviceID\n");
      if ( AtaXChannelFdoExtension->DeviceFlags[AtaXDevicePdoExtension->TargetId] & DFLAGS_DEVICE_PRESENT );
      {
        if ( AtaXChannelFdoExtension->DeviceFlags[AtaXDevicePdoExtension->TargetId] & DFLAGS_ATAPI_DEVICE )
          RtlInitUnicodeString(&SourceString, L"IDE\\CdRomVBOX_CD-ROM_____________________________1.0_____");
        else
          RtlInitUnicodeString(&SourceString, L"IDE\\DiskVBOX_HARDDISK___________________________1.0_____");
      }
      break;

    case BusQueryHardwareIDs:
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_ID / BusQueryHardwareIDs\n");
      if ( AtaXChannelFdoExtension->DeviceFlags[AtaXDevicePdoExtension->TargetId] & DFLAGS_DEVICE_PRESENT );
      {
        if ( AtaXChannelFdoExtension->DeviceFlags[AtaXDevicePdoExtension->TargetId] & DFLAGS_ATAPI_DEVICE )
        {
          //Index += swprintf(&Buffer[Index], L"IDE\\CdRomVBOX_CD-ROM_____________________________1.0_____") + 1;
          //Index += swprintf(&Buffer[Index], L"IDE\\VBOX_CD-ROM_____________________________1.0_____") + 1;
          //Index += swprintf(&Buffer[Index], L"IDE\\CdRomVBOX_CD-ROM_____________________________") + 1;
          //Index += swprintf(&Buffer[Index], L"VBOX_CD-ROM_____________________________1.0_____") + 1;
          Index += swprintf(&Buffer[Index], L"GenCdRom") + 1;
        }          
        else
        {
          //Index += swprintf(&Buffer[Index], L"IDE\\DiskVBOX_HARDDISK___________________________1.0_____") + 1;
          //Index += swprintf(&Buffer[Index], L"IDE\\VBOX_HARDDISK___________________________1.0_____") + 1;
          //Index += swprintf(&Buffer[Index], L"IDE\\DiskVBOX_HARDDISK___________________________") + 1;
          //Index += swprintf(&Buffer[Index], L"VBOX_HARDDISK___________________________1.0_____") + 1;
          Index += swprintf(&Buffer[Index], L"GenDisk") + 1;
        }          

        Buffer[Index] = UNICODE_NULL;
        SourceString.Length = SourceString.MaximumLength = Index * sizeof(WCHAR);
        SourceString.Buffer = Buffer;
      }
      break;

    case BusQueryCompatibleIDs:
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_ID / BusQueryCompatibleIDs\n");
      if ( AtaXChannelFdoExtension->DeviceFlags[AtaXDevicePdoExtension->TargetId] & DFLAGS_DEVICE_PRESENT );
      {
        if ( AtaXChannelFdoExtension->DeviceFlags[AtaXDevicePdoExtension->TargetId] & DFLAGS_ATAPI_DEVICE )
          Index += swprintf(&Buffer[Index], L"GenCdRom") + 1;
        else
          Index += swprintf(&Buffer[Index], L"GenDisk") + 1;
      }

      Buffer[Index] = UNICODE_NULL;
      SourceString.Length = SourceString.MaximumLength = Index * sizeof(WCHAR);
      SourceString.Buffer = Buffer;
      break;

    case BusQueryInstanceID:
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_ID / BusQueryInstanceID\n");
      swprintf(Buffer, L"%lu", AtaXDevicePdoExtension->TargetId);
      RtlInitUnicodeString(&SourceString, Buffer);
      break;

    default:
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_ID / unknown query id type 0x%lx\n", IdType);
      ASSERT(FALSE);
      return STATUS_NOT_SUPPORTED;

  }

  Status = DuplicateUnicodeString(
               RTL_DUPLICATE_UNICODE_STRING_NULL_TERMINATE,
               &SourceString,
               &String);

  DPRINT(" AtaXDevicePdoQueryId return - %p \n", Status);
  Irp->IoStatus.Information = (ULONG_PTR)String.Buffer;
  return Status;
}

NTSTATUS
AtaXDevicePdoQueryDeviceText(
    IN PDEVICE_OBJECT AtaXDevicePdo,
    IN PIRP Irp)
{
  PPDO_DEVICE_EXTENSION   AtaXDevicePdoExtension;
  PFDO_CHANNEL_EXTENSION  AtaXChannelFdoExtension;
  ULONG                   DeviceTextType;
  PCWSTR                  SourceString;
  UNICODE_STRING          String;

  DPRINT("AtaXDevicePdoQueryDeviceText (%p %p)\n", AtaXDevicePdo, Irp);

  DeviceTextType = IoGetCurrentIrpStackLocation(Irp)->Parameters.QueryDeviceText.DeviceTextType;
  AtaXDevicePdoExtension = (PPDO_DEVICE_EXTENSION)AtaXDevicePdo->DeviceExtension;
  AtaXChannelFdoExtension = (PFDO_CHANNEL_EXTENSION)AtaXDevicePdoExtension->AtaXChannelFdoExtension;

  switch ( DeviceTextType )
  {
    case DeviceTextDescription:
    {
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_DEVICE_TEXT / DeviceTextDescription\n");

      if ( AtaXChannelFdoExtension->DeviceFlags[AtaXDevicePdoExtension->TargetId] & DFLAGS_DEVICE_PRESENT );
      {
        if ( AtaXChannelFdoExtension->DeviceFlags[AtaXDevicePdoExtension->TargetId] & DFLAGS_ATAPI_DEVICE )
          SourceString = L"VBOX CD-ROM";
        else
          SourceString = L"VBOX HARDDISK";
      }

      break;
    }

    case DeviceTextLocationInformation:
    {
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_DEVICE_TEXT / DeviceTextLocationInformation\n");
      SourceString = L"0";
      break;
    }

    default:
    {
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_DEVICE_TEXT / unknown type 0x%lx\n", DeviceTextType);
      ASSERT(FALSE);
      return STATUS_NOT_SUPPORTED;
    }
  }

  if ( RtlCreateUnicodeString(&String, SourceString) )
  {
    Irp->IoStatus.Information = (ULONG_PTR)String.Buffer;
    DPRINT(" AtaXDevicePdoQueryDeviceText return - %p \n", STATUS_SUCCESS);
    return STATUS_SUCCESS;
  }
  else
  {
    DPRINT(" AtaXDevicePdoQueryDeviceText return - STATUS_INSUFFICIENT_RESOURCES\n");
    return STATUS_INSUFFICIENT_RESOURCES;
  }
}

NTSTATUS
AtaXDevicePdoStartDevice(
    IN PDEVICE_OBJECT AtaXDevicePdo,
    IN PIRP Irp)
{
  NTSTATUS  Status = STATUS_SUCCESS;
  DPRINT("AtaXDevicePdoStartDevice (%p %p)\n", AtaXDevicePdo, Irp);

  DPRINT(" AtaXDevicePdoStartDevice return - %p \n", Status);
  return Status;
}

NTSTATUS
AtaXDevicePdoQueryPnPDeviceState(
    IN PDEVICE_OBJECT AtaXDevicePdo,
    IN PIRP Irp)
{
  PPDO_DEVICE_EXTENSION  AtaXDevicePdoExtension;
  NTSTATUS               Status = STATUS_SUCCESS;

  DPRINT("AtaXDevicePdoQueryPnPDeviceState (%p %p)\n", AtaXDevicePdo, Irp);

  AtaXDevicePdoExtension = AtaXDevicePdo->DeviceExtension;
  if ( AtaXDevicePdoExtension )
  {
    if ( AtaXDevicePdoExtension->DeviceNotDisableable )
      Irp->IoStatus.Information |= PNP_DEVICE_NOT_DISABLEABLE; //0x20;
  }
  else
  {
    Status = STATUS_DEVICE_DOES_NOT_EXIST;
  }

  DPRINT(" AtaXDevicePdoQueryPnPDeviceState return - %p \n", Status);
  return Status;
}

NTSTATUS
AtaXDevicePdoQueryDeviceRelations(
    IN PDEVICE_OBJECT AtaXDevicePdo,
    IN PIRP Irp)
{
  NTSTATUS  Status = STATUS_SUCCESS;
  DPRINT("AtaXDevicePdoQueryDeviceRelations (%p %p)\n", AtaXDevicePdo, Irp);

  DPRINT(" AtaXDevicePdoQueryDeviceRelations return - %p \n", Status);
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
      Status = AtaXDevicePdoStartDevice(AtaXDevicePdo, Irp);
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
      Status = AtaXDevicePdoQueryDeviceRelations(AtaXDevicePdo, Irp);
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
      Status = AtaXDevicePdoQueryDeviceText(AtaXDevicePdo, Irp);
      break;

    case IRP_MN_QUERY_ID:                     /* 0x13 */  //AtaXDevicePdoQueryId
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_ID\n");
      Status = AtaXDevicePdoQueryId(AtaXDevicePdo, Irp);
      break;

    case IRP_MN_QUERY_PNP_DEVICE_STATE:       /* 0x14 */  //AtaXDevicePdoQueryPnPDeviceState
      DPRINT("IRP_MJ_PNP / IRP_MN_QUERY_PNP_DEVICE_STATE\n");
      Status = AtaXDevicePdoQueryPnPDeviceState(AtaXDevicePdo, Irp);
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
