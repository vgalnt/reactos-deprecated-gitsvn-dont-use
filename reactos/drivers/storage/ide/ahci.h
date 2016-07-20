#ifndef _AHCI_PCH_
#define _AHCI_PCH_

#include <ntddk.h>
#include <ide.h>
#include <srb.h>


typedef enum {

  FIS_TYPE_REGISTER_H2D    = 0x27,  // Register FIS - Host to Device
  FIS_TYPE_REGISTER_D2H    = 0x34,  // Register FIS - Device to Host
  FIS_TYPE_DMA_ACTIVATE    = 0x39,  // DMA activate FIS - Device to Host
  FIS_TYPE_DMA_SETUP       = 0x41,  // DMA setup FIS - Bi-directional
  FIS_TYPE_DATA            = 0x46,  // Data FIS - Bi-directional
  FIS_TYPE_BIST_ACTIVATE   = 0x58,  // BIST activate FIS - Bi-directional
  FIS_TYPE_PIO_SETUP       = 0x5F,  // PIO setup FIS - Device to Host
  FIS_TYPE_SET_DEVICE_BITS = 0xA1,  // Set Device bits FIS - Device to Host

} FIS_TYPE;

typedef struct _FIS_REGISTER_H2D {

  UCHAR  FISType;                   // 0x27  FIS_TYPE_REGISTER_H2D
  UCHAR  PortMultiplierPort :4;     // Port multiplier
  UCHAR  Reserved0          :3;
  UCHAR  RegisterType       :1;     // 1 - Command, 0 - Control
  UCHAR  Command;                   // Contains the contents of the Command register of the Shadow Register Block
  UCHAR  FeaturesLow;               // Feature register          7:0

  UCHAR  LBA0;                      // LBA low register  7:0
  UCHAR  LBA1;                      // LBA mid register  15:8
  UCHAR  LBA2;                      // LBA high register 23:16
  UCHAR  Device;                    // Device register

  UCHAR  LBA3;                      // LBA register      31:24
  UCHAR  LBA4;                      // LBA register      39:32
  UCHAR  LBA5;                      // LBA register      47:40
  UCHAR  FeaturesHigh;              // Feature register          15:8

  UCHAR  CountLow;                  // Count register   7:0
  UCHAR  CountHigh;                 // Count register  15:8
  UCHAR  IsochronousCmdCompletion;  // ICC 
  UCHAR  Control;                   // Control register

  UCHAR  Reserved[4];

} FIS_REGISTER_H2D, *PFIS_REGISTER_H2D;

//-------------------------------------------------

typedef struct _AHCI_PRD {

  ULONG  DataBaseAddress;             // DBA   Indicates the 32-bit physical address of the data block. The block must be word aligned, indicated by bit 0 being reserved.
  ULONG  DataBaseAddressU;            // DBAU  This is the upper 32-bits of the data block physical address. It is only valid if the HBA indicated that it can support 64-bit addressing through the S64A bit in the capabilities register, and is ignored otherwise.
  ULONG  Reserved0;

  struct {
    ULONG  DataByteCount         :22; // DBC  A С0Т based value that Indicates the length, in bytes, of the data block. A maximum of length of 4MB may exist for any entry. Bit С0Т of this field must always be С1Т to indicate an even byte count. A value of С1Т indicates 2 bytes, С3Т indicates 4 bytes, etc.
    ULONG  Reserved1             :9;
    ULONG  InterruptOnCompletion :1;  // I    When set, indicates that hardware should assert an interrupt when the data block for this entry has transferred, which means that no data is in the HBA hardware. Data may still be in flight to system memory (disk reads), or at the device (an R_OK or R_ERR has yet to be received). The HBA shall set the PxIS.DPS bit after completing the data transfer, and if enabled, generate an interrupt.
  };

} AHCI_PRD, *PAHCI_PRD;

typedef struct _AHCI_PRDT {  // 128 byte - aligned (0x0080)
  AHCI_PRD Descriptor[32];  // дл€ 128 кЅ (0x20000) должно хватить?(может +1?) ≈сли добавл€ть ещЄ один AHCI_PRD (а это 16 байт), то надо добивать +112 байт (128 byte - aligned)
} AHCI_PRDT, *PAHCI_PRDT;

