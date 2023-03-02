#ifndef __CBTABLE_H__
#define __CBTABLE_H__

/* The coreboot table information is for conveying information
 * from the firmware to the loaded OS image.  Primarily this
 * is expected to be information that cannot be discovered by
 * other means, such as querying the hardware directly.
 *
 * All of the information should be Position Independent Data.
 * That is it should be safe to relocated any of the information
 * without it's meaning/correctness changing.   For table that
 * can reasonably be used on multiple architectures the data
 * size should be fixed.  This should ease the transition between
 * 32 bit and 64 bit architectures etc.
 *
 * The completeness test for the information in this table is:
 * - Can all of the hardware be detected?
 * - Are the per motherboard constants available?
 * - Is there enough to allow a kernel to run that was written before
 *   a particular motherboard is constructed? (Assuming the kernel
 *   has drivers for all of the hardware but it does not have
 *   assumptions on how the hardware is connected together).
 *
 * With this test it should be straight forward to determine if a
 * table entry is required or not.  This should remove much of the
 * long term compatibility burden as table entries which are
 * irrelevant or have been replaced by better alternatives may be
 * dropped.  Of course it is polite and expedite to include extra
 * table entries and be backwards compatible, but it is not required.
 */

enum {
	LB_TAG_UNUSED = 0x0000,
	LB_TAG_MEMORY = 0x0001,
	LB_TAG_HWRPB = 0x0002,
	LB_TAG_MAINBOARD = 0x0003,
	LB_TAG_VERSION = 0x0004,
	LB_TAG_EXTRA_VERSION = 0x0005,
	LB_TAG_BUILD = 0x0006,
	LB_TAG_COMPILE_TIME = 0x0007,
	LB_TAG_COMPILE_BY = 0x0008,
	LB_TAG_COMPILE_HOST = 0x0009,
	LB_TAG_COMPILE_DOMAIN = 0x000a,
	LB_TAG_COMPILER = 0x000b,
	LB_TAG_LINKER = 0x000c,
	LB_TAG_ASSEMBLER = 0x000d,
	LB_TAG_SERIAL = 0x000f,
	LB_TAG_CONSOLE = 0x0010,
	LB_TAG_FORWARD = 0x0011,
	LB_TAG_FRAMEBUFFER = 0x0012,
	LB_TAG_GPIO = 0x0013,
	LB_TAG_TIMESTAMPS = 0x0016,
	LB_TAG_CBMEM_CONSOLE = 0x0017,
	LB_TAG_MRC_CACHE = 0x0018,
	LB_TAG_VBNV = 0x0019,
	LB_TAG_VBOOT_HANDOFF = 0x0020,  /* deprecated */
	LB_TAG_X86_ROM_MTRR = 0x0021,
	LB_TAG_DMA = 0x0022,
	LB_TAG_RAM_OOPS = 0x0023,
	LB_TAG_ACPI_GNVS = 0x0024,
	LB_TAG_BOARD_ID = 0x0025,  /* deprecated */
	LB_TAG_VERSION_TIMESTAMP = 0x0026,
	LB_TAG_WIFI_CALIBRATION = 0x0027,
	LB_TAG_RAM_CODE = 0x0028,  /* deprecated */
	LB_TAG_SPI_FLASH = 0x0029,
	LB_TAG_SERIALNO = 0x002a,
	LB_TAG_MTC = 0x002b,
	LB_TAG_VPD = 0x002c,
	LB_TAG_SKU_ID = 0x002d,  /* deprecated */
	LB_TAG_BOOT_MEDIA_PARAMS = 0x0030,
	LB_TAG_CBMEM_ENTRY = 0x0031,
	LB_TAG_TSC_INFO = 0x0032,
	LB_TAG_MAC_ADDRS = 0x0033,
	LB_TAG_VBOOT_WORKBUF = 0x0034,
	LB_TAG_MMC_INFO = 0x0035,
	LB_TAG_TCPA_LOG = 0x0036,
	LB_TAG_FMAP = 0x0037,
	LB_TAG_PLATFORM_BLOB_VERSION = 0x0038,
	LB_TAG_SMMSTOREV2 = 0x0039,
	LB_TAG_TPM_PPI_HANDOFF = 0x003a,
	LB_TAG_BOARD_CONFIG = 0x0040,
	LB_TAG_ACPI_CNVS = 0x0041,
	LB_TAG_TYPE_C_INFO = 0x0042,
	LB_TAG_ACPI_RSDP = 0x0043,
	LB_TAG_PCIE = 0x0044,
	/* The following options are CMOS-related */
	LB_TAG_CMOS_OPTION_TABLE = 0x00c8,
	LB_TAG_OPTION = 0x00c9,
	LB_TAG_OPTION_ENUM = 0x00ca,
	LB_TAG_OPTION_DEFAULTS = 0x00cb,
	LB_TAG_OPTION_CHECKSUM = 0x00cc,
};

/* Coreboot table header structure */
struct coreboot_table_header {
	char signature[4];
	UINT32 header_bytes;
	UINT32 header_checksum;
	UINT32 table_bytes;
	UINT32 table_checksum;
	UINT32 table_entries;
};

/* List of coreboot entry structures that is used */
/* Generic */
struct coreboot_table_entry {
	UINT32 tag;
	UINT32 size;
};

/* Points to a CBMEM entry */
struct lb_cbmem_ref {
	UINT32 tag;
	UINT32 size;

	UINT64 cbmem_addr;
};

struct cbmem_console {
	UINT32 size;
	UINT32 cursor;
};
#define CBMC_CURSOR_MASK ((1 << 28) - 1)
#define CBMC_OVERFLOW (1 << 31)

#include <pshpack1.h>
struct timestamp_entry {
	UINT32	entry_id;
	INT64	entry_stamp;
};

struct timestamp_table {
	UINT64	base_time;
	UINT16	max_entries;
	UINT16	tick_freq_mhz;
	UINT32	num_entries;
	struct timestamp_entry entries[1]; /* Variable number of entries */
};

#define MAX_TCPA_LOG_ENTRIES 50
#define TCPA_DIGEST_MAX_LENGTH 64
#define TCPA_PCR_HASH_NAME 50
#define TCPA_PCR_HASH_LEN 10
/* Assumption of 2K TCPA log size reserved for CAR/SRAM */
#define MAX_PRERAM_TCPA_LOG_ENTRIES 15

struct tcpa_entry {
	UINT32 pcr;
	char digest_type[TCPA_PCR_HASH_LEN];
	UINT8 digest[TCPA_DIGEST_MAX_LENGTH];
	UINT32 digest_length;
	char name[TCPA_PCR_HASH_NAME];
};

struct tcpa_table {
	UINT16 max_entries;
	UINT16 num_entries;
	struct tcpa_entry entries[0]; /* Variable number of entries */
};

#include <poppack.h>

#endif /* __CBTABLE_H__ */