#if !defined(_CBTABLE_H_)
#define _CBTABLE_H_

#pragma warning(disable:4200)  // suppress nameless struct/union warning
#pragma warning(disable:4201)  // suppress nameless struct/union warning
#pragma warning(disable:4214)  // suppress bit field types other than int warning
#include <initguid.h>
#include <wdm.h>

#pragma warning(default:4200)
#pragma warning(default:4201)
#pragma warning(default:4214)
#include <wdf.h>

#include "cbtable.h"

//
// String definitions
//

#define DRIVERNAME                 "crosecbus.sys: "

#define CBTABLE_POOL_TAG            (ULONG) 'CBTB'
#define CBTABLE_HARDWARE_IDS        L"CoolStar\\BOOT0000\0\0"
#define CBTABLE_HARDWARE_IDS_LENGTH sizeof(CBTABLE_HARDWARE_IDS)

#define NTDEVICE_NAME_STRING       L"\\Device\\BOOT0000"
#define SYMBOLIC_NAME_STRING       L"\\DosDevices\\BOOT0000"

#define true 1
#define false 0

typedef struct MEMMAPPING {
	BOOLEAN mapped;
	PHYSICAL_ADDRESS physAddr;
	PVOID virtAddr;
	size_t sz;
} MemMapping, PMemMapping;

enum NextRequest {
	NextRequestConsole,
	NextRequestTimestamps,
	NextRequestRoot,
	NextRequestTcpa,
	NextRequestReserved
};

typedef struct _CBTABLE_CONTEXT
{

	//
	// Handle back to the WDFDEVICE
	//

	WDFDEVICE FxDevice;

	WDFQUEUE CmdQueue;

	MemMapping rootMapping;
	MemMapping consoleMapping;
	MemMapping timestampMapping;
	MemMapping tcpaMapping;

	enum NextRequest nextRequest;

	UINT32 entryCount;

} CBTABLE_CONTEXT, *PCBTABLE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(CBTABLE_CONTEXT, GetDeviceContext)

//
// Function definitions
//

DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_UNLOAD CBTableDriverUnload;

EVT_WDF_DRIVER_DEVICE_ADD CBTableEvtDeviceAdd;

EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL CBTableEvtInternalDeviceControl;

//
// Helper macros
//

#define DEBUG_LEVEL_ERROR   1
#define DEBUG_LEVEL_INFO    2
#define DEBUG_LEVEL_VERBOSE 3

#define DBG_INIT  1
#define DBG_PNP   2
#define DBG_IOCTL 4

#if 1
#define CBTablePrint(dbglevel, dbgcatagory, fmt, ...) {          \
    if (CBTableDebugLevel >= dbglevel &&                         \
        (CBTableDebugCatagories && dbgcatagory))                 \
		    {                                                           \
        DbgPrint(DRIVERNAME);                                   \
        DbgPrint(fmt, __VA_ARGS__);                             \
		    }                                                           \
}
#else
#define CBTablePrint(dbglevel, fmt, ...) {                       \
}
#endif
#endif