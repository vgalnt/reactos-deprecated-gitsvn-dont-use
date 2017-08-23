#ifndef USBEHCI_H__
#define USBEHCI_H__

#include <ntddk.h>
#include <windef.h>
#include <stdio.h>
#include <hubbusif.h>
#include <usbbusif.h>
#include <usbdlib.h>
#include <drivers/usbport/usbmport.h>

#include "hardware.h"

extern USBPORT_REGISTRATION_PACKET RegPacket;

#define EHCI_HCD_TD_FLAG_ALLOCATED 0x01
#define EHCI_HCD_TD_FLAG_PROCESSED 0x02
#define EHCI_HCD_TD_FLAG_DONE      0x08
#define EHCI_HCD_TD_FLAG_ACTIVE    0x10
#define EHCI_HCD_TD_FLAG_DUMMY     0x20

struct _EHCI_HCD_QH;
struct _EHCI_ENDPOINT;
struct _EHCI_TRANSFER;

typedef struct _EHCI_HCD_TD {
  //Hardware
  EHCI_QUEUE_TD HwTD;
  //Software
  struct _EHCI_HCD_TD * PhysicalAddress;
  ULONG TdFlags;
  struct _EHCI_ENDPOINT * EhciEndpoint;
  struct _EHCI_TRANSFER * EhciTransfer;
  struct _EHCI_HCD_TD * NextHcdTD;
  struct _EHCI_HCD_TD * AltNextHcdTD;
  USB_DEFAULT_PIPE_SETUP_PACKET SetupPacket;
  ULONG LengthThisTD;
  LIST_ENTRY DoneLink;
  ULONG Pad[40];
} EHCI_HCD_TD, *PEHCI_HCD_TD;

C_ASSERT(sizeof(EHCI_HCD_TD) == 0x100);

typedef struct _EHCI_STATIC_QH {
  //Hardware
  EHCI_QUEUE_HEAD HwQH;
  //Software
  ULONG QhFlags;
  struct _EHCI_HCD_QH * PhysicalAddress;
  struct _EHCI_HCD_QH * PrevHead;
  struct _EHCI_HCD_QH * NextHead;
  struct _EHCI_STATIC_QH * StaticQH;
  ULONG Pad[18];
} EHCI_STATIC_QH, *PEHCI_STATIC_QH;

C_ASSERT(sizeof(EHCI_STATIC_QH) == 0xA0);

typedef struct _EHCI_HCD_QH {
  struct _EHCI_STATIC_QH sqh;
  ULONG Pad[24];
} EHCI_HCD_QH, *PEHCI_HCD_QH;

C_ASSERT(sizeof(EHCI_HCD_QH) == 0x100);

typedef struct _EHCI_ENDPOINT {
  ULONG Reserved;
  ULONG EndpointStatus;
  ULONG EndpointState;
  USBPORT_ENDPOINT_PROPERTIES EndpointProperties;
  PEHCI_HCD_TD DummyTdVA; // DmaBufferVA
  PEHCI_HCD_TD DummyTdPA; // DmaBufferPA
  PEHCI_HCD_TD FirstTD;
  ULONG MaxTDs;
  ULONG PendingTDs;
  ULONG RemainTDs;
  PEHCI_HCD_QH QH;
  PEHCI_HCD_TD HcdHeadP;
  PEHCI_HCD_TD HcdTailP;
  LIST_ENTRY ListTDs;
} EHCI_ENDPOINT, *PEHCI_ENDPOINT;

typedef struct _EHCI_TRANSFER {
  ULONG Reserved;
  PUSBPORT_TRANSFER_PARAMETERS TransferParameters;
  ULONG USBDStatus;
  ULONG TransferLen;
  PEHCI_ENDPOINT EhciEndpoint;
  ULONG PendingTDs;
  ULONG TransferOnAsyncList;
} EHCI_TRANSFER, *PEHCI_TRANSFER;

typedef struct _EHCI_HC_RESOURCES {
  PEHCI_STATIC_QH PeriodicFrameList[1024]; // 4K-page aligned array
  EHCI_STATIC_QH AsyncHead;
  EHCI_STATIC_QH PeriodicHead[64];
  UCHAR Padded[0x160];
  EHCI_HCD_QH DummyQH[1024];
} EHCI_HC_RESOURCES, *PEHCI_HC_RESOURCES;

#define EHCI_FLAGS_CONTROLLER_SUSPEND 0x01
#define EHCI_FLAGS_IDLE_SUPPORT       0x20

