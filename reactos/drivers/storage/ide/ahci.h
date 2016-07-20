#ifndef _AHCI_PCH_
#define _AHCI_PCH_

#include <ntddk.h>
#include <ide.h>
#include <srb.h>


typedef struct _AHCI_INTERRUPT_RESOURCE {

  UCHAR        InterruptShareDisposition;
  USHORT       InterruptFlags;
  ULONG        InterruptLevel;
  ULONG        InterruptVector;
  KAFFINITY    InterruptAffinity;
  PKINTERRUPT  InterruptObject;

} AHCI_INTERRUPT_RESOURCE, *PAHCI_INTERRUPT_RESOURCE;

typedef struct _AHCI_INTERFACE {

  // generic interface header
  USHORT                    Size;
  USHORT                    Version;
  PVOID                     Context;
  PINTERFACE_REFERENCE      InterfaceReference;
  PINTERFACE_DEREFERENCE    InterfaceDereference;

  // controller interface
  PVOID                     ChannelPdoExtension;
  PAHCI_INTERRUPT_RESOURCE  InterruptResource;


} AHCI_INTERFACE, *PAHCI_INTERFACE;

#endif /* _AHCI_PCH_ */
