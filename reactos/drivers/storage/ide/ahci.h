#ifndef _AHCI_PCH_
#define _AHCI_PCH_

#include <ntddk.h>
#include <ide.h>
#include <srb.h>


typedef union _AHCI_PORT_COMMAND {

  struct {
    ULONG  Start                              :1; // ST     When set, the HBA may process the command list. When cleared, the HBA may not process the command list. Whenever this bit is changed from a 0 to a 1, the HBA starts processing the command list at entry 0. Whenever this bit is changed from a 1 to a 0, the PxCI register is cleared by the HBA upon the HBA putting the controller into an idle state. This bit shall only be set to 1 by software after PxCMD.FRE has been set to 1. Refer to section 10.3.1 for important restrictions on when ST can be set to 1.
    ULONG  SpinUpDevice                       :1; // SUD
    ULONG  PowerOnDevice                      :1; // POD
    ULONG  CmdListOverride                    :1; // CLO    Setting this bit to 1 causes PxTFD.STS.BSY and PxTFD.STS.DRQ to be cleared to 0. This allows a software reset to be transmitted to the device regardless of whether the BSY and DRQ bits are still set in the PxTFD.STS register. The HBA sets this bit to 0 when PxTFD.STS.BSY and PxTFD.STS.DRQ have been cleared to 0. A write to this register with a value of 0 shall have no effect. This bit shall only be set to 1 immediately prior to setting the PxCMD.ST bit to 1 from a previous value of 0. Setting this bit to 1 at any other time is not supported and will result in indeterminate behavior. Software must wait for CLO to be cleared to 0 before setting PxCMD.ST to 1.
    ULONG  FISReceiveEnable                   :1; // FRE    When set, the HBA may post received FISes into the FIS receive area pointed to by PxFB (and for 64-bit HBAs, PxFBU). When cleared, received FISes are not accepted by the HBA, except for the first D2H register FIS after the initialization sequence, and no FISes are posted to the FIS receive area. System software must not set this bit until PxFB (PxFBU) have been programmed with a valid pointer to the FIS receive area, and if software wishes to move the base, this bit must first be cleared, and software must wait for the FR bit in this register to be cleared.
    ULONG  Reserved                           :3;

    ULONG  CurrentCmdSlot                     :5; // CCS    This field is valid when PxCMD.ST is set to 1 and shall be set to the command slot value of the command that is currently being issued by the HBA. When PxCMD.ST transitions from 1 to 0, this field shall be reset to 0. After PxCMD.ST transitions from 0 to 1, the highest priority slot to issue from next is command slot 0. After the first command has been issued, the highest priority slot to issue from next is PxCMD.CCS + 1. For example, after the HBA has issued its first command, if CCS = 0h and PxCI is set to 3h, the next command that will be issued is from command slot 1.
    ULONG  MechanicalPresenceSwitchState      :1; // MPSS
    ULONG  FISReceiveRunning                  :1; // FR     When set, the FIS Receive DMA engine for the port is running.
    ULONG  CmdListRunning                     :1; // CR     When this bit is set, the command list DMA engine for the port is running. 

    ULONG  ColdPresenceState                  :1; // CPS
    ULONG  PortMultiplierAttached             :1; // PMA
    ULONG  HotPlugCapablePort                 :1; // HPCP
    ULONG  MechanicalPresenceSwitchAttached   :1; // MPSP
    ULONG  ColdPresenceDetection              :1; // CPD
    ULONG  ExternalSATAPort                   :1; // ESP
    ULONG  FISSwitchingCapablePort            :1; // FBSCP
    ULONG  AutomaticPartialSlumberTransitions :1; // APSTE

    ULONG  DeviceIsAtapi                      :1; // ATAPI  When set to 1, the connected device is an ATAPI device. This bit is used by the HBA to control whether or not to generate the desktop LED when commands are active.
    ULONG  AtapiDriveLedEnable                :1; // DLAE
    ULONG  AggressiveLinkPowerManagement      :1; // ALPE
    ULONG  AggressiveSlumberPartial           :1; // ASP
    ULONG  InterfaceCommunicationControl      :4; // ICC
  };

  ULONG AsULONG;

} AHCI_PORT_COMMAND, *PAHCI_PORT_COMMAND;

