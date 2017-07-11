#include "usbehci.h"

//#define NDEBUG
#include <debug.h>

VOID
NTAPI
EHCI_RH_GetRootHubData(IN PVOID ehciExtension,
                       IN PVOID rootHubData)
{
    DPRINT("EHCI_RH_GetRootHubData: UNIMPLEMENTED. FIXME\n");
}

MPSTATUS
NTAPI
EHCI_RH_GetStatus(IN PVOID ehciExtension,
                  IN PUSHORT Status)
{
    DPRINT("EHCI_RH_GetStatus: UNIMPLEMENTED. FIXME\n");
    return 0;
}

MPSTATUS
NTAPI
EHCI_RH_GetPortStatus(IN PVOID ehciExtension,
                      IN USHORT Port,
                      IN PULONG PortStatus)
{
    DPRINT("EHCI_RH_GetPortStatus: UNIMPLEMENTED. FIXME\n");
    return 0;
}

MPSTATUS
NTAPI
EHCI_RH_GetHubStatus(IN PVOID ehciExtension,
                     IN PULONG HubStatus)
{
    DPRINT("EHCI_RH_GetHubStatus: UNIMPLEMENTED. FIXME\n");
    return 0;
}

MPSTATUS
NTAPI
EHCI_RH_SetFeaturePortReset(IN PVOID ehciExtension,
                            IN USHORT Port)
{
    DPRINT("EHCI_RH_SetFeaturePortReset: UNIMPLEMENTED. FIXME\n");
    return 0;
}

MPSTATUS
NTAPI
EHCI_RH_SetFeaturePortPower(IN PVOID ehciExtension,
                            IN USHORT Port)
{
    DPRINT("EHCI_RH_SetFeaturePortPower: UNIMPLEMENTED. FIXME\n");
    return 0;
}

MPSTATUS
NTAPI
EHCI_RH_SetFeaturePortEnable(IN PVOID ehciExtension,
                             IN USHORT Port)
{
    DPRINT("EHCI_RH_SetFeaturePortEnable: UNIMPLEMENTED. FIXME\n");
    return 0;
}

MPSTATUS
NTAPI
EHCI_RH_SetFeaturePortSuspend(IN PVOID ehciExtension,
                              IN USHORT Port)
{
    DPRINT("EHCI_RH_SetFeaturePortSuspend: UNIMPLEMENTED. FIXME\n");
    return 0;
}

MPSTATUS
NTAPI
EHCI_RH_ClearFeaturePortEnable(IN PVOID ehciExtension,
                               IN USHORT Port)
{
    DPRINT("EHCI_RH_ClearFeaturePortEnable: UNIMPLEMENTED. FIXME\n");
    return 0;
}

MPSTATUS
NTAPI
EHCI_RH_ClearFeaturePortPower(IN PVOID ehciExtension,
                              IN USHORT Port)
{
    DPRINT("EHCI_RH_ClearFeaturePortPower: UNIMPLEMENTED. FIXME\n");
    return 0;
}

MPSTATUS
NTAPI
EHCI_RH_ClearFeaturePortSuspend(IN PVOID ehciExtension,
                                IN USHORT Port)
{
    DPRINT("EHCI_RH_ClearFeaturePortSuspend: UNIMPLEMENTED. FIXME\n");
    return 0;
}

MPSTATUS
NTAPI
EHCI_RH_ClearFeaturePortEnableChange(IN PVOID ehciExtension,
                                     IN USHORT Port)
{
    DPRINT("EHCI_RH_ClearFeaturePortEnableChange: UNIMPLEMENTED. FIXME\n");
    return 0;
}

MPSTATUS
NTAPI
EHCI_RH_ClearFeaturePortConnectChange(IN PVOID ehciExtension,
                                      IN USHORT Port)
{
    DPRINT("EHCI_RH_ClearFeaturePortConnectChange: UNIMPLEMENTED. FIXME\n");
    return 0;
}

MPSTATUS
NTAPI
EHCI_RH_ClearFeaturePortResetChange(IN PVOID ehciExtension,
                                    IN USHORT Port)
{
    DPRINT("EHCI_RH_ClearFeaturePortResetChange: UNIMPLEMENTED. FIXME\n");
    return 0;
}

MPSTATUS
NTAPI
EHCI_RH_ClearFeaturePortSuspendChange(IN PVOID ehciExtension,
                                      IN USHORT Port)
{
    DPRINT("EHCI_RH_ClearFeaturePortSuspendChange: UNIMPLEMENTED. FIXME\n");
    return 0;
}

MPSTATUS
NTAPI
EHCI_RH_ClearFeaturePortOvercurrentChange(IN PVOID ehciExtension,
                                          IN USHORT Port)
{
    DPRINT("EHCI_RH_ClearFeaturePortOvercurrentChange: UNIMPLEMENTED. FIXME\n");
    return 0;
}

VOID
NTAPI
EHCI_RH_DisableIrq(IN PVOID ehciExtension)
{
    DPRINT("EHCI_RH_DisableIrq: UNIMPLEMENTED. FIXME\n");
}

VOID
NTAPI
EHCI_RH_EnableIrq(IN PVOID ehciExtension)
{
    DPRINT("EHCI_RH_EnableIrq: UNIMPLEMENTED. FIXME\n");
}

