#include "hiscore_flash.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <string.h>

#define HS_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

void hsflash_load(hs_table_t *t)
{
    const uint8_t *p = (const uint8_t *)(XIP_BASE + HS_OFFSET);
    uint8_t blob[HS_BLOB_SIZE];
    memcpy(blob, p, HS_BLOB_SIZE);
    if (!hs_decode(t, blob)) hs_clear(t);
}

void hsflash_save(const hs_table_t *t)
{
    static uint8_t page[FLASH_PAGE_SIZE];      /* 256 B */
    memset(page, 0xFF, sizeof page);
    hs_encode(t, page);
    uint32_t irq = save_and_disable_interrupts();
    flash_range_erase(HS_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(HS_OFFSET, page, FLASH_PAGE_SIZE);
    restore_interrupts(irq);
}
