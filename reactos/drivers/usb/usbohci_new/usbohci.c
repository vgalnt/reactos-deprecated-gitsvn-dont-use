#include "usbohci.h"

//#define NDEBUG
#include <debug.h>

#define NDEBUG_OHCI_TRACE
#include "dbg_ohci.h"

USBPORT_REGISTRATION_PACKET RegPacket;

MPSTATUS
NTAPI
OHCI_OpenEndpoint(IN PVOID ohciExtension,
                  IN PVOID endpointParameters,
                  IN PVOID ohciEndpoint)
{
    DPRINT("OHCI_OpenEndpoint: UNIMPLEMENTED. FIXME\n");
    return 0;
}

MPSTATUS
NTAPI
OHCI_ReopenEndpoint(IN PVOID ohciExtension,
                    IN PVOID endpointParameters,
                    IN PVOID ohciEndpoint)
{
    DPRINT("OHCI_ReopenEndpoint: UNIMPLEMENTED. FIXME\n");
    return 0;
}

VOID
NTAPI
OHCI_QueryEndpointRequirements(IN PVOID ohciExtension,
                               IN PVOID endpointParameters,
                               IN PULONG EndpointRequirements)
{
    DPRINT("OHCI_QueryEndpointRequirements: UNIMPLEMENTED. FIXME\n");
}

VOID
NTAPI
OHCI_CloseEndpoint(IN PVOID ohciExtension,
                   IN PVOID ohciEndpoint,
                   IN BOOLEAN IsDoDisablePeriodic)
{
    DPRINT("OHCI_CloseEndpoint: Not supported\n");
}

MPSTATUS
NTAPI
OHCI_TakeControlHC(IN POHCI_EXTENSION OhciExtension)
{
    DPRINT1("OHCI_TakeControlHC: UNIMPLEMENTED. FIXME\n");
    return 0;
}

