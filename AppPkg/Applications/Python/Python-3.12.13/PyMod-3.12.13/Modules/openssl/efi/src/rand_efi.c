/*
 * Copyright 1995-2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stddef.h>

#include <crypto/rand.h>

size_t OPENSSL_ia32_rdseed_bytes(unsigned char *buf, size_t len);

int rand_pool_init(void)
{
    return 1;
}

void rand_pool_cleanup(void)
{
}

void rand_pool_keep_random_devices_open(int keep)
{
}

size_t rand_pool_acquire_entropy(RAND_POOL *pool)
{
  uint8_t buffer[256] = {0};

  OPENSSL_ia32_rdseed_bytes(buffer, sizeof(buffer));
  return rand_pool_add(pool, (unsigned char *)&buffer, sizeof(buffer), 0);
}

int rand_pool_add_nonce_data(RAND_POOL *pool)
{
  uint8_t buffer[256] = {0};

  OPENSSL_ia32_rdseed_bytes(buffer, sizeof(buffer));
  return rand_pool_add(pool, (unsigned char *)&buffer, sizeof(buffer), 0);
}

int rand_pool_add_additional_data(RAND_POOL *pool)
{
  uint8_t buffer[256] = {0};

  OPENSSL_ia32_rdseed_bytes(buffer, sizeof(buffer));
  return rand_pool_add(pool, (unsigned char *)&buffer, sizeof(buffer), 0);
}
