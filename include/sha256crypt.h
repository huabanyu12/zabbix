/*
** Zabbix
** Copyright (C) 2001-2021 Zabbix SIA
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**/

#ifndef ZABBIX_SHA256CRYPT_H
#define ZABBIX_SHA256CRYPT_H

#include "common.h"

#define ZBX_SHA256_DIGEST_SIZE	32

void	zbx_sha256_hash(const char *in, char *out);
void*	zbx_sha256_hash_for_hmac(const void* data, const size_t datalen, void* out, const size_t outlen);

#endif /* ZABBIX_SHA256CRYPT_H */
