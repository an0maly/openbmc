/*
 * Copyright 2015-present Facebook. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __EDB_H__
#define __EDB_H__

#ifdef __cplusplus
extern "C" {
#endif

#define EDB_CACHE_PATH "/tmp/cache.db"
#define EDB_FLASH_PATH "/mnt/data/flash.db"

int edb_cache_get(char* key, char *value);
int edb_cache_set(char* key, char *value);
int edb_flash_get(char* key, char *value);
int edb_flash_set(char* key, char *value);

#ifdef __cplusplus
}
#endif

#endif /* __EDB_H__ */
