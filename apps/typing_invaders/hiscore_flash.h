// hiscore_flash.h - persist the hs_table_t in the last 4 KB flash sector.
// CONTRACT: hsflash_save() only while audio is stopped (sfxring_stop) and
// no display flush is running (st7796_flush_busy() == false) - flash ops
// stall the QMI, which also serves PSRAM (framebuffers).
#ifndef HISCORE_FLASH_H
#define HISCORE_FLASH_H
#include "hiscore.h"

void hsflash_load(hs_table_t *t);          /* bad/blank flash -> empty table */
void hsflash_save(const hs_table_t *t);
#endif
