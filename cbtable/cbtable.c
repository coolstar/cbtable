#include "driver.h"
#include "stdint.h"

static ULONG CBTableDebugLevel = 100;
static ULONG CBTableDebugCatagories = DBG_INIT || DBG_PNP || DBG_IOCTL;

NTSTATUS
DriverEntry(
__in PDRIVER_OBJECT  DriverObject,
__in PUNICODE_STRING RegistryPath
)
{
	NTSTATUS               status = STATUS_SUCCESS;
	WDF_DRIVER_CONFIG      config;
	WDF_OBJECT_ATTRIBUTES  attributes;

	CBTablePrint(DEBUG_LEVEL_INFO, DBG_INIT,
		"Driver Entry\n");

	WDF_DRIVER_CONFIG_INIT(&config, CBTableEvtDeviceAdd);

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

	//
	// Create a framework driver object to represent our driver.
	//

	status = WdfDriverCreate(DriverObject,
		RegistryPath,
		&attributes,
		&config,
		WDF_NO_HANDLE
		);

	if (!NT_SUCCESS(status))
	{
		CBTablePrint(DEBUG_LEVEL_ERROR, DBG_INIT,
			"WdfDriverCreate failed with status 0x%x\n", status);
	}

	return status;
}

NTSTATUS
OnPrepareHardware(
_In_  WDFDEVICE     FxDevice,
_In_  WDFCMRESLIST  FxResourcesRaw,
_In_  WDFCMRESLIST  FxResourcesTranslated
)
/*++

Routine Description:

This routine caches the SPB resource connection ID.

Arguments:

FxDevice - a handle to the framework device object
FxResourcesRaw - list of translated hardware resources that
the PnP manager has assigned to the device
FxResourcesTranslated - list of raw hardware resources that
the PnP manager has assigned to the device

Return Value:

Status

--*/
{
	PCBTABLE_CONTEXT pDevice = GetDeviceContext(FxDevice);
	NTSTATUS status = STATUS_INSUFFICIENT_RESOURCES;

	UNREFERENCED_PARAMETER(FxResourcesRaw);
	
	//
	// Parse resources.
	//

	ULONG resourceCount = WdfCmResourceListGetCount(FxResourcesTranslated);
	
	for (ULONG i = 0; i < resourceCount; i++)
	{
		PCM_PARTIAL_RESOURCE_DESCRIPTOR pDescriptor;

		pDescriptor = WdfCmResourceListGetDescriptor(
			FxResourcesTranslated, i);

		switch (pDescriptor->Type)
		{
		case CmResourceTypeMemory:
			pDevice->rootMapping.physAddr = pDescriptor->u.Memory.Start;
			pDevice->rootMapping.sz = pDescriptor->u.Memory.Length;

			status = STATUS_SUCCESS;
			break;
		default:
			//
			// Ignoring all other resource types.
			//
			break;
		}
	}

	if (!NT_SUCCESS(status))
		return status;

	pDevice->rootMapping.virtAddr = MmMapIoSpace(pDevice->rootMapping.physAddr, pDevice->rootMapping.sz, MmCached);
	if (!pDevice->rootMapping.virtAddr)
		return STATUS_NO_MEMORY;

	pDevice->rootMapping.mapped = TRUE;

	return status;
}

NTSTATUS
OnReleaseHardware(
_In_  WDFDEVICE     FxDevice,
_In_  WDFCMRESLIST  FxResourcesTranslated
)
/*++

Routine Description:

Arguments:

FxDevice - a handle to the framework device object
FxResourcesTranslated - list of raw hardware resources that
the PnP manager has assigned to the device

Return Value:

Status

--*/
{
	NTSTATUS status = STATUS_SUCCESS;

	PCBTABLE_CONTEXT pDevice = GetDeviceContext(FxDevice);
	UNREFERENCED_PARAMETER(FxResourcesTranslated);

	if (pDevice->rootMapping.virtAddr) {
		MmUnmapIoSpace(pDevice->rootMapping.virtAddr, pDevice->rootMapping.sz);
	}

	return status;
}