typedef struct _AHCI_COMMAND_TABLE { // This address must be aligned to a 128-byte cache line (0x0080)

  UCHAR      CommandFIS[0x40];    // CFIS  This is a software constructed FIS. For data transfer operations, this is the H2D Register FIS format as specified in the Serial ATA Revision 2.5 specification. The HBA sets PxTFD.STS.BSY, and then sends this structure to the attached port. If a Port Multiplier is attached, this field must have the Port Multiplier port number in the FIS itself Ц it shall not be added by the HBA. Valid CFIS lengths are 2 to 16 Dwords and must be in Dword granularity.
  UCHAR      AtapiCommand[0x10];  // ACMD  ATAPI command, 12 or 16 bytes. This is a software constructed region of 12 or 16 bytes in length that contains the ATAPI command to transmit if the УAФ bit is set in the command header. The ATAPI command must be either 12 or 16 bytes in length. The length transmitted by the HBA is determined by the PIO setup FIS that is sent by the device requesting the ATAPI command.
  UCHAR      Reserved[0x30];

  AHCI_PRDT  PRDTable;            // PRDT  Physical Region Descriptor Table.

} AHCI_COMMAND_TABLE, *PAHCI_COMMAND_TABLE;

typedef union _AHCI_DESCRIPTION_INFORMATION {    
    
  struct {
    ULONG  CommandFISLength   :5;  // CFL    Length of the Command FIS (in DWORDS, 2 ... 16). A С0Т represents 0 DW, С4Т represents 4 DW. A length of С0Т or С1Т is illegal. The maximum value allowed is 10h, or 16 DW. The HBA uses this field to know the length of the FIS it shall send to the device.
    ULONG  ATAPI              :1;  // A      When С1Т, indicates that a PIO setup FIS shall be sent by the device indicating a transfer for the ATAPI command. The HBA may prefetch data from CTBAz[ACMD] in anticipation of receiving the PIO Setup FIS.
    ULONG  Write              :1;  // W      (1: H2D, 0: D2H). When set, indicates that the direction is a device write (data from system memory to device). When cleared, indicates that the direction is a device read (data from device to system memory). If this bit is set and the P bit is set, the HBA may prefetch data in anticipation of receiving a DMA Setup FIS, a DMA Activate FIS, or PIO Setup FIS, in addition to prefetching PRDs.
    ULONG  Prefetchable       :1;  // P      This bit is only valid if the PRDTL field is non-zero or the ATAPI СAТ bit is set in the command header. When set and PRDTL is non-zero, the HBA may prefetch PRDs in anticipation of performing a data transfer. When set and the ATAPI СAТ bit is set in the command header, the HBA may prefetch the ATAPI command. System software shall not set this bit when using native command queuing commands or when using FIS-based switching with a Port Multiplier.
    ULONG  Reset              :1;  // R      When С1Т, indicates that the command that software built is for a part of a software reset sequence that manipulates the SRST bit in the Device Control register. The HBA must perform a SYNC escape (if necessary) to get the device into an idle state before sending the command.
    ULONG  BIST               :1;  // B      When С1Т, indicates that the command that software built is for sending a BIST FIS. The HBA shall send the FIS and enter a test mode. The tests that can be run in this mode are outside the scope of this specification.
    ULONG  ClearBusy          :1;  // C      Clear Busy upon R_OK. When set, the HBA shall clear PxTFD.STS.BSY and PxCI.CI(pIssueSlot) after transmitting this FIS and receiving R_OK. When cleared, the HBA shall not clear PxTFD.STS.BSY nor PxCI.CI(pIssueSlot) after transmitting this FIS and receiving R_OK.
    ULONG  Reserved           :1;        
    ULONG  PortMultiplierPort :4;  // PMP    Indicates the port number that should be used when constructing Data FISes on transmit, and to check against all FISes received for this command. This value shall be set to 0h by software when it has been determined that it is communicating to a directly attached device.
    ULONG  PRDTLength         :16; // PRDTL  Physical Region Descriptor Table Length. Length of the scatter/gather descriptor table in entries, called the Physical Region Descriptor Table. Each entry is 4 DW. A С0Т represents 0 entries, FFFFh represents 65,535 entries. The HBA uses this field to know when to stop fetching PRDs. If this field is С0Т, then no data transfer shall occur with the command.
  };
  
  ULONG  AsULONG;

} AHCI_DESCRIPTION_INFORMATION, *PAHCI_DESCRIPTION_INFORMATION;

typedef struct _AHCI_COMMAND_HEADER {

  AHCI_DESCRIPTION_INFORMATION  DescriptionInformation;   //0 DI 
  ULONG                         PRDTByteCount;            //1 PRDBC  Command Status. Physical Region Descriptor Byte Count: Indicates the current byte count that has transferred on device writes (system memory to device) or device reads (device to system memory).
  PAHCI_COMMAND_TABLE           CmdTableDescriptorBase;   //2 CTBA   Command Table Base Address. Indicates the 32-bit physical address of the command table, which contains the command FIS, ATAPI Command, and PRD table. This address must be aligned to a 128-byte cache line, indicated by bits 06:00 being reserved.
  ULONG                         CmdTableDescriptorBaseU;  //3 CTBAU  Command Table Base Address Upper.  This is the upper 32-bits of the Command Table Base. It is only valid if the HBA indicated that it can support 64-bit addressing through the S64A bit in the capabilities register, and is ignored otherwise.

  ULONG                         Reserved[4];              //4 ... 7

} AHCI_COMMAND_HEADER, *PAHCI_COMMAND_HEADER; 

