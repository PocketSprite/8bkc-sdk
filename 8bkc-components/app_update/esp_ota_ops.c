/*
Replacement OTA code. The 8bkc does not support ota, it has the appfs instead, so most/all these 
functions are gutted/stubbed.
*/

// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "esp_err.h"
#include "esp_partition.h"
#include "esp_spi_flash.h"
#include "esp_image_format.h"
#include "esp_secure_boot.h"
#include "esp_flash_encrypt.h"
#include "sdkconfig.h"

#include "esp_ota_ops.h"
#include "rom/queue.h"
#include "rom/crc.h"
#include "soc/dport_reg.h"
#include "esp_log.h"



//const static char *TAG = "esp_ota_ops";

esp_err_t esp_ota_begin(const esp_partition_t *partition, size_t image_size, esp_ota_handle_t *out_handle)
{
    return ESP_ERR_NOT_FOUND;
}

esp_err_t esp_ota_write(esp_ota_handle_t handle, const void *data, size_t size)
{
    return ESP_ERR_INVALID_ARG;
}

esp_err_t esp_ota_end(esp_ota_handle_t handle)
{
    return ESP_ERR_INVALID_ARG;
}

esp_err_t esp_ota_set_boot_partition(const esp_partition_t *partition)
{
    return ESP_ERR_INVALID_ARG;
}

const esp_partition_t *esp_ota_get_boot_partition(void)
{
    return NULL;
}


static const esp_partition_t* esp_ota_get_running_partition_find_ptype(esp_partition_type_t type)
{
   /* Find the flash address of this exact function. By definition that is part
       of the currently running firmware. Then find the enclosing partition. */

    size_t phys_offs = spi_flash_cache2phys(esp_ota_get_running_partition);

    assert (phys_offs != SPI_FLASH_CACHE2PHYS_FAIL); /* indicates cache2phys lookup is buggy */

    //Look for any partition, so we also get a result if the app is in appfs
    esp_partition_iterator_t it = esp_partition_find(type,
                                                     ESP_PARTITION_SUBTYPE_ANY,
                                                     NULL);

    while (it != NULL) {
        const esp_partition_t *p = esp_partition_get(it);
        if (p->address <= phys_offs && p->address + p->size > phys_offs) {
            esp_partition_iterator_release(it);
            return p;
        }
        it = esp_partition_next(it);
    }
    esp_partition_iterator_release(it);
    return NULL;
}

const esp_partition_t* esp_ota_get_running_partition(void)
{
    const esp_partition_t *ret;
    //Look for 'normal' app partitions (e.g. the factory app partition)
    ret=esp_ota_get_running_partition_find_ptype(ESP_PARTITION_TYPE_APP);
    //Look for appfs partition
    ret=esp_ota_get_running_partition_find_ptype(0x43);
    if (ret) return ret;

    abort(); /* Partition table is invalid or corrupt */
}


const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t *start_from)
{
    return NULL;
}