typedef union _AHCI_PORT_INTERRUPT_STATUS {

  struct {
    ULONG  FisRegisterD2HInterrupt    :1;  // DHRS  A D2H Register FIS has been received with the I bit set, and has been copied into system memory.
    ULONG  FisPioSetupInterrupt       :1;  // PSS   A PIO Setup FIS has been received with the I bit set, it has been copied into system memory, and the data related to that FIS has been transferred. This bit shall be set even if the data transfer resulted in an error.
    ULONG  FisDmaSetupInterrupt       :1;  // DSS   A DMA Setup FIS has been received with the I bit set and has been copied into system memory.
    ULONG  FisSetDeviceBitsInterrupt  :1;  // SDBS  A Set Device Bits FIS has been received with the I bit set and has been copied into system memory.
    ULONG  FisUnknownInterrupt        :1;  // UFS   When set to 1, indicates that an unknown FIS was received and has been copied into system memory. This bit is cleared to 0 by software clearing the PxSERR.DIAG.F bit to 0. Note that this bit does not directly reflect the PxSERR.DIAG.F bit. PxSERR.DIAG.F is set immediately when an unknown FIS is detected, whereas this bit is set when that FIS is posted to memory. Software should wait to act on an unknown FIS until this bit is set to 1 or the two bits may become out of sync.
    ULONG  DescriptorProcessed        :1;  // DPS   A PRD with the I bit set has transferred all of its data.
    ULONG  PortConnectChange          :1;  // PCS   1=Change in Current Connect Status. 0=No change in Current Connect Status. This bit reflects the state of PxSERR.DIAG.X. This bit is only cleared when PxSERR.DIAG.X is cleared.
    ULONG  DeviceMechanicalPresence   :1;  // DMPS  When set, indicates that a mechanical presence switch associated with this port has been opened or closed, which may lead to a change in the connection state of the device. This bit is only valid if both CAP.SMPS and PxCMD.MPSP are set to 1.

    ULONG  Reserved0                  :14;
    ULONG  PhyRdyChange               :1;  // PRCS  When set to 1 indicates the internal PhyRdy signal changed state. This bit reflects the state of PxSERR.DIAG.N. To clear this bit, software must clear PxSERR.DIAG.N to 0.
    ULONG  IncorrectPortMultiplier    :1;  // IPMS  Indicates that the HBA received a FIS from a device that did not have a command outstanding. The IPMS bit may be set during enumeration of devices on a Port Multiplier due to the normal Port Multiplier enumeration process. It is recommended that IPMS only be used after enumeration is complete on the Port Multiplier. IPMS is not set when an asynchronous notification is received (a Set Device Bits FIS with the Notification N bit set to 1).

    ULONG  Overflow                   :1;  // OFS   Indicates that the HBA received more bytes from a device than was specified in the PRD table for the command. 
    ULONG  Reserved1                  :1;
    ULONG  InterfaceNonFatalError     :1;  // INFS  Indicates that the HBA encountered an error on the Serial ATA interface but was able to continue operation.
    ULONG  InterfaceFatalError        :1;  // IFS   Indicates that the HBA encountered an error on the Serial ATA interface which caused the transfer to stop.
    ULONG  HostBusDataError           :1;  // HBDS  Indicates that the HBA encountered a data error (uncorrectable ECC / parity) when reading from or writing to system memory.
    ULONG  HostBusFatalError          :1;  // HBFS  Indicates that the HBA encountered a host bus error that it cannot recover from, such as a bad software pointer. In PCI, such an indication would be a target or master abort.
    ULONG  TaskFileError              :1;  // TFES  This bit is set whenever the status register is updated by the device and the error bit (bit 0 of the Status field in the received FIS) is set.
    ULONG  ColdPortDetect             :1;  // CPDS  When set, a device status has changed as detected by the cold presence detect logic. This bit can either be set due to a non-connected port receiving a device, or a connected port having its device removed. This bit is only valid if the port supports cold presence detect as indicated by PxCMD.CPD set to 1.
  };

  ULONG AsULONG;

} AHCI_PORT_INTERRUPT_STATUS, *PAHCI_PORT_INTERRUPT_STATUS;