typedef struct _AHCI_COMMAND_LIST {        // 1024 byte - aligned (0x0400)
  AHCI_COMMAND_HEADER Header[32];
} AHCI_COMMAND_LIST, *PAHCI_COMMAND_LIST; 

typedef struct _AHCI_RECEIVED_FIS {        // 256 byte - aligned  (0x0100)

  FIS_DMA_SETUP        FisDmaSetup;      // DSFIS   When a DMA setup FIS arrives from the device, the HBA copies it to the DSFIS area of this structure
  ULONG                Reserved0;
  FIS_PIO_SETUP        FisPioSetup;      // PSFIS   When a PIO setup FIS arrives from the device, the HBA copies it to the PSFIS area of this structure
  ULONG                Reserved1[3];
  FIS_REGISTER_D2H     FisRegisterD2H;   // RFIS    When a D2H (device to host) Register FIS arrives from the device, the HBA copies it to the RFIS area of this structure
  ULONG                Reserved2;
  FIS_SET_DEVICE_BITS  FisSetDeviceBits; // SDBFIS  When a Set Device Bits FIS arrives from the device, the HBA copies it to the SDBFIS area of this structure
  FIS_UNKNOWN          FisUnknown;       // UFIS    When an unknown FIS arrives from the device, the HBA copies it to the UFIS area in this structure, and sets PxSERR.DIAG.F, which is reflected in PxIS.UFS when the FIS is posted to memory
  ULONG                Reserved3[24];

} AHCI_RECEIVED_FIS, *PAHCI_RECEIVED_FIS;

typedef union _AHCI_SATA_STATUS {

  struct {
    ULONG  DeviceDetection          :4;  // Indicates the interface device detection and Phy state (0,1,3,4)
    ULONG  CurrentInterfaceSpeed    :4;  // Indicates the negotiated interface communication speed (0,1,2,3)
    ULONG  InterfacePowerManagement :4;  // Indicates the current interface state (0,1,2,6)
    ULONG  Reserved                 :20;
  };

  ULONG AsULONG;

} AHCI_SATA_STATUS, *PAHCI_SATA_STATUS;

typedef union _AHCI_PORT_COMMAND {

  struct {
    ULONG  Start                              :1; // ST     When set, the HBA may process the command list. When cleared, the HBA may not process the command list. Whenever this bit is changed from a С0Т to a С1Т, the HBA starts processing the command list at entry С0Т. Whenever this bit is changed from a С1Т to a С0Т, the PxCI register is cleared by the HBA upon the HBA putting the controller into an idle state. This bit shall only be set to С1Т by software after PxCMD.FRE has been set to С1Т. Refer to section 10.3.1 for important restrictions on when ST can be set to С1Т.
    ULONG  SpinUpDevice                       :1; // SUD
    ULONG  PowerOnDevice                      :1; // POD
    ULONG  CmdListOverride                    :1; // CLO    Setting this bit to С1Т causes PxTFD.STS.BSY and PxTFD.STS.DRQ to be cleared to С0Т. This allows a software reset to be transmitted to the device regardless of whether the BSY and DRQ bits are still set in the PxTFD.STS register. The HBA sets this bit to С0Т when PxTFD.STS.BSY and PxTFD.STS.DRQ have been cleared to С0Т. A write to this register with a value of С0Т shall have no effect. This bit shall only be set to С1Т immediately prior to setting the PxCMD.ST bit to С1Т from a previous value of С0Т. Setting this bit to С1Т at any other time is not supported and will result in indeterminate behavior. Software must wait for CLO to be cleared to С0Т before setting PxCMD.ST to С1Т.
    ULONG  FISReceiveEnable                   :1; // FRE    When set, the HBA may post received FISes into the FIS receive area pointed to by PxFB (and for 64-bit HBAs, PxFBU). When cleared, received FISes are not accepted by the HBA, except for the first D2H register FIS after the initialization sequence, and no FISes are posted to the FIS receive area. System software must not set this bit until PxFB (PxFBU) have been programmed with a valid pointer to the FIS receive area, and if software wishes to move the base, this bit must first be cleared, and software must wait for the FR bit in this register to be cleared.
    ULONG  Reserved                           :3;

    ULONG  CurrentCmdSlot                     :5; // CCS    This field is valid when PxCMD.ST is set to С1Т and shall be set to the command slot value of the command that is currently being issued by the HBA. When PxCMD.ST transitions from С1Т to С0Т, this field shall be reset to С0Т. After PxCMD.ST transitions from С0Т to С1Т, the highest priority slot to issue from next is command slot 0. After the first command has been issued, the highest priority slot to issue from next is PxCMD.CCS + 1. For example, after the HBA has issued its first command, if CCS = 0h and PxCI is set to 3h, the next command that will be issued is from command slot 1.
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

    ULONG  DeviceIsAtapi                      :1; // ATAPI  When set to С1Т, the connected device is an ATAPI device. This bit is used by the HBA to control whether or not to generate the desktop LED when commands are active.
    ULONG  AtapiDriveLedEnable                :1; // DLAE
    ULONG  AggressiveLinkPowerManagement      :1; // ALPE
    ULONG  AggressiveSlumberPartial           :1; // ASP
    ULONG  InterfaceCommunicationControl      :4; // ICC
  };

  ULONG AsULONG;

} AHCI_PORT_COMMAND, *PAHCI_PORT_COMMAND;