VOID
OnIoRead(
	_In_  WDFQUEUE    FxQueue,
	_In_  WDFREQUEST  FxRequest,
	_In_  size_t      Length
)
{
	WDFDEVICE device;
	PCBTABLE_CONTEXT pDevice;

	device = WdfIoQueueGetDevice(FxQueue);
	pDevice = GetDeviceContext(device);

	NTSTATUS status;

	PVOID Buffer;
	size_t BufLen;

	status = WdfRequestRetrieveOutputBuffer(FxRequest, Length, &Buffer, &BufLen);
	if (!NT_SUCCESS(status)) {
		DbgPrint("Failed to get output buffer\n");
		goto exit;
	}

	RtlZeroMemory(Buffer, BufLen);

	MemMapping* mapping;
	switch (pDevice->nextRequest) {
	case NextRequestRoot:
		mapping = &pDevice->rootMapping;
		break;
	case NextRequestTcpa:
		mapping = &pDevice->tcpaMapping;
		break;
	case NextRequestTimestamps:
		mapping = &pDevice->timestampMapping;
		break;
	case NextRequestConsole:
	default:
		mapping = &pDevice->consoleMapping;
		break;
	}	

	if (!mapping->mapped) {
		status = STATUS_DEVICE_NOT_READY;
		DbgPrint("Requested mapping not present\n");
	}
	else {
		RtlCopyMemory(Buffer, mapping->virtAddr, min(BufLen, mapping->sz));

		WdfRequestSetInformation(FxRequest, min(BufLen, mapping->sz));
	}

	pDevice->nextRequest = NextRequestConsole;

exit:
	WdfRequestComplete(FxRequest, status);
}

VOID
OnTopLevelIoDefault(
	_In_  WDFQUEUE    FxQueue,
	_In_  WDFREQUEST  FxRequest
)
/*++
  Routine Description:
	Accepts all incoming requests and pends or forwards appropriately.
  Arguments:
	FxQueue -  Handle to the framework queue object that is associated with the
		I/O request.
	FxRequest - Handle to a framework request object.
  Return Value:
	None.
--*/
{
	UNREFERENCED_PARAMETER(FxQueue);

	WDFDEVICE device;
	PCBTABLE_CONTEXT pDevice;
	WDF_REQUEST_PARAMETERS params;
	NTSTATUS status;

	device = WdfIoQueueGetDevice(FxQueue);
	pDevice = GetDeviceContext(device);

	WDF_REQUEST_PARAMETERS_INIT(&params);

	WdfRequestGetParameters(FxRequest, &params);

	status = WdfRequestForwardToIoQueue(FxRequest, pDevice->CmdQueue);

	if (!NT_SUCCESS(status))
	{
		CBTablePrint(DEBUG_LEVEL_INFO, DBG_PNP,
			"Failed to forward WDFREQUEST %p to SPB queue %p - 0x%x",
			FxRequest,
			pDevice->CmdQueue,
			status);

		WdfRequestComplete(FxRequest, status);
	}
}

VOID
OnIoWrite(
	_In_  WDFQUEUE    FxQueue,
	_In_  WDFREQUEST  FxRequest,
	_In_  size_t      Length
)
{
	WDFDEVICE device;
	PCBTABLE_CONTEXT pDevice;

	device = WdfIoQueueGetDevice(FxQueue);
	pDevice = GetDeviceContext(device);

	NTSTATUS status;

	PVOID Buffer;
	size_t BufLen;

	status = WdfRequestRetrieveInputBuffer(FxRequest, Length, &Buffer, &BufLen);
	if (!NT_SUCCESS(status)) {
		DbgPrint("Failed to get input buffer\n");
		goto exit;
	}

	if (BufLen < sizeof(pDevice->nextRequest)) {
		DbgPrint("Input buffer too small\n");
		status = STATUS_INVALID_PARAMETER;
	}
	else {
		enum NextRequest param = ((enum NextRequest *)Buffer)[0];
		if (param < NextRequestConsole || param >= NextRequestReserved) {
			status = STATUS_INVALID_PARAMETER;
		}
		else {
			pDevice->nextRequest = param;
		}
	}

exit:
	WdfRequestComplete(FxRequest, status);
}

