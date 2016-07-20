#ifndef _AHCI_PCH_
#define _AHCI_PCH_

#include <ntddk.h>
#include <ide.h>
#include <srb.h>


typedef union _AHCI_HOST_CAPABILITIES  {

  struct {
    ULONG NumberPorts                    :5; // NP:    0Тs based value indicating the maximum number of ports supported by the HBA silicon. A maximum of 32 ports can be supported. A value of С0hТ, indicating one port, is the minimum requirement. Note that the number of ports indicated in this field may be more than the number of ports indicated in the PI register.
    ULONG SupportsExternalSATA           :1; // SXS:   When set to С1Т, indicates that the HBA has one or more Serial ATA ports that has a signal only connector that is externally accessible (e.g. eSATA connector). If this bit is set to С1Т, software may refer to the PxCMD.ESP bit to determine whether a specific port has its signal connector externally accessible as a signal only connector (i.e. power is not part of that connector). When the bit is cleared to С0Т, indicates that the HBA has no Serial ATA ports that have a signal only connector externally accessible.
    ULONG EnclosureManagement            :1; // EMS:   When set to С1Т, indicates that the HBA supports enclosure management as defined in section  12. When enclosure management is supported, the HBA has implemented the EM_LOC and EM_CTL global HBA registers. When cleared to С0Т, indicates that the HBA does not support enclosure management and the EM_LOC and EM_CTL global HBA registers are not implemented.
    ULONG CmdCompletionCoalescing        :1; // CCCS:  When set to С1Т, indicates that the HBA supports command completion coalescing as defined in section  11. When command completion coalescing is supported, the HBA has implemented the CCC_CTL and the CCC_PORTS global HBA registers. When cleared to С0Т, indicates that the HBA does not support command completion coalescing and the CCC_CTL and CCC_PORTS global HBA registers are not implemented. 
    ULONG NumberCommandSlots             :5; // NCS:   0Тs based value indicating the number of command slots per port supported by this HBA. A minimum of 1 and maximum of 32 slots per port can be supported. The same number of command slots is available on each implemented port.
    ULONG PartialStateCapable            :1; // PSC:   Indicates whether the HBA can support transitions to the Partial state. When cleared to С0Т, software must not allow the HBA to initiate transitions to the Partial state via agressive link power management nor the PxCMD.ICC field in each port, and the PxSCTL.IPM field in each port must be programmed to disallow device initiated Partial requests. When set to С1Т, HBA and device initiated Partial requests can be supported.
    ULONG SlumberStateCapable            :1; // SSC:   Indicates whether the HBA can support transitions to the Slumber state. When cleared to С0Т, software must not allow the HBA to initiate transitions to the Slumber state via agressive link power management nor the PxCMD.ICC field in each port, and the PxSCTL.IPM field in each port must be programmed to disallow device initiated Slumber requests. When set to С1Т, HBA and device initiated Slumber requests can be supported.
    ULONG PioMultipleDrqBlock            :1; // PMD:   If set to С1Т, the HBA supports multiple DRQ block data transfers for the PIO command protocol. If cleared to С0Т the HBA only supports single DRQ block data transfers for the PIO command protocol. AHCI 1.2 HBAs shall have this bit set to С1Т.
    ULONG FISbasedSwitching              :1; // FBSS:  When set to С1Т, indicates that the HBA supports Port Multiplier FIS-based switching. When cleared to С0Т, indicates that the HBA does not support FIS-based switching. This bit shall only be set to С1Т if the SPM bit is set to С1Т.
    ULONG PortMultiplier                 :1; // SPM:   Indicates whether the HBA can support a Port Multiplier. When set, a Port Multiplier using command-based switching is supported and FIS-based switching may be supported. When cleared to С0Т, a Port Multiplier is not supported, and a Port Multiplier may not be attached to this HBA.
    ULONG AhciModeOnly                   :1; // SAM:   The SATA controller may optionally support AHC access mechanisms only. A value of '0' indicates that in addition to the native AHC mechanism (via ABAR), the SATA controller implements a legacy, task-file based register interface such as SFF-8038i. A value of '1' indicates that the SATA controller does not implement a legacy, task-file based register interface.
    ULONG Reserved                       :1; // SNZO
    ULONG InterfaceSpeed                 :4; // ISS:   Indicates the maximum speed the HBA can support on its ports. These encodings match the system software programmable PxSCTL.DET.SPD field.
    ULONG CommandListOverride            :1; // SCLO:  When set to С1Т, the HBA supports the PxCMD.CLO bit and its associated function. When cleared to С0Т, the HBA is not capable of clearing the BSY and DRQ bits in the Status register in order to issue a software reset if these bits are still set from a previous operation.
    ULONG ActivityLED                    :1; // SAL:   When set to С1Т, the HBA supports a single activity indication output pin. This pin can be connected to an LED on the platform to indicate device activity on any drive. When cleared to С0Т, this function is not supported. 
    ULONG AggressiveLinkPowerManagement  :1; // SALP:  When set to С1Т, the HBA can support auto-generating link requests to the Partial or Slumber states when there are no commands to process. When cleared to С0Т, this function is not supported and software shall treat the PxCMD.ALPE and PxCMD.ASP bits as reserved.
    ULONG StaggeredSpinUp                :1; // SSS:   When set to С1Т, the HBA supports staggered spin-up on its ports, for use in balancing power spikes. When cleared to С0Т, this function is not supported. This value is loaded by the BIOS prior to OS initiallization.
    ULONG MechanicalPresenceSwitch       :1; // SMPS:  When set to С1Т, the HBA supports mechanical presence switches on its ports for use in hot plug operations. When cleared to С0Т, this function is not supported. This value is loaded by the BIOS prior to OS initialization.
    ULONG SNotificationRegister          :1; // SSNTF: When set to С1Т, the HBA supports the PxSNTF (SNotification) register and its associated functionality. When cleared to С0Т, the HBA does not support the PxSNTF (SNotification) register and its associated functionality. Refer to section 10.11.1. Asynchronous notification with a directly attached device is always supported.
    ULONG NativeCommandQueuing           :1; // SNCQ:  Indicates whether the HBA supports Serial ATA native command queuing. If set to С1Т, an HBA shall handle DMA Setup FISes natively, and shall handle the auto-activate optimization through that FIS. If cleared to С0Т, native command queuing is not supported and software should not issue any native command queuing commands.
    ULONG Addressing64bit                :1; // S64A:  Indicates whether the HBA can access 64-bit data structures. When set to С1Т, the HBA shall make the 32-bit upper bits of the port DMA Descriptor, the PRD Base, and each PRD entry read/write. When cleared to С0Т, these are read-only and treated as С0Т by the HBA. 
  };

  ULONG AsULONG;

} AHCI_HOST_CAPABILITIES, *PAHCI_HOST_CAPABILITIES;

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
  ULONG                     Channel;                    // логический канал (пор€дковый номер)
  ULONG                     AhciChannel;                // физический канал (Ports Implemented) (0 ... 31)


  PAHCI_START_IO            AhciStartIo;
  PAHCI_INTERRUPT           AhciInterrupt;

} AHCI_INTERFACE, *PAHCI_INTERFACE;

#endif /* _AHCI_PCH_ */
