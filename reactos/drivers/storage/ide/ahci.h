#ifndef _AHCI_PCH_
#define _AHCI_PCH_

#include <ntddk.h>
#include <ide.h>
#include <srb.h>


typedef struct _AHCI_MEMORY_REGISTERS {
                                                   // 0x0000 ... 0x002B - Generic Host Control :
  AHCI_HOST_CAPABILITIES    HostCapabilities;      // 0x0000, CAP
  AHCI_HOST_GLOBAL_CONTROL  GlobalHostControl;     // 0x0004, GHC
  ULONG	                    InterruptStatus;       // 0x0008, IS
  ULONG	                    PortsImplemented;      // 0x000C, PI
  ULONG	                    Version;               // 0x0010, VS
  ULONG	                    CmdCompletionControl;  // 0x0014, CCC_CTL    Command completion coalescing control
  ULONG	                    CmdCompletionPorts;    // 0x0018, CCC_PORTS  Command completion coalescing ports
  ULONG	                    EMLocation;            // 0x001C, EM_LOC     Enclosure management location
  ULONG	                    EMControl;             // 0x0020, EM_CTL     Enclosure management control
  ULONG	                    HostCapabilitiesExt;   // 0x0024, CAP2       Host capabilities extended
  ULONG	                    Handoff;               // 0x0028, BOHC       BIOS/OS handoff control and status
  
  ULONG	                    Reserved[0x0D];        // 0x002C ... 0x005F
  ULONG	                    ReservedNvmhci[0x10];  // 0x0060 ... 0x009F - Reserved for NVMHCI
  ULONG	                    Vendor[0x18];          // 0x00A0 ... 0x00FF - Vendor specific registers

  AHCI_PORT_REGISTERS       PortControl[32];       // 0x0100 ... 0x10FF - Port control registers
  
} AHCI_MEMORY_REGISTERS, *PAHCI_MEMORY_REGISTERS;

typedef struct _AHCI_INTERRUPT_RESOURCE {

  UCHAR        InterruptShareDisposition;
  USHORT       InterruptFlags;
  ULONG        InterruptLevel;
  ULONG        InterruptVector;
  KAFFINITY    InterruptAffinity;
  PKINTERRUPT  InterruptObject;

} AHCI_INTERRUPT_RESOURCE, *PAHCI_INTERRUPT_RESOURCE;

typedef NTSTATUS
(NTAPI *PAHCI_START_IO)(IN PVOID  ChannelPdoExtension,
                        IN PIDENTIFY_DATA Identify,
                        IN PSCSI_REQUEST_BLOCK  Srb);

typedef NTSTATUS
(NTAPI *PAHCI_INTERRUPT)(IN PVOID  ChannelPdoExtension,
                         IN PIDENTIFY_DATA IdentifyData,
                         IN PSCSI_REQUEST_BLOCK  Srb);

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
  PAHCI_MEMORY_REGISTERS    Abar;

  // port interface
  ULONG                     Channel;                    // логический канал (порядковый номер)
  ULONG                     AhciChannel;                // физический канал (Ports Implemented) (0 ... 31)


  PAHCI_START_IO            AhciStartIo;
  PAHCI_INTERRUPT           AhciInterrupt;

} AHCI_INTERFACE, *PAHCI_INTERFACE;

#endif /* _AHCI_PCH_ */
