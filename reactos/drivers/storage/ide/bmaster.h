#ifndef _BMASTER_PCH_
#define _BMASTER_PCH_


typedef NTSTATUS
(NTAPI *PBUS_MASTER_PREPARE)(
  IN PVOID           DeviceExtension,
  IN PVOID           CurrentVirtualAddress,
  IN ULONG           Length,
  IN PMDL            Mdl,
  IN BOOLEAN         WriteToDevice,
  IN PVOID           AllocateAdapter,
  IN PDEVICE_OBJECT  AtaXChannelFdo);

typedef NTSTATUS
(NTAPI *PBUS_MASTER_START)(IN PVOID  DeviceExtension);

typedef NTSTATUS
(NTAPI *PBUS_MASTER_STOP)(IN PVOID  DeviceExtension);

typedef NTSTATUS
(NTAPI *PBUS_MASTER_READ_STATUS)(IN PVOID  DeviceExtension);

typedef NTSTATUS
(NTAPI *PBUS_MASTER_COMPLETE)(IN PVOID  DeviceExtension);

typedef struct _BUS_MASTER_INTERFACE {

  // generic interface header
  USHORT                   Size;
  USHORT                   Version;
  PVOID                    Context;
  PINTERFACE_REFERENCE     InterfaceReference;
  PINTERFACE_DEREFERENCE   InterfaceDereference;

  // bus master interface
  PVOID                    ChannelPdoExtension;
  ULONG                    BusMasterBase;
  PBUS_MASTER_PREPARE      BusMasterPrepare;
  PBUS_MASTER_START        BusMasterStart;
  PBUS_MASTER_STOP         BusMasterStop;
  PBUS_MASTER_READ_STATUS  BusMasterReadStatus;
  PBUS_MASTER_COMPLETE     BusMasterComplete;

} BUS_MASTER_INTERFACE, *PBUS_MASTER_INTERFACE;

typedef struct _PHYSICAL_REGION_DESCRIPTOR {

  ULONG   BaseAddress;
  USHORT  ByteCount;
  USHORT  EndTable;

} PHYSICAL_REGION_DESCRIPTOR, *PPHYSICAL_REGION_DESCRIPTOR;

typedef struct _PHYSICAL_REGION_DESCRIPTOR_TABLE {

  PHYSICAL_REGION_DESCRIPTOR  Elements[1];

} PHYSICAL_REGION_DESCRIPTOR_TABLE, *PPHYSICAL_REGION_DESCRIPTOR_TABLE;

#endif /* _BMASTER_PCH_ */