/*
 * calculate ip checksum (16 bit quantities) on a passed in buffer. In case
 * the buffer length is odd last byte is excluded from the calculation
 */
static UINT16 ipchcksum(const void* addr, unsigned size)
{
	const UINT16* p = addr;
	unsigned i, n = size / 2; /* don't expect odd sized blocks */
	UINT32 sum = 0;

	for (i = 0; i < n; i++)
		sum += p[i];

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	sum = ~sum & 0xffff;
	return (UINT16)sum;
}

static void mapConsole(PCBTABLE_CONTEXT pDevice) {
	struct cbmem_console* console_p;
	size_t size, cursor;

	size = sizeof(*console_p);

	size_t lastMapping = size;
	console_p = MmMapIoSpace(pDevice->consoleMapping.physAddr, lastMapping, MmCached);
	if (!console_p)
		return;

	cursor = console_p->cursor & CBMC_CURSOR_MASK;
	if (!(console_p->cursor & CBMC_OVERFLOW) && cursor < console_p->size)
		size = cursor;
	else
		size = console_p->size;
	MmUnmapIoSpace(console_p, lastMapping);

	lastMapping = size + sizeof(*console_p);
	console_p = MmMapIoSpace(pDevice->consoleMapping.physAddr, lastMapping, MmCached);
	if (!console_p)
		return;

	pDevice->consoleMapping.virtAddr = console_p;
	pDevice->consoleMapping.sz = lastMapping;
	pDevice->consoleMapping.mapped = TRUE;
}

static void mapTimestamps(PCBTABLE_CONTEXT pDevice) {
	struct timestamp_table* timestamp_p;
	size_t size, cursor;

	size = sizeof(*timestamp_p);

	size_t lastMapping = size;
	timestamp_p = MmMapIoSpace(pDevice->timestampMapping.physAddr, lastMapping, MmCached);
	if (!timestamp_p)
		return;

	size += timestamp_p->num_entries * sizeof(timestamp_p->entries[0]);
	MmUnmapIoSpace(timestamp_p, lastMapping);

	lastMapping = size;
	timestamp_p = MmMapIoSpace(pDevice->timestampMapping.physAddr, lastMapping, MmCached);
	if (!timestamp_p)
		return;

	pDevice->timestampMapping.virtAddr = timestamp_p;
	pDevice->timestampMapping.sz = lastMapping;
	pDevice->timestampMapping.mapped = TRUE;
}

static void mapTcpa(PCBTABLE_CONTEXT pDevice) {
	struct tcpa_table* tcpa_p;
	size_t size, cursor;

	size = sizeof(*tcpa_p);

	size_t lastMapping = size;
	tcpa_p = MmMapIoSpace(pDevice->tcpaMapping.physAddr, lastMapping, MmCached);
	if (!tcpa_p)
		return;

	size += tcpa_p->num_entries * sizeof(tcpa_p->entries[0]);
	MmUnmapIoSpace(tcpa_p, lastMapping);

	lastMapping = size;
	tcpa_p = MmMapIoSpace(pDevice->tcpaMapping.physAddr, lastMapping, MmCached);
	if (!tcpa_p)
		return;

	pDevice->tcpaMapping.virtAddr = tcpa_p;
	pDevice->tcpaMapping.sz = lastMapping;
	pDevice->tcpaMapping.mapped = TRUE;
}

