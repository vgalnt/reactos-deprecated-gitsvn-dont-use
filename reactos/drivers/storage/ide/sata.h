#ifndef _SATA_PCH_
#define _SATA_PCH_


typedef struct _SATA_INTERFACE {

  // generic interface header
  USHORT                    Size;
  USHORT                    Version;
  PVOID                     Context;
  PINTERFACE_REFERENCE      InterfaceReference;
  PINTERFACE_DEREFERENCE    InterfaceDereference;

  // controller interface
  PVOID                     ChannelPdoExtension;
  ULONG                     SataBaseAddress;
  PSATA_INTERRUPT_RESOURCE  InterruptResource;

} SATA_INTERFACE, *PSATA_INTERFACE;

#endif /* _SATA_PCH_ */