MPSTATUS
NTAPI
OHCI_StartController(IN PVOID ohciExtension,
                     IN PUSBPORT_RESOURCES Resources)
{
    POHCI_EXTENSION OhciExtension;
    POHCI_OPERATIONAL_REGISTERS OperationalRegs;
    OHCI_REG_INTERRUPT_ENABLE_DISABLE Interrupts;
    OHCI_REG_RH_STATUS HcRhStatus;
    OHCI_REG_FRAME_INTERVAL FrameInterval;
    OHCI_REG_CONTROL Control;
    PVOID ScheduleStartVA;
    PVOID ScheduleStartPA;
    UCHAR HeadIndex;
    POHCI_ENDPOINT_DESCRIPTOR StaticED;
    ULONG_PTR SchedulePA;
    POHCI_HCCA OhciHCCA;
    LARGE_INTEGER SystemTime;
    LARGE_INTEGER CurrentTime;
    ULONG ix;
    ULONG jx;
    MPSTATUS MPStatus = 0;

    DPRINT_OHCI("OHCI_StartController: ohciExtension - %p, Resources - %p\n",
                ohciExtension,
                Resources);

    OhciExtension = (POHCI_EXTENSION)ohciExtension;

    /* HC on-chip operational registers */
    OperationalRegs = (POHCI_OPERATIONAL_REGISTERS)Resources->ResourceBase;
    OhciExtension->OperationalRegs = OperationalRegs;

    MPStatus = OHCI_TakeControlHC(OhciExtension);

    if (MPStatus != 0)
    {
        DPRINT1("OHCI_StartController: OHCI_TakeControlHC return MPStatus - %x\n",
                MPStatus);

        return MPStatus;
    }

    OhciExtension->HcResourcesVA = (ULONG_PTR)Resources->StartVA;
    OhciExtension->HcResourcesPA = (ULONG_PTR)Resources->StartPA;

    DPRINT_OHCI("OHCI_StartController: HcResourcesVA - %p, HcResourcesPA - %p\n",
                OhciExtension->HcResourcesVA,
                OhciExtension->HcResourcesPA);

    ScheduleStartVA = (PVOID)((ULONG_PTR)Resources->StartVA + sizeof(OHCI_HCCA));
    ScheduleStartPA = (PVOID)((ULONG_PTR)Resources->StartPA + sizeof(OHCI_HCCA));

    OhciExtension->ScheduleStartVA = ScheduleStartVA;
    OhciExtension->ScheduleStartPA = ScheduleStartPA;

    StaticED = (POHCI_ENDPOINT_DESCRIPTOR)ScheduleStartVA;
    SchedulePA = (ULONG_PTR)ScheduleStartPA;

    ix = 0;

    for (ix = 0; ix < 63; ix++) // FIXME 63 == 32+16+8+4+2+1 (Endpoint Poll Interval (ms))
    {
        if (ix == 0)
        {
            HeadIndex = ED_EOF;
            StaticED->NextED = 0;
        }
        else
        {
            HeadIndex = ((ix - 1) >> 1);
            ASSERT(HeadIndex >= 0 && HeadIndex < 31);
            StaticED->NextED = OhciExtension->IntStaticED[HeadIndex].PhysicalAddress;
        }
  
        StaticED->EndpointControl.sKip = 1;
        StaticED->TailPointer = 0;
        StaticED->HeadPointer = 0;
  
        OhciExtension->IntStaticED[ix].HwED = StaticED;
        OhciExtension->IntStaticED[ix].PhysicalAddress = SchedulePA;
        OhciExtension->IntStaticED[ix].HeadIndex = HeadIndex;
        OhciExtension->IntStaticED[ix].pNextED = &StaticED->NextED;
  
        InitializeListHead(&OhciExtension->IntStaticED[ix].Link);
  
        StaticED += 1;
        SchedulePA += sizeof(OHCI_ENDPOINT_DESCRIPTOR);
    }

    OhciHCCA = (POHCI_HCCA)OhciExtension->HcResourcesVA;
    DPRINT_OHCI("OHCI_InitializeSchedule: OhciHCCA - %p\n", OhciHCCA);

    for (ix = 0, jx = 31; ix < 32; ix++, jx++)
    {
        static UCHAR Balance[32] =
        {0, 16, 8, 24, 4, 20, 12, 28, 2, 18, 10, 26, 6, 22, 14, 30, 
         1, 17, 9, 25, 5, 21, 13, 29, 3, 19, 11, 27, 7, 23, 15, 31};
  
        OhciHCCA->InterrruptTable[Balance[ix]] =
            (POHCI_ENDPOINT_DESCRIPTOR)(OhciExtension->IntStaticED[jx].PhysicalAddress);
  
        OhciExtension->IntStaticED[jx].pNextED =
            (PULONG)&OhciHCCA->InterrruptTable[Balance[ix]];

        OhciExtension->IntStaticED[jx].HccaIndex = Balance[ix];
    }

    DPRINT_OHCI("OHCI_InitializeSchedule: ix - %x\n", ix);

    InitializeListHead(&OhciExtension->ControlStaticED.Link);

    OhciExtension->ControlStaticED.HeadIndex = ED_EOF;
    OhciExtension->ControlStaticED.Type = OHCI_NUMBER_OF_INTERRUPTS + 1;
    OhciExtension->ControlStaticED.pNextED = &OperationalRegs->HcControlHeadED;

    InitializeListHead(&OhciExtension->BulkStaticED.Link);

    OhciExtension->BulkStaticED.HeadIndex = ED_EOF;
    OhciExtension->BulkStaticED.Type = OHCI_NUMBER_OF_INTERRUPTS + 2;
    OhciExtension->BulkStaticED.pNextED = &OperationalRegs->HcBulkHeadED;

    FrameInterval.AsULONG = READ_REGISTER_ULONG(&OperationalRegs->HcFmInterval.AsULONG);

    if ((FrameInterval.FrameInterval) < ((12000 - 1) - 120) ||
        (FrameInterval.FrameInterval) > ((12000 - 1) + 120)) // FIXME 10%
    {
        FrameInterval.FrameInterval = (12000 - 1);
    }

    FrameInterval.FrameIntervalToggle = 1;
    FrameInterval.FSLargestDataPacket = 
        ((FrameInterval.FrameInterval - MAXIMUM_OVERHEAD) * 6) / 7;

    OhciExtension->FrameInterval = FrameInterval;

    DPRINT_OHCI("OHCI_StartController: FrameInterval - %p\n",
                FrameInterval.AsULONG);

    /* reset */
    WRITE_REGISTER_ULONG(&OperationalRegs->HcCommandStatus.AsULONG,
                         1);

    KeStallExecutionProcessor(25);

    Control.AsULONG = READ_REGISTER_ULONG(&OperationalRegs->HcControl.AsULONG);
    Control.HostControllerFunctionalState = OHCI_HC_STATE_RESET;

    WRITE_REGISTER_ULONG(&OperationalRegs->HcControl.AsULONG,
                         Control.AsULONG);

    KeQuerySystemTime(&CurrentTime);
    CurrentTime.QuadPart += 5000000; // 0.5 sec

    while (TRUE)
    {
        WRITE_REGISTER_ULONG(&OperationalRegs->HcFmInterval.AsULONG,
                             OhciExtension->FrameInterval.AsULONG);

        FrameInterval.AsULONG =
            READ_REGISTER_ULONG(&OperationalRegs->HcFmInterval.AsULONG);

        KeQuerySystemTime(&SystemTime);

        if (SystemTime.QuadPart >= CurrentTime.QuadPart)
        {
            MPStatus = 7;
            break;
        }

        if (FrameInterval.AsULONG == OhciExtension->FrameInterval.AsULONG)
        {
            MPStatus = 0;
            break;
        }
    }

    if (MPStatus != 0)
    {
        DPRINT_OHCI("OHCI_StartController: frame interval not set\n");
        return MPStatus;
    }

    WRITE_REGISTER_ULONG(&OperationalRegs->HcPeriodicStart,
                        (OhciExtension->FrameInterval.FrameInterval * 9) / 10); //90%

    WRITE_REGISTER_ULONG(&OperationalRegs->HcHCCA,
                         OhciExtension->HcResourcesPA);

    Interrupts.AsULONG = 0;

    Interrupts.SchedulingOverrun = 1;
    Interrupts.WritebackDoneHead = 1;
    Interrupts.UnrecoverableError = 1;
    Interrupts.FrameNumberOverflow = 1;
    Interrupts.OwnershipChange = 1;

    WRITE_REGISTER_ULONG(&OperationalRegs->HcInterruptEnable.AsULONG,
                         Interrupts.AsULONG);

    Control.AsULONG = READ_REGISTER_ULONG(&OperationalRegs->HcControl.AsULONG);

    Control.ControlBulkServiceRatio = 0; // FIXME (1 : 1)
    Control.PeriodicListEnable = 1;
    Control.IsochronousEnable = 1;
    Control.ControlListEnable = 1;
    Control.BulkListEnable = 1;
    Control.HostControllerFunctionalState = OHCI_HC_STATE_OPERATIONAL;

    WRITE_REGISTER_ULONG(&OperationalRegs->HcControl.AsULONG,
                         Control.AsULONG);
  
    HcRhStatus.AsULONG = 0;
    HcRhStatus.SetGlobalPower = 1;

    WRITE_REGISTER_ULONG(&OperationalRegs->HcRhStatus.AsULONG,
                         HcRhStatus.AsULONG);

    return 0;
}