NTSTATUS
OnD0Entry(
_In_  WDFDEVICE               FxDevice,
_In_  WDF_POWER_DEVICE_STATE  FxPreviousState
)
/*++

Routine Description:

This routine allocates objects needed by the driver.

Arguments:

FxDevice - a handle to the framework device object
FxPreviousState - previous power state

Return Value:

Status

--*/
{
	UNREFERENCED_PARAMETER(FxPreviousState);
	NTSTATUS status = STATUS_SUCCESS;

	PCBTABLE_CONTEXT pDevice = GetDeviceContext(FxDevice);

	struct coreboot_table_header* hdr = pDevice->rootMapping.virtAddr;
	if (memcmp(hdr->signature, "LBIO", 4) != 0) {
		DbgPrint("Invalid coreboot table\n");
		return STATUS_INVALID_DEVICE_STATE;
	}

	UINT32 checksum = ipchcksum((UINT8*)hdr + hdr->header_bytes, hdr->table_bytes);
	if (hdr->table_checksum != checksum) {
		DbgPrint("Invalid cbmem checksum 0x%x vs 0x%x\n", hdr->table_checksum, checksum);
		return STATUS_INVALID_DEVICE_STATE;
	}

	UINT8* entryStart = (UINT8 *)hdr + hdr->header_bytes;
	for (int i = 0; i < hdr->table_entries; i++) {
		struct coreboot_table_entry* entry = entryStart;

		if (entry->tag == LB_TAG_CBMEM_CONSOLE) {
			struct lb_cbmem_ref* console = entry;
			DbgPrint("Found cbmem console at 0x%llx\n", console->cbmem_addr);

			pDevice->consoleMapping.physAddr.QuadPart = console->cbmem_addr;

			mapConsole(pDevice);
		} else if (entry->tag == LB_TAG_TIMESTAMPS) {
			struct lb_cbmem_ref* timestamps = entry;
			DbgPrint("Found cbmem timestamps at 0x%llx\n", timestamps->cbmem_addr);

			pDevice->timestampMapping.physAddr.QuadPart = timestamps->cbmem_addr;

			mapTimestamps(pDevice);
		} else if (entry->tag == LB_TAG_TCPA_LOG) {
			struct lb_cbmem_ref* tcpa_log = entry;
			DbgPrint("Found cbmem tcpa at 0x%llx\n", tcpa_log->cbmem_addr);

			pDevice->tcpaMapping.physAddr.QuadPart = tcpa_log->cbmem_addr;

			mapTcpa(pDevice);
		}

		entryStart += entry->size;
	}

	return status;
}

NTSTATUS
OnD0Exit(
_In_  WDFDEVICE               FxDevice,
_In_  WDF_POWER_DEVICE_STATE  FxTargetState
)
/*++

Routine Description:

This routine destroys objects needed by the driver.

Arguments:

FxDevice - a handle to the framework device object
FxTargetState - target power state

Return Value:

Status

--*/
{
	UNREFERENCED_PARAMETER(FxTargetState);

	NTSTATUS status = STATUS_SUCCESS;

	PCBTABLE_CONTEXT pDevice = GetDeviceContext(FxDevice);
	if (pDevice->consoleMapping.mapped) {
		MmUnmapIoSpace(pDevice->consoleMapping.virtAddr, pDevice->consoleMapping.sz);
	}

	if (pDevice->timestampMapping.mapped) {
		MmUnmapIoSpace(pDevice->timestampMapping.virtAddr, pDevice->timestampMapping.sz);
	}

	if (pDevice->tcpaMapping.mapped) {
		MmUnmapIoSpace(pDevice->tcpaMapping.virtAddr, pDevice->tcpaMapping.sz);
	}

	return status;
}