typedef struct _EHCI_EXTENSION {
  ULONG Reserved;
  ULONG Flags;
  PULONG BaseIoAdress;
  PEHCI_HW_REGISTERS OperationalRegs;
  UCHAR FrameLengthAdjustment;
  BOOLEAN IsStarted;
  USHORT HcSystemErrors;
  ULONG PortRoutingControl;
  USHORT NumberOfPorts; // HCSPARAMS => N_PORTS 
  USHORT PortPowerControl; // HCSPARAMS => Port Power Control (PPC)
  EHCI_INTERRUPT_ENABLE InterruptMask;
  EHCI_INTERRUPT_ENABLE InterruptStatus;
  /* Shedule */
  PEHCI_HC_RESOURCES HcResourcesVA;
  PEHCI_HC_RESOURCES HcResourcesPA;
  PEHCI_STATIC_QH AsyncHead;
  PEHCI_STATIC_QH PeriodicHead[64];
  ULONG_PTR DummyQHListVA;
  ULONG_PTR DummyQHListPA;
  ULONG FrameIndex;
  ULONG FrameHighPart;
  /* Root Hub Bits */
  ULONG ConnectPortBits;
  ULONG SuspendPortBits;
  ULONG ResetPortBits;
  ULONG FinishResetPortBits;
  /* Transfers */
  ULONG PendingTransfers;
  /* Lock Queue */
  PEHCI_HCD_QH PrevQH;
  PEHCI_HCD_QH LockQH;
  PEHCI_HCD_QH NextQH;
  /* Registers Copy Bakup */
  ULONG BakupPeriodiclistbase; // PERIODICLISTBASE
  ULONG BakupAsynclistaddr; // ASYNCLISTADDR
  ULONG BakupCtrlDSSegment; // CTRLDSSEGMENT
  ULONG BakupUSBCmd; // USBCMD
} EHCI_EXTENSION, *PEHCI_EXTENSION;

/* debug.c */
VOID
NTAPI
EHCI_DumpHwTD(
  IN PEHCI_HCD_TD TD);

VOID
NTAPI
EHCI_DumpHwQH(
  IN PEHCI_HCD_QH QH);

/* roothub.c */
VOID
NTAPI
EHCI_RH_GetRootHubData(
  IN PVOID ohciExtension,
  IN PVOID rootHubData);

MPSTATUS
NTAPI
EHCI_RH_GetStatus(
  IN PVOID ohciExtension,
  IN PUSHORT Status);

MPSTATUS
NTAPI
EHCI_RH_GetPortStatus(
  IN PVOID ohciExtension,
  IN USHORT Port,
  IN PUSB_PORT_STATUS_AND_CHANGE PortStatus);

MPSTATUS
NTAPI
EHCI_RH_GetHubStatus(
  IN PVOID ohciExtension,
  IN PUSB_HUB_STATUS_AND_CHANGE HubStatus);

MPSTATUS
NTAPI
EHCI_RH_SetFeaturePortReset(
  IN PVOID ohciExtension,
  IN USHORT Port);

MPSTATUS
NTAPI
EHCI_RH_SetFeaturePortPower(
  IN PVOID ohciExtension,
  IN USHORT Port);

MPSTATUS
NTAPI
EHCI_RH_SetFeaturePortEnable(
  IN PVOID ohciExtension,
  IN USHORT Port);

MPSTATUS
NTAPI
EHCI_RH_SetFeaturePortSuspend(
  IN PVOID ohciExtension,
  IN USHORT Port);

MPSTATUS
NTAPI
EHCI_RH_ClearFeaturePortEnable(
  IN PVOID ohciExtension,
  IN USHORT Port);

MPSTATUS
NTAPI
EHCI_RH_ClearFeaturePortPower(
  IN PVOID ohciExtension,
  IN USHORT Port);

MPSTATUS
NTAPI
EHCI_RH_ClearFeaturePortSuspend(
  IN PVOID ohciExtension,
  IN USHORT Port);

MPSTATUS
NTAPI
EHCI_RH_ClearFeaturePortEnableChange(
  IN PVOID ohciExtension,
  IN USHORT Port);

MPSTATUS
NTAPI
EHCI_RH_ClearFeaturePortConnectChange(
  IN PVOID ohciExtension,
  IN USHORT Port);

MPSTATUS
NTAPI
EHCI_RH_ClearFeaturePortResetChange(
  IN PVOID ohciExtension,
  IN USHORT Port);

MPSTATUS
NTAPI
EHCI_RH_ClearFeaturePortSuspendChange(
  IN PVOID ohciExtension,
  IN USHORT Port);

MPSTATUS
NTAPI
EHCI_RH_ClearFeaturePortOvercurrentChange(
  IN PVOID ohciExtension,
  IN USHORT Port);

VOID
NTAPI
EHCI_RH_DisableIrq(
  IN PVOID ohciExtension);

VOID
NTAPI
EHCI_RH_EnableIrq(
  IN PVOID ohciExtension);

#endif /* USBEHCI_H__ */