VOID
NTAPI
OHCI_StopController(IN PVOID ohciExtension,
                    IN BOOLEAN IsDoDisableInterrupts)
{
    DPRINT("OHCI_StopController: UNIMPLEMENTED. FIXME\n");
}

VOID
NTAPI
OHCI_SuspendController(IN PVOID ohciExtension)
{
    DPRINT("OHCI_SuspendController: UNIMPLEMENTED. FIXME\n");
}

MPSTATUS
NTAPI
OHCI_ResumeController(IN PVOID ohciExtension)
{
    DPRINT("OHCI_ResumeController: UNIMPLEMENTED. FIXME\n");
    return 0;
}

BOOLEAN
NTAPI
OHCI_InterruptService(IN PVOID ohciExtension)
{
    DPRINT("OHCI_InterruptService: UNIMPLEMENTED. FIXME\n");
    return 0;
}

VOID
NTAPI
OHCI_InterruptDpc(IN PVOID ohciExtension,
                  IN BOOLEAN IsDoEnableInterrupts)
{
    DPRINT("OHCI_InterruptDpc: UNIMPLEMENTED. FIXME\n");
}

MPSTATUS
NTAPI
OHCI_SubmitTransfer(IN PVOID ohciExtension,
                    IN PVOID ohciEndpoint,
                    IN PVOID transferParameters,
                    IN PVOID ohciTransfer,
                    IN PVOID sgList)
{
    DPRINT("OHCI_SubmitTransfer: UNIMPLEMENTED. FIXME\n");
    return 0;
}