NTSTATUS
CBTableEvtDeviceAdd(
IN WDFDRIVER       Driver,
IN PWDFDEVICE_INIT DeviceInit
)
{
	NTSTATUS                      status = STATUS_SUCCESS;
	WDF_OBJECT_ATTRIBUTES         attributes;
	WDFDEVICE                     device;
	UCHAR                         minorFunction;
	PCBTABLE_CONTEXT               devContext;
	WDF_QUERY_INTERFACE_CONFIG  qiConfig;

	UNREFERENCED_PARAMETER(Driver);

	PAGED_CODE();

	CBTablePrint(DEBUG_LEVEL_INFO, DBG_PNP,
		"CBTableEvtDeviceAdd called\n");

	{
		WDF_PNPPOWER_EVENT_CALLBACKS pnpCallbacks;
		WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpCallbacks);

		pnpCallbacks.EvtDevicePrepareHardware = OnPrepareHardware;
		pnpCallbacks.EvtDeviceReleaseHardware = OnReleaseHardware;
		pnpCallbacks.EvtDeviceD0Entry = OnD0Entry;
		pnpCallbacks.EvtDeviceD0Exit = OnD0Exit;

		WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpCallbacks);
	}

	//
	// Prepare for file object handling.
	//

	{
		WDF_FILEOBJECT_CONFIG fileObjectConfig;

		WDF_FILEOBJECT_CONFIG_INIT(
			&fileObjectConfig,
			NULL,
			NULL,
			NULL);

		WDF_OBJECT_ATTRIBUTES fileObjectAttributes;
		WDF_OBJECT_ATTRIBUTES_INIT(&fileObjectAttributes);

		WdfDeviceInitSetFileObjectConfig(
			DeviceInit,
			&fileObjectConfig,
			&fileObjectAttributes);
	}

	//
	// Setup the device context
	//

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, CBTABLE_CONTEXT);

	//
	// Create a framework device object.This call will in turn create
	// a WDM device object, attach to the lower stack, and set the
	// appropriate flags and attributes.
	//

	status = WdfDeviceCreate(&DeviceInit, &attributes, &device);

	if (!NT_SUCCESS(status))
	{
		CBTablePrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"WdfDeviceCreate failed with status code 0x%x\n", status);

		return status;
	}

	devContext = GetDeviceContext(device);
	devContext->FxDevice = device;

	WDF_IO_QUEUE_CONFIG queueConfig;
	WDFQUEUE queue;

	//
	// Top-level queue
	//

	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
		&queueConfig,
		WdfIoQueueDispatchParallel);
	queueConfig.EvtIoDefault = OnTopLevelIoDefault;
	queueConfig.PowerManaged = WdfFalse;

	status = WdfIoQueueCreate(
		devContext->FxDevice,
		&queueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&queue
	);

	if (!NT_SUCCESS(status))
	{
		CBTablePrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"Error creating top-level IO queue - 0x%x",
			status);

		return status;
	}

	//
	// Create I/O queue
	//
	WDF_IO_QUEUE_CONFIG_INIT(
		&queueConfig,
		WdfIoQueueDispatchSequential);

	queueConfig.EvtIoRead = OnIoRead;
	queueConfig.EvtIoWrite = OnIoWrite;
	//queueConfig.EvtIoDeviceControl = OnIoDeviceControl;
	queueConfig.PowerManaged = WdfFalse;

	status = WdfIoQueueCreate(
		devContext->FxDevice,
		&queueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&devContext->CmdQueue
	);
	if (!NT_SUCCESS(status))
	{
		CBTablePrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"WdfIoQueueCreate failed 0x%x\n", status);

		return status;
	}

	DECLARE_CONST_UNICODE_STRING(dosDeviceName, SYMBOLIC_NAME_STRING);

	status = WdfDeviceCreateSymbolicLink(device,
		&dosDeviceName
	);
	if (!NT_SUCCESS(status)) {
		CBTablePrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"WdfDeviceCreateSymbolicLink failed 0x%x\n", status);
		return status;
	}

	return status;
}