typedef union _AHCI_PORT_INTERRUPT_STATUS {

  struct {
    ULONG  FisRegisterD2HInterrupt    :1;  // DHRS  A D2H Register FIS has been received with the СIТ bit set, and has been copied into system memory.
    ULONG  FisPioSetupInterrupt       :1;  // PSS   A PIO Setup FIS has been received with the СIТ bit set, it has been copied into system memory, and the data related to that FIS has been transferred. This bit shall be set even if the data transfer resulted in an error.
    ULONG  FisDmaSetupInterrupt       :1;  // DSS   A DMA Setup FIS has been received with the СIТ bit set and has been copied into system memory.
    ULONG  FisSetDeviceBitsInterrupt  :1;  // SDBS  A Set Device Bits FIS has been received with the СIТ bit set and has been copied into system memory.
    ULONG  FisUnknownInterrupt        :1;  // UFS   When set to С1Т, indicates that an unknown FIS was received and has been copied into system memory. This bit is cleared to С0Т by software clearing the PxSERR.DIAG.F bit to С0Т. Note that this bit does not directly reflect the PxSERR.DIAG.F bit. PxSERR.DIAG.F is set immediately when an unknown FIS is detected, whereas this bit is set when that FIS is posted to memory. Software should wait to act on an unknown FIS until this bit is set to С1Т or the two bits may become out of sync.
    ULONG  DescriptorProcessed        :1;  // DPS   A PRD with the СIТ bit set has transferred all of its data.
    ULONG  PortConnectChange          :1;  // PCS   1=Change in Current Connect Status. 0=No change in Current Connect Status. This bit reflects the state of PxSERR.DIAG.X. This bit is only cleared when PxSERR.DIAG.X is cleared.
    ULONG  DeviceMechanicalPresence   :1;  // DMPS  When set, indicates that a mechanical presence switch associated with this port has been opened or closed, which may lead to a change in the connection state of the device. This bit is only valid if both CAP.SMPS and PxCMD.MPSP are set to С1Т.

    ULONG  Reserved0                  :14;
    ULONG  PhyRdyChange               :1;  // PRCS  When set to С1Т indicates the internal PhyRdy signal changed state. This bit reflects the state of PxSERR.DIAG.N. To clear this bit, software must clear PxSERR.DIAG.N to С0Т.
    ULONG  IncorrectPortMultiplier    :1;  // IPMS  Indicates that the HBA received a FIS from a device that did not have a command outstanding. The IPMS bit may be set during enumeration of devices on a Port Multiplier due to the normal Port Multiplier enumeration process. It is recommended that IPMS only be used after enumeration is complete on the Port Multiplier. IPMS is not set when an asynchronous notification is received (a Set Device Bits FIS with the Notification СNТ bit set to С1Т).

    ULONG  Overflow                   :1;  // OFS   Indicates that the HBA received more bytes from a device than was specified in the PRD table for the command. 
    ULONG  Reserved1                  :1;
    ULONG  InterfaceNonFatalError     :1;  // INFS  Indicates that the HBA encountered an error on the Serial ATA interface but was able to continue operation.
    ULONG  InterfaceFatalError        :1;  // IFS   Indicates that the HBA encountered an error on the Serial ATA interface which caused the transfer to stop.
    ULONG  HostBusDataError           :1;  // HBDS  Indicates that the HBA encountered a data error (uncorrectable ECC / parity) when reading from or writing to system memory.
    ULONG  HostBusFatalError          :1;  // HBFS  Indicates that the HBA encountered a host bus error that it cannot recover from, such as a bad software pointer. In PCI, such an indication would be a target or master abort.
    ULONG  TaskFileError              :1;  // TFES  This bit is set whenever the status register is updated by the device and the error bit (bit 0 of the Status field in the received FIS) is set.
    ULONG  ColdPortDetect             :1;  // CPDS  When set, a device status has changed as detected by the cold presence detect logic. This bit can either be set due to a non-connected port receiving a device, or a connected port having its device removed. This bit is only valid if the port supports cold presence detect as indicated by PxCMD.CPD set to С1Т.
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