typedef struct _AHCI_PORT_REGISTERS {

  PAHCI_COMMAND_LIST          CmdListBaseAddress;       // 0x00, PxCLB   Command List Base Address, 1024 byte - aligned
  ULONG                       CmdListBaseAddressUpper;  // 0x04, PxCLBU  Command List Base Address, upper 32 bits
  PAHCI_RECEIVED_FIS          FISBaseAddress;           // 0x08, PxFB    256 byte - aligned
  ULONG                       FISBaseAddressUpper;      // 0x0C  PxFBU   Upper 32 bits
  AHCI_PORT_INTERRUPT_STATUS  InterruptStatus;          // 0x10  PxIS
  ULONG                       InterruptEnable;          // 0x14  PxIE
  AHCI_PORT_COMMAND           Command;                  // 0x18, PxCMD   Command and Status
  ULONG                       Reserved1;                // 0x1C
  ULONG                       TaskFileData;             // 0x20  PxTFD
  ULONG                       Signature;                // 0x24  PxSIG
  AHCI_SATA_STATUS            SataStatus;               // 0x28, PxSSTS  SATA Status       (SCR0:SStatus)
  ULONG                       SataControl;              // 0x2C, PxSCTL  SATA Control      (SCR2:SControl)
  ULONG                       SataError;                // 0x30, PxSERR  SATA Error        (SCR1:SError)
  ULONG                       SataActive;               // 0x34, PxSACT  SATA Active       (SCR3:SActive)
  ULONG                       CommandIssue;             // 0x38  PxCI
  ULONG                       SataNotification;         // 0x3C, PxSNTF  SATA Notification (SCR4:SNotification)
  ULONG                       FISSwitchingControl;      // 0x40, PxFBS   FIS-based Switch Control
  ULONG                       Reserved2[11];            // 0x44 ... 0x6F
  ULONG                       VendorSpecific[4];        // 0x70 ... 0x7F

}  AHCI_PORT_REGISTERS, *PAHCI_PORT_REGISTERS;

typedef union _AHCI_HOST_GLOBAL_CONTROL {

  struct {
    ULONG Reset               :1;  // HR    HBA Reset: When set by SW, this bit causes an internal reset of the HBA.
    ULONG InterruptEnable     :1;  // IE    This global bit enables interrupts from the HBA. When cleared (reset default), all interrupt sources from all ports are disabled. When set, interrupts are enabled.
    ULONG RevertSingleMessage :1;  // MRSM  MSI Revert to Single Message (MRSM)
    ULONG Reserved            :28;
    ULONG AhciEnable          :1;  // AE    When set, indicates that communication to the HBA shall be via AHCI mechanisms.
  };

  ULONG AsULONG;

} AHCI_HOST_GLOBAL_CONTROL, *PAHCI_HOST_GLOBAL_CONTROL;

