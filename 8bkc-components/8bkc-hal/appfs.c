/*
Theory of operation:
An appfs filesystem is meant to store executable applications (=ESP32 programs) alongside other
data that is mmap()-able as a contiguous file.

Appfs does that by making sure the files rigidly adhere to the 64K-page-structure (called a 'sector'
in this description) as predicated by the ESP32s MMU. This way, every file can be mmap()'ed into a 
contiguous region or ran as an ESP32 application. (For the future, maybe: Smaller files can be stored 
in parts of a 64K page, as long as all are contiguous and none cross any 64K boundaries. 
What about fragmentation tho?)

Because of these reasons, only a few operations are available:
- Creating a file. This needs the filesize to be known beforehand; a file cannot change size afterwards.
- Modifying a file. This follows the same rules as spi_flash_*  because it maps directly to the underlying flash.
- Deleting a file
- Mmap()ping a file

At the moment, appfs is not yet tested with encrypted flash; compatibility is unknown.

Filesystem meta-info is stored using the first sector: there are 2 32K half-sectors there with management
info. Each has a serial and a checksum. The sector with the highest serial and a matching checksum is
taken as current; the data will ping-pong between the sectors. (And yes, this means the pages in these
sectors will be rewritten every time a file is added/removed. Appfs is built with the assumption that
it's a mostly store-only filesystem and apps will only change every now and then. The flash chips 
connected to the ESP32 chips usually can do up to 100.000 erases, so for most purposes the lifetime of
the flash with appfs on it exceeds the lifetime of the product.)

Appfs assumes a partition of 16MiB or less, allowing for 256 128-byte sector descriptors to be stored in
the management half-sectors. The first descriptor is a header used for filesystem meta-info.

Metainfo is stored per sector; each sector descriptor contains a zero-terminated filename (no 
directories are supported, but '/' is an usable character), the size of the file and a pointer to the
next entry for the file if needed. The filename is only set for the first sector; it is all zeroes 
(actually: ignored) for other entries.

Integrity of the meta-info is guaranteed: the file system will never be in a state where sectors are 
lost or anything. Integrity of data is *NOT* guaranteed: on power loss, data may be half-written,
contain sectors with only 0xff, and so on. It's up to the user to take care of this. However, files that
are not written to do not run the risk of getting corrupted.

With regards to this code: it is assumed that an ESP32 will only have one appfs on flash, so everything
is implemented as a singleton.

*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <rom/crc.h>
#include "esp_image_format.h"
#include "esp_spi_flash.h"
#include "esp_partition.h"
#include "esp_log.h"
#include "esp_err.h"
#include "appfs.h"

#include "hexdump.h"

static const char *TAG = "appfs";


#define APPFS_SECTOR_SZ SPI_FLASH_MMU_PAGE_SIZE
#define APPFS_META_SZ (APPFS_SECTOR_SZ/2)
#define APPFS_META_CNT 2
#define APPFS_META_DESC_SZ 128
#define APPFS_PAGES 255
#define APPFS_MAGIC "AppFsDsc"

#define APPFS_USE_FREE 0xff		//No file allocated here
#define APPFS_ILLEGAL 0x55		//Sector cannot be used (usually because it's outside the partition)
#define APPFS_USE_DATA 0		//Sector is in use for data

typedef struct {
	uint8_t magic[8]; //must be AppFsDsc
	uint32_t serial;
	uint32_t crc32;
	uint8_t reserved[128-16];
} AppfsHeader;

typedef struct {
	char name[112]; //Only set for 1st sector of file. Rest has name set to 0xFF 0xFF ...
	uint32_t size; //in bytes
	uint8_t next; //next page containing the next 64K of the file; 0 if no next page
	uint8_t used; //one of APPFS_USE_*
	uint8_t reserved[10];
} AppfsPageInfo;

typedef struct {
	AppfsHeader hdr;
	AppfsPageInfo page[APPFS_PAGES];
} AppfsMeta;

static const esp_partition_t *appfsPart=NULL;
static int appfsActiveMeta=0; //number of currently active metadata half-sector (0 or 1)
static const AppfsMeta *appfsMeta=NULL; //mmap'ed flash
static spi_flash_mmap_handle_t appfsMetaMmapHandle;

//Find active meta half-sector. Updates appfsActiveMeta to the most current one and returns ESP_OK success.
//Returns ESP_ERR_NOT_FOUND when no active metasector is found.
static esp_err_t findActiveMeta() {
	int validSec=0; //bitmap of valid sectors
	uint32_t serial[APPFS_META_CNT]={0};
	AppfsHeader hdr;
	for (int sec=0; sec<APPFS_META_CNT; sec++) {
		//Read header
		memcpy(&hdr, &appfsMeta[sec], sizeof(AppfsHeader));
		if (memcmp(hdr.magic, APPFS_MAGIC, 8)==0) {
			//Save serial
			serial[sec]=hdr.serial;
			//Save and zero CRC
			uint32_t expectedCrc=hdr.crc32;
			hdr.crc32=0;
			uint32_t crc=0;
			crc=crc32_le(crc, (const uint8_t *)&hdr, APPFS_META_DESC_SZ);
			for (int j=0; j<APPFS_PAGES; j++) {
				crc=crc32_le(crc, (const uint8_t *)&appfsMeta[sec].page[j], APPFS_META_DESC_SZ);
			}
			if (crc==expectedCrc) validSec|=(1<<sec);
		}
	}
	//Here, validSec should be a bitmap of sectors that are valid, while serials[] should contain their
	//serials.
	int best=-1;
	for (int sec=0; sec<APPFS_META_CNT; sec++) {
		if (validSec&(1<<sec)) {
			if (best!=-1 || serial[sec]>serial[best]) best=sec;
		}
	}
	//'best' here is either still -1 (no valid sector found) or the sector with the highest valid serial.
	if (best==-1) {
		//Eek! Nothing found!
		return ESP_ERR_NOT_FOUND;
	}

	ESP_LOGI(TAG, "Meta page 0: %svalid (serial %d)", (validSec&1)?"":"in", serial[0]);
	ESP_LOGI(TAG, "Meta page 1: %svalid (serial %d)", (validSec&2)?"":"in", serial[1]);
	ESP_LOGI(TAG, "using page %d as current.", best);
	appfsActiveMeta=best;
	return ESP_OK;
}


//Modifies the header in hdr to the correct crc and writes it to meta info no metano.
//Assumes the serial etc is in order already, and the header section for metano has been erased.
static esp_err_t writeHdr(AppfsHeader *hdr, int metaNo) {
	hdr->crc32=0;
	uint32_t crc=0;
	crc=crc32_le(crc, (const uint8_t *)hdr, APPFS_META_DESC_SZ);
	for (int j=0; j<APPFS_PAGES; j++) {
		crc=crc32_le(crc, (const uint8_t *)&appfsMeta[metaNo].page[j], APPFS_META_DESC_SZ);
	}
	hdr->crc32=crc;
	return esp_partition_write(appfsPart, metaNo*APPFS_META_SZ, hdr, sizeof(AppfsHeader));
}

//Kill all existing filesystem metadata and re-initialize the fs.
static esp_err_t initializeFs() {
	esp_err_t r;
	//Kill management sector
	r=esp_partition_erase_range(appfsPart, 0, APPFS_SECTOR_SZ);
	if (r!=ESP_OK) return r;
	//All the data pages are now set to 'free'. Add a header that makes the entire mess valid.
	AppfsHeader hdr;
	memset(&hdr, 0xff, sizeof(hdr));
	memcpy(hdr.magic, APPFS_MAGIC, 8);
	hdr.serial=0;
	//Mark pages outside of partition as invalid.
	int lastPage=(appfsPart->size/APPFS_SECTOR_SZ);
	for (int j=lastPage; j<APPFS_PAGES; j++) {
		AppfsPageInfo pi;
		memset(&pi, 0xff, sizeof(pi));
		pi.used=APPFS_ILLEGAL;
		r=esp_partition_write(appfsPart, 0*APPFS_META_SZ+(j+1)*APPFS_META_DESC_SZ, &pi, sizeof(pi));
		if (r!=ESP_OK) return r;
	}
	writeHdr(&hdr, 0);
	ESP_LOGI(TAG, "Re-initialized appfs: %d pages", lastPage);
	//Officially, we should also write the CRCs... we don't do this here because during the
	//runtime of this, the CRCs aren't checked and when the device reboots, it'll re-initialize
	//the fs anyway.
	appfsActiveMeta=0;
	return ESP_OK;
}


esp_err_t appfsInit(int type, int subtype) {
	esp_err_t r;
	//Compile-time sanity check on size of structs
	_Static_assert(sizeof(AppfsHeader)==APPFS_META_DESC_SZ, "sizeof AppfsHeader != 128bytes");
	_Static_assert(sizeof(AppfsPageInfo)==APPFS_META_DESC_SZ, "sizeof AppfsPageInfo != 128bytes");
	_Static_assert(sizeof(AppfsMeta)==APPFS_META_SZ, "sizeof AppfsMeta != APPFS_META_SZ");
	//Find the indicated partition
	appfsPart=esp_partition_find_first(type, subtype, NULL);
	if (!appfsPart) return ESP_ERR_NOT_FOUND;
	//Memory map the appfs header so we can Do Stuff with it
	r=esp_partition_mmap(appfsPart, 0, APPFS_SECTOR_SZ, SPI_FLASH_MMAP_DATA, (const void**)&appfsMeta, &appfsMetaMmapHandle);
	if (r!=ESP_OK) return r;
//	if (findActiveMeta()!=ESP_OK) {
		//No valid metadata half-sector found. Initialize the first sector.
		ESP_LOGE(TAG, "No valid meta info found. Re-initializing fs.");
		initializeFs();
//	}
	ESP_LOGD(TAG, "Initialized.");
	return ESP_OK;
}

static int appfsGetFirstPageFor(const char *filename) {
	for (int j=0; j<APPFS_PAGES; j++) {
		if (appfsMeta[appfsActiveMeta].page[j].used==APPFS_USE_DATA && strcmp(appfsMeta[appfsActiveMeta].page[j].name, filename)==0) {
			return j;
		}
	}
	//Nothing found.
	return -1;
}

static bool appfsFdValid(int fd) {
	if (fd<0 || fd>=APPFS_PAGES) return false;
	if (appfsMeta[appfsActiveMeta].page[(int)fd].used!=APPFS_USE_DATA) return false;
	if (appfsMeta[appfsActiveMeta].page[(int)fd].name[0]==0xff) return false;
	return true;
}

static esp_err_t writePageInfo(int newMeta, int page, AppfsPageInfo *pi) {
	return esp_partition_write(appfsPart, newMeta*APPFS_META_SZ+(page+1)*APPFS_META_DESC_SZ, pi, sizeof(AppfsPageInfo));
}

int appfsExists(char *filename) {
	return (appfsGetFirstPageFor(filename)==-1)?0:1;
}

appfs_handle_t appfsOpen(char *filename) {
	return appfsGetFirstPageFor(filename);
}

void appfsClose(appfs_handle_t handle) {
	//Not needed in this implementation. Added for possible later use (concurrency?)
}

//This essentially writes a new meta page without any references to the file indicated.
esp_err_t appfsDeleteFile(char *filename) {
	esp_err_t r;
	int next=-1;
	int newMeta;
	AppfsHeader hdr;
	AppfsPageInfo pi;
	//See if we actually need to do something
	if (!appfsExists(filename)) return 0;
	//Create a new management sector
	newMeta=(appfsActiveMeta+1)%APPFS_META_CNT;
	r=esp_partition_erase_range(appfsPart, newMeta*APPFS_META_SZ, APPFS_META_SZ);
	if (r!=ESP_OK) return r;
	//Prepare header
	memcpy(&hdr, &appfsMeta[appfsActiveMeta].hdr, sizeof(hdr));
	hdr.serial++;
	hdr.crc32=0;
	for (int j=0; j<APPFS_PAGES; j++) {
		int needDelete=0;
		//Grab old page info from current meta sector
		memcpy(&pi, &appfsMeta[appfsActiveMeta].page[j], sizeof(pi));
		if (next==-1) {
			if (pi.used==APPFS_USE_DATA && strcmp(pi.name, filename)==0) {
				needDelete=1;
				next=pi.next;
			}
		} else if (next==0) {
			//File is killed entirely. No need to look for anything.
		} else {
			//Look for next sector of file
			if (j==next) {
				needDelete=1;
				next=pi.next;
			}
		}
		if (needDelete) {
			//Page info is 0xff anyway. No need to explicitly write that.
		} else {
			r=writePageInfo(newMeta, j, &pi);
			if (r!=ESP_OK) return r;
		}
	}
	r=writeHdr(&hdr, newMeta);
	appfsActiveMeta=newMeta;
	return r;
}


//Allocate space for a new file. Will kill any existing files if needed.
//Warning: may kill old file but not create new file if new file won't fit on fs, even with old file removed.
//ToDo: in that case, fail before deleting file.
esp_err_t appfsCreateFile(char *filename, size_t size, appfs_handle_t *handle) {
	esp_err_t r;
	//If there are any references to this file, kill 'em.
	appfsDeleteFile(filename);
	ESP_LOGD(TAG, "Creating new file '%s'", filename);

	//Figure out what pages to reserve for the file, and the next link structure.
	uint8_t nextForPage[APPFS_PAGES]; //Next page if used for file, APPFS_PAGES if not
	//Mark all pages as unused for file.
	for (int j=0; j<APPFS_PAGES; j++) nextForPage[j]=APPFS_PAGES;
	//Find pages where we can store data for the file.
	int first=-1, prev=-1;
	int sizeLeft=size;
	for (int j=0; j<APPFS_PAGES; j++) {
		if (appfsMeta[appfsActiveMeta].page[j].used==APPFS_USE_FREE) {
			ESP_LOGD(TAG, "Using page %d...", j);
			if (prev==-1) {
				first=j; //first free page; save to store name here.
			} else {
				nextForPage[prev]=j; //mark prev page to go here
			}
			nextForPage[j]=0; //end of file... for now.
			prev=j;
			sizeLeft-=APPFS_SECTOR_SZ;
			if (sizeLeft<=0) break;
		}
	}

	if (sizeLeft>0) {
		//Eek! Can't allocate enough space!
		ESP_LOGD(TAG, "Not enough free space!\n");
		return ESP_ERR_NO_MEM;
	}

	//Re-write a new meta page but with file allocated
	int newMeta=(appfsActiveMeta+1)%APPFS_META_CNT;
	ESP_LOGD(TAG, "Re-writing meta data to meta page %d...", newMeta);
	r=esp_partition_erase_range(appfsPart, newMeta*APPFS_META_SZ, APPFS_META_SZ);
	if (r!=ESP_OK) return r;
	//Prepare header
	AppfsHeader hdr;
	memcpy(&hdr, &appfsMeta[appfsActiveMeta].hdr, sizeof(hdr));
	hdr.serial++;
	hdr.crc32=0;
	for (int j=0; j<APPFS_PAGES; j++) {
		AppfsPageInfo pi;
		if (nextForPage[j]!=APPFS_PAGES) {
			//This is part of the file. Rewrite page data to indicate this.
			memset(&pi, 0xff, sizeof(pi));
			if (j==first) {
				//First page. Copy name and size.
				strcpy(pi.name, filename);
				pi.size=size;
			}
			pi.used=APPFS_USE_DATA;
			pi.next=nextForPage[j];
		} else {
			//Grab old page info from current meta sector
			memcpy(&pi, &appfsMeta[appfsActiveMeta].page[j], sizeof(pi));
		}
		if (pi.used!=APPFS_USE_FREE) {
			r=writePageInfo(newMeta, j, &pi);
			if (r!=ESP_OK) return r;
		}
	}
	//Write header and make active.
	r=writeHdr(&hdr, newMeta);
	appfsActiveMeta=newMeta;
	if (handle) *handle=first;
	ESP_LOGD(TAG, "Re-writing meta data done.");
	return r;
}

esp_err_t appfsMmap(appfs_handle_t fd, size_t offset, size_t len, const void** out_ptr, 
									spi_flash_mmap_memory_t memory, spi_flash_mmap_handle_t* out_handle) {
	esp_err_t r;
	int page=(int)fd;
	if (!appfsFdValid(page)) return ESP_ERR_NOT_FOUND;
	ESP_LOGD(TAG, "Mmapping file %s, offset %d, size %d", appfsMeta[appfsActiveMeta].page[page].name, offset, len);
	if (appfsMeta[appfsActiveMeta].page[page].size < (offset+len)) {
		return ESP_ERR_INVALID_SIZE;
	}
	int dataStartPage=(appfsPart->address/SPI_FLASH_MMU_PAGE_SIZE)+1;
	while (offset > APPFS_SECTOR_SZ) {
		page=appfsMeta[appfsActiveMeta].page[page].next;
		offset-=APPFS_SECTOR_SZ;
	}

	int *pages=alloca(sizeof(int)*((len/APPFS_SECTOR_SZ)+1));
	int nopages=0;
	while(len>0) {
		pages[nopages++]=page+dataStartPage;
		ESP_LOGD(TAG, "Mapping page %d (part offset %d).", page, dataStartPage);
		page=appfsMeta[appfsActiveMeta].page[page].next;
		len-=APPFS_SECTOR_SZ;
	}

	r=spi_flash_mmap_pages(pages, nopages, memory, out_ptr, out_handle);
	if (r!=ESP_OK) return r;
	*out_ptr=((uint8_t*)*out_ptr)+offset;
	return ESP_OK;
}

esp_err_t appfsErase(appfs_handle_t fd, size_t start, size_t len) {
	esp_err_t r;
	int page=(int)fd;
	if (!appfsFdValid(page)) return ESP_ERR_NOT_FOUND;
	if (appfsMeta[appfsActiveMeta].page[page].size < (start+len)) {
		return ESP_ERR_INVALID_SIZE;
	}

	while (start > APPFS_SECTOR_SZ) {
		page=appfsMeta[appfsActiveMeta].page[page].next;
		start-=APPFS_SECTOR_SZ;
	}
	while (len>0) {
		size_t size=len;
		if (size>APPFS_SECTOR_SZ) size=APPFS_SECTOR_SZ;
		ESP_LOGD(TAG, "Erasing page %d", page);
		r=esp_partition_erase_range(appfsPart, (page+1)*APPFS_SECTOR_SZ, size);
		if (r!=ESP_OK) return r;
		page=appfsMeta[appfsActiveMeta].page[page].next;
		len-=size;
	}
	return ESP_OK;
}

esp_err_t appfsWrite(appfs_handle_t fd, size_t start, uint8_t *buf, size_t len) {
	esp_err_t r;
	int page=(int)fd;
	if (!appfsFdValid(page)) return ESP_ERR_NOT_FOUND;
	if (appfsMeta[appfsActiveMeta].page[page].size < (start+len)) {
		return ESP_ERR_INVALID_SIZE;
	}

	while (start > APPFS_SECTOR_SZ) {
		page=appfsMeta[appfsActiveMeta].page[page].next;
		start-=APPFS_SECTOR_SZ;
	}
	while (len>0) {
		size_t size=len;
		if (size+start>APPFS_SECTOR_SZ) size=APPFS_SECTOR_SZ-start;
		ESP_LOGD(TAG, "Writing to page %d offset %d size %d", page, start, size);
		r=esp_partition_write(appfsPart, (page+1)*APPFS_SECTOR_SZ+start, buf, size);
		if (r!=ESP_OK) return r;
		page=appfsMeta[appfsActiveMeta].page[page].next;
		len-=size;
		buf+=size;
		start=0;
	}
	return ESP_OK;
}

void appfsDump() {
	printf("AppFsDump: ..=free XX=illegal no=next page\n");
	for (int i=0; i<16; i++) printf("%02X-", i);
	printf("\n");
	for (int i=0; i<APPFS_PAGES; i++) {
		if (appfsMeta[appfsActiveMeta].page[i].used==APPFS_USE_FREE) {
			printf("..");
		} else if (appfsMeta[appfsActiveMeta].page[i].used==APPFS_USE_DATA) {
			printf("%02X", appfsMeta[appfsActiveMeta].page[i].next);
		} else if (appfsMeta[appfsActiveMeta].page[i].used==APPFS_ILLEGAL) {
			printf("XX");
		} else {
			printf("??");
		}
		if ((i&15)==15) {
			printf("\n");
		} else {
			printf(" ");
		}
	}
	printf("\n");
	for (int i=0; i<APPFS_PAGES; i++) {
		if (appfsMeta[appfsActiveMeta].page[i].used==APPFS_USE_DATA && appfsMeta[appfsActiveMeta].page[i].name[0]!=0xff) {
			printf("File %s starts at page %d\n", appfsMeta[appfsActiveMeta].page[i].name, i);
		}
	}
}