MPSTATUS
NTAPI
OHCI_SubmitIsoTransfer(IN PVOID ohciExtension,
                       IN PVOID ohciEndpoint,
                       IN PVOID transferParameters,
                       IN PVOID ohciTransfer,
                       IN PVOID isoParameters)
{
    DPRINT("OHCI_SubmitIsoTransfer: UNIMPLEMENTED. FIXME\n");
    return 0;
}

VOID
NTAPI
OHCI_AbortTransfer(IN PVOID ohciExtension,
                   IN PVOID ohciEndpoint,
                   IN PVOID ohciTransfer,
                   IN PULONG CompletedLength)
{
    DPRINT("OHCI_AbortTransfer: UNIMPLEMENTED. FIXME\n");
}

ULONG
NTAPI
OHCI_GetEndpointState(IN PVOID ohciExtension,
                      IN PVOID ohciEndpoint)
{
    DPRINT("OHCI_GetEndpointState: UNIMPLEMENTED. FIXME\n");
    return 0;
}

VOID
NTAPI
OHCI_SetEndpointState(IN PVOID ohciExtension,
                      IN PVOID ohciEndpoint,
                      IN ULONG EndpointState)
{
    DPRINT("OHCI_SetEndpointState: UNIMPLEMENTED. FIXME\n");
}

VOID
NTAPI
OHCI_PollEndpoint(IN PVOID ohciExtension,
                  IN PVOID ohciEndpoint)
{
    DPRINT("OHCI_PollEndpoint: UNIMPLEMENTED. FIXME\n");
}

VOID
NTAPI
OHCI_CheckController(IN PVOID ohciExtension)
{
    DPRINT("OHCI_CheckController: UNIMPLEMENTED. FIXME\n");
}

ULONG
NTAPI
OHCI_Get32BitFrameNumber(IN PVOID ohciExtension)
{
    DPRINT("OHCI_Get32BitFrameNumber: UNIMPLEMENTED. FIXME\n");
    return 0;
}

VOID
NTAPI
OHCI_InterruptNextSOF(IN PVOID ohciExtension)
{
    DPRINT("OHCI_InterruptNextSOF: UNIMPLEMENTED. FIXME\n");
}

VOID
NTAPI
OHCI_EnableInterrupts(IN PVOID ohciExtension)
{
    POHCI_EXTENSION OhciExtension;
    POHCI_OPERATIONAL_REGISTERS OperationalRegs;
 
    OhciExtension = (POHCI_EXTENSION)ohciExtension;
    DPRINT_OHCI("OHCI_EnableInterrupts: OhciExtension - %p\n",
                OhciExtension);

    OperationalRegs = OhciExtension->OperationalRegs;

    /* HcInterruptEnable.MIE - Master Interrupt Enable */
    WRITE_REGISTER_ULONG(&OperationalRegs->HcInterruptEnable.AsULONG,
                         0x80000000);
}

VOID
NTAPI
OHCI_DisableInterrupts(IN PVOID ohciExtension)
{
    POHCI_EXTENSION  OhciExtension = (POHCI_EXTENSION)ohciExtension;
    DPRINT_OHCI("OHCI_DisableInterrupts\n");

    /* HcInterruptDisable.MIE - disables interrupt generation */
    WRITE_REGISTER_ULONG(&OhciExtension->OperationalRegs->HcInterruptDisable.AsULONG,
                         0x80000000);
}

VOID
NTAPI
OHCI_PollController(IN PVOID ohciExtension)
{
    DPRINT("OHCI_PollController: UNIMPLEMENTED. FIXME\n");
}

VOID
NTAPI
OHCI_SetEndpointDataToggle(IN PVOID ohciExtension,
                           IN PVOID ohciEndpoint,
                           IN ULONG DataToggle)
{
    DPRINT("OHCI_SetEndpointDataToggle: UNIMPLEMENTED. FIXME\n");
}

ULONG
NTAPI
OHCI_GetEndpointStatus(IN PVOID ohciExtension,
                       IN PVOID ohciEndpoint)
{
    DPRINT("OHCI_GetEndpointStatus: UNIMPLEMENTED. FIXME\n");
    return 0;
}

VOID
NTAPI
OHCI_SetEndpointStatus(IN PVOID ohciExtension,
                       IN PVOID ohciEndpoint,
                       IN ULONG EndpointStatus)
{
    DPRINT("OHCI_SetEndpointStatus: UNIMPLEMENTED. FIXME\n");
}

VOID
NTAPI
OHCI_ResetController(IN PVOID ohciExtension)
{
    DPRINT("OHCI_ResetController: UNIMPLEMENTED. FIXME\n");
}

MPSTATUS
NTAPI
OHCI_StartSendOnePacket(IN PVOID ohciExtension,
                        IN PVOID PacketParameters,
                        IN PVOID Data,
                        IN PULONG pDataLength,
                        IN PVOID BufferVA,
                        IN PVOID BufferPA,
                        IN ULONG BufferLength,
                        IN USBD_STATUS * pUSBDStatus)
{
    DPRINT("OHCI_StartSendOnePacket: UNIMPLEMENTED. FIXME\n");
    return 0;
}

MPSTATUS
NTAPI
OHCI_EndSendOnePacket(IN PVOID ohciExtension,
                      IN PVOID PacketParameters,
                      IN PVOID Data,
                      IN PULONG pDataLength,
                      IN PVOID BufferVA,
                      IN PVOID BufferPA,
                      IN ULONG BufferLength,
                      IN USBD_STATUS * pUSBDStatus)
{
    DPRINT("OHCI_EndSendOnePacket: UNIMPLEMENTED. FIXME\n");
    return 0;
}

MPSTATUS
NTAPI
OHCI_PassThru(IN PVOID ohciExtension,
              IN PVOID passThruParameters,
              IN ULONG ParameterLength,
              IN PVOID pParameters)
{
    DPRINT("OHCI_PassThru: UNIMPLEMENTED. FIXME\n");
    return 0;
}

VOID
NTAPI
OHCI_Unload(IN PVOID ohciExtension)
{
    DPRINT1("OHCI_Unload: Not supported\n");
}