typedef union _AHCI_HOST_CAPABILITIES  {

  struct {
    ULONG NumberPorts                    :5; // NP:    0s based value indicating the maximum number of ports supported by the HBA silicon. A maximum of 32 ports can be supported. A value of 0h, indicating one port, is the minimum requirement. Note that the number of ports indicated in this field may be more than the number of ports indicated in the PI register.
    ULONG SupportsExternalSATA           :1; // SXS:   When set to 1, indicates that the HBA has one or more Serial ATA ports that has a signal only connector that is externally accessible (e.g. eSATA connector). If this bit is set to 1, software may refer to the PxCMD.ESP bit to determine whether a specific port has its signal connector externally accessible as a signal only connector (i.e. power is not part of that connector). When the bit is cleared to 0, indicates that the HBA has no Serial ATA ports that have a signal only connector externally accessible.
    ULONG EnclosureManagement            :1; // EMS:   When set to 1, indicates that the HBA supports enclosure management as defined in section  12. When enclosure management is supported, the HBA has implemented the EM_LOC and EM_CTL global HBA registers. When cleared to 0, indicates that the HBA does not support enclosure management and the EM_LOC and EM_CTL global HBA registers are not implemented.
    ULONG CmdCompletionCoalescing        :1; // CCCS:  When set to 1, indicates that the HBA supports command completion coalescing as defined in section  11. When command completion coalescing is supported, the HBA has implemented the CCC_CTL and the CCC_PORTS global HBA registers. When cleared to 0, indicates that the HBA does not support command completion coalescing and the CCC_CTL and CCC_PORTS global HBA registers are not implemented. 
    ULONG NumberCommandSlots             :5; // NCS:   0s based value indicating the number of command slots per port supported by this HBA. A minimum of 1 and maximum of 32 slots per port can be supported. The same number of command slots is available on each implemented port.
    ULONG PartialStateCapable            :1; // PSC:   Indicates whether the HBA can support transitions to the Partial state. When cleared to 0, software must not allow the HBA to initiate transitions to the Partial state via agressive link power management nor the PxCMD.ICC field in each port, and the PxSCTL.IPM field in each port must be programmed to disallow device initiated Partial requests. When set to 1, HBA and device initiated Partial requests can be supported.
    ULONG SlumberStateCapable            :1; // SSC:   Indicates whether the HBA can support transitions to the Slumber state. When cleared to 0, software must not allow the HBA to initiate transitions to the Slumber state via agressive link power management nor the PxCMD.ICC field in each port, and the PxSCTL.IPM field in each port must be programmed to disallow device initiated Slumber requests. When set to 1, HBA and device initiated Slumber requests can be supported.
    ULONG PioMultipleDrqBlock            :1; // PMD:   If set to 1, the HBA supports multiple DRQ block data transfers for the PIO command protocol. If cleared to 0 the HBA only supports single DRQ block data transfers for the PIO command protocol. AHCI 1.2 HBAs shall have this bit set to 1.
    ULONG FISbasedSwitching              :1; // FBSS:  When set to 1, indicates that the HBA supports Port Multiplier FIS-based switching. When cleared to 0, indicates that the HBA does not support FIS-based switching. This bit shall only be set to 1 if the SPM bit is set to 1.
    ULONG PortMultiplier                 :1; // SPM:   Indicates whether the HBA can support a Port Multiplier. When set, a Port Multiplier using command-based switching is supported and FIS-based switching may be supported. When cleared to 0, a Port Multiplier is not supported, and a Port Multiplier may not be attached to this HBA.
    ULONG AhciModeOnly                   :1; // SAM:   The SATA controller may optionally support AHC access mechanisms only. A value of '0' indicates that in addition to the native AHC mechanism (via ABAR), the SATA controller implements a legacy, task-file based register interface such as SFF-8038i. A value of '1' indicates that the SATA controller does not implement a legacy, task-file based register interface.
    ULONG Reserved                       :1; // SNZO
    ULONG InterfaceSpeed                 :4; // ISS:   Indicates the maximum speed the HBA can support on its ports. These encodings match the system software programmable PxSCTL.DET.SPD field.
    ULONG CommandListOverride            :1; // SCLO:  When set to 1, the HBA supports the PxCMD.CLO bit and its associated function. When cleared to 0, the HBA is not capable of clearing the BSY and DRQ bits in the Status register in order to issue a software reset if these bits are still set from a previous operation.
    ULONG ActivityLED                    :1; // SAL:   When set to 1, the HBA supports a single activity indication output pin. This pin can be connected to an LED on the platform to indicate device activity on any drive. When cleared to 0, this function is not supported. 
    ULONG AggressiveLinkPowerManagement  :1; // SALP:  When set to 1, the HBA can support auto-generating link requests to the Partial or Slumber states when there are no commands to process. When cleared to 0, this function is not supported and software shall treat the PxCMD.ALPE and PxCMD.ASP bits as reserved.
    ULONG StaggeredSpinUp                :1; // SSS:   When set to 1, the HBA supports staggered spin-up on its ports, for use in balancing power spikes. When cleared to 0, this function is not supported. This value is loaded by the BIOS prior to OS initiallization.
    ULONG MechanicalPresenceSwitch       :1; // SMPS:  When set to 1, the HBA supports mechanical presence switches on its ports for use in hot plug operations. When cleared to 0, this function is not supported. This value is loaded by the BIOS prior to OS initialization.
    ULONG SNotificationRegister          :1; // SSNTF: When set to 1, the HBA supports the PxSNTF (SNotification) register and its associated functionality. When cleared to 0, the HBA does not support the PxSNTF (SNotification) register and its associated functionality. Refer to section 10.11.1. Asynchronous notification with a directly attached device is always supported.
    ULONG NativeCommandQueuing           :1; // SNCQ:  Indicates whether the HBA supports Serial ATA native command queuing. If set to 1, an HBA shall handle DMA Setup FISes natively, and shall handle the auto-activate optimization through that FIS. If cleared to 0, native command queuing is not supported and software should not issue any native command queuing commands.
    ULONG Addressing64bit                :1; // S64A:  Indicates whether the HBA can access 64-bit data structures. When set to 1, the HBA shall make the 32-bit upper bits of the port DMA Descriptor, the PRD Base, and each PRD entry read/write. When cleared to 0, these are read-only and treated as 0 by the HBA. 
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
  ULONG                     Channel;                    // ėīćč÷åńźčé źąķąė (ļīš’äźīāūé ķīģåš)
  ULONG                     AhciChannel;                // ōčēč÷åńźčé źąķąė (Ports Implemented) (0 ... 31)


  PAHCI_START_IO            AhciStartIo;
  PAHCI_INTERRUPT           AhciInterrupt;

} AHCI_INTERFACE, *PAHCI_INTERFACE;

#endif /* _AHCI_PCH_ */
