#ifndef _BMASTER_PCH_
#define _BMASTER_PCH_


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

} BUS_MASTER_INTERFACE, *PBUS_MASTER_INTERFACE;


#endif /* _BMASTER_PCH_ */