NTSTATUS
NTAPI
DriverEntry(IN PDRIVER_OBJECT DriverObject,
            IN PUNICODE_STRING RegistryPath)
{
    NTSTATUS Status;

    DPRINT_OHCI("DriverEntry: DriverObject - %p, RegistryPath - %wZ\n",
                DriverObject,
                RegistryPath);

    RtlZeroMemory(&RegPacket, sizeof(USBPORT_REGISTRATION_PACKET));

    RegPacket.MiniPortVersion = USB_MINIPORT_VERSION_OHCI;

    RegPacket.MiniPortFlags = USB_MINIPORT_FLAGS_INTERRUPT |
                              USB_MINIPORT_FLAGS_MEMORY_IO |
                              8;

    RegPacket.MiniPortBusBandwidth = 12000;

    RegPacket.MiniPortExtensionSize = sizeof(OHCI_EXTENSION);
    RegPacket.MiniPortEndpointSize = sizeof(OHCI_ENDPOINT);
    RegPacket.MiniPortTransferSize = sizeof(OHCI_TRANSFER);
    RegPacket.MiniPortResourcesSize = sizeof(OHCI_HC_RESOURCES);

    RegPacket.OpenEndpoint = OHCI_OpenEndpoint;
    RegPacket.ReopenEndpoint = OHCI_ReopenEndpoint;
    RegPacket.QueryEndpointRequirements = OHCI_QueryEndpointRequirements;
    RegPacket.CloseEndpoint = OHCI_CloseEndpoint;
    RegPacket.StartController = OHCI_StartController;
    RegPacket.StopController = OHCI_StopController;
    RegPacket.SuspendController = OHCI_SuspendController;
    RegPacket.ResumeController = OHCI_ResumeController;
    RegPacket.InterruptService = OHCI_InterruptService;
    RegPacket.InterruptDpc = OHCI_InterruptDpc;
    RegPacket.SubmitTransfer = OHCI_SubmitTransfer;
    RegPacket.SubmitIsoTransfer = OHCI_SubmitIsoTransfer;
    RegPacket.AbortTransfer = OHCI_AbortTransfer;
    RegPacket.GetEndpointState = OHCI_GetEndpointState;
    RegPacket.SetEndpointState = OHCI_SetEndpointState;
    RegPacket.PollEndpoint = OHCI_PollEndpoint;
    RegPacket.CheckController = OHCI_CheckController;
    RegPacket.Get32BitFrameNumber = OHCI_Get32BitFrameNumber;
    RegPacket.InterruptNextSOF = OHCI_InterruptNextSOF;
    RegPacket.EnableInterrupts = OHCI_EnableInterrupts;
    RegPacket.DisableInterrupts = OHCI_DisableInterrupts;
    RegPacket.PollController = OHCI_PollController;
    RegPacket.SetEndpointDataToggle = OHCI_SetEndpointDataToggle;
    RegPacket.GetEndpointStatus = OHCI_GetEndpointStatus;
    RegPacket.SetEndpointStatus = OHCI_SetEndpointStatus;
    RegPacket.ResetController = OHCI_ResetController;
    RegPacket.RH_GetRootHubData = OHCI_RH_GetRootHubData;
    RegPacket.RH_GetStatus = OHCI_RH_GetStatus;
    RegPacket.RH_GetPortStatus = OHCI_RH_GetPortStatus;
    RegPacket.RH_GetHubStatus = OHCI_RH_GetHubStatus;
    RegPacket.RH_SetFeaturePortReset = OHCI_RH_SetFeaturePortReset;
    RegPacket.RH_SetFeaturePortPower = OHCI_RH_SetFeaturePortPower;
    RegPacket.RH_SetFeaturePortEnable = OHCI_RH_SetFeaturePortEnable;
    RegPacket.RH_SetFeaturePortSuspend = OHCI_RH_SetFeaturePortSuspend;
    RegPacket.RH_ClearFeaturePortEnable = OHCI_RH_ClearFeaturePortEnable;
    RegPacket.RH_ClearFeaturePortPower = OHCI_RH_ClearFeaturePortPower;
    RegPacket.RH_ClearFeaturePortSuspend = OHCI_RH_ClearFeaturePortSuspend;
    RegPacket.RH_ClearFeaturePortEnableChange = OHCI_RH_ClearFeaturePortEnableChange;
    RegPacket.RH_ClearFeaturePortConnectChange = OHCI_RH_ClearFeaturePortConnectChange;
    RegPacket.RH_ClearFeaturePortResetChange = OHCI_RH_ClearFeaturePortResetChange;
    RegPacket.RH_ClearFeaturePortSuspendChange = OHCI_RH_ClearFeaturePortSuspendChange;
    RegPacket.RH_ClearFeaturePortOvercurrentChange = OHCI_RH_ClearFeaturePortOvercurrentChange;
    RegPacket.RH_DisableIrq = OHCI_RH_DisableIrq;
    RegPacket.RH_EnableIrq = OHCI_RH_EnableIrq;
    RegPacket.StartSendOnePacket = OHCI_StartSendOnePacket;
    RegPacket.EndSendOnePacket = OHCI_EndSendOnePacket;
    RegPacket.PassThru = OHCI_PassThru;
    RegPacket.FlushInterrupts = OHCI_Unload;

    DriverObject->DriverUnload = (PDRIVER_UNLOAD)OHCI_Unload;

    Status = USBPORT_RegisterUSBPortDriver(DriverObject, 100, &RegPacket);

    DPRINT_OHCI("DriverEntry: USBPORT_RegisterUSBPortDriver return Status - %x\n",
                Status);

    return Status;
}
