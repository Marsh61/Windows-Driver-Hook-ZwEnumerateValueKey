#include <ntddk.h>
#include <wdf.h>
#include <wdm.h>
#include <initguid.h>


DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD InitDevice;
EVT_WDF_DRIVER_UNLOAD DriverUnload;



// struct of the KeServiceDescriptorTable
typedef struct SystemServiceTable {
    UINT32* ServiceTable;
    UINT32* CounterTable;
    UINT32	ServiceLimit;
    UINT32* ArgumentTable;
} SST;


typedef NTSTATUS(*ZwEnumerateValueKeyOriginal) (
    HANDLE                      KeyHandle,
    ULONG                       Index,
    KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass,
    PVOID                       KeyValueInformation,
    ULONG                       Length,
    PULONG                      ResultLength
    );

