#ifndef APPFS_H
#define APPFS_H

#include "esp_err.h"
#include <stdint.h>
#include "esp_spi_flash.h"


typedef int appfs_handle_t;

#define APPFS_INVALID_FD -1

esp_err_t appfsInit(int type, int subtype);
int appfsExists(char *filename);
bool appfsFdValid(int fd);
appfs_handle_t appfsOpen(char *filename);
void appfsClose(appfs_handle_t handle);
esp_err_t appfsDeleteFile(char *filename);
esp_err_t appfsCreateFile(char *filename, size_t size, appfs_handle_t *handle);
esp_err_t appfsMmap(appfs_handle_t fd, size_t offset, size_t len, const void** out_ptr, 
									spi_flash_mmap_memory_t memory, spi_flash_mmap_handle_t* out_handle);
esp_err_t appfsErase(appfs_handle_t fd, size_t start, size_t len);
esp_err_t appfsWrite(appfs_handle_t fd, size_t start, uint8_t *buf, size_t len);
void appfsDump();
void appfsEntryInfo(appfs_handle_t fd, const char **name, int *size);
appfs_handle_t appfsNextEntry(appfs_handle_t fd);
size_t appfsGetFreeMem();

#ifdef BOOTLOADER_BUILD
#include "bootloader_flash.h"
typedef struct {
	uint32_t fileAddr;
	uint32_t mapAddr;
	uint32_t length;
} AppfsBlRegionToMap;

esp_err_t appfsBlInit(uint32_t offset, uint32_t len);
void appfsBlDeinit();
void* appfsBlMmap(int fd);
void appfsBlMunmap();
esp_err_t appfsBlMapRegions(int fd, AppfsBlRegionToMap *regions, int noRegions);

#endif

#endif
