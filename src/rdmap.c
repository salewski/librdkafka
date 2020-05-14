/*
 * librdkafka - The Apache Kafka C/C++ library
 *
 * Copyright (c) 2020 Magnus Edenhill
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "rd.h"
#include "rdsysqueue.h"
#include "rdmap.h"

struct rd_map_s {
        LIST_HEAD(, rd_map_elem_s) *rmap_buckets; /**< Hash buckets */
        int rmap_bucket_cnt;                      /**< Bucket count */
        int rmap_cnt;                             /**< Element count */

        LIST_HEAD(, rd_map_elem_s) rmap_iter;  /**< Element list for iterating
                                                *   over all elements. */

        unsigned int (*rmap_hash) (const void *key);   /**< Key hash function */
        int (*rmap_cmp) (const void *a, const void *b); /**< Key comparator */
        void (*rmap_destroy_key) (void *key);     /**< Optional key free */
        void (*rmap_destroy_value) (void *value); /**< Optional value free */

        void *rmap_opaque;
};


static RD_INLINE
int rd_map_elem_cmp (const void *_a, const void *_b, const rd_map_t *rmap) {
        const rd_map_elem_t *a = _a, *b = _b;
        int r = a->hash - b->hash;
        if (r != 0)
                return r;
        return rmap->rmap_cmp(a->key, b->key);
}

static void rd_map_elem_destroy (rd_map_t *rmap, rd_map_elem_t *elem) {
        rd_assert(rmap->rmap_cnt > 0);
        rmap->rmap_cnt--;
        if (rmap->rmap_destroy_key)
                rmap->rmap_destroy_key((void *)elem->key);
        if (rmap->rmap_destroy_value)
                rmap->rmap_destroy_value((void *)elem->value);
        LIST_REMOVE(elem, hlink);
        LIST_REMOVE(elem, link);
        rd_free(elem);
}

static rd_map_elem_t *rd_map_find (rd_map_t *rmap, int *bktp,
                                   const rd_map_elem_t *skel) {
        int bkt = skel->hash % rmap->rmap_bucket_cnt;
        rd_map_elem_t *elem;

        if (bktp)
                *bktp = bkt;

        LIST_FOREACH(elem, &rmap->rmap_buckets[bkt], hlink) {
                if (!rd_map_elem_cmp(skel, elem, rmap))
                        return elem;
        }

        return NULL;
}

void rd_map_set (rd_map_t *rmap, void *key, void *value) {
        rd_map_elem_t skel = { .key = key,
                               .hash = rmap->rmap_hash(key) };
        rd_map_elem_t *elem;
        int bkt;

        if (!(elem = rd_map_find(rmap, &bkt, &skel))) {
                elem = rd_calloc(1, sizeof(*elem));
                elem->hash = skel.hash;
                elem->key = key; /* takes ownership of key */
                LIST_INSERT_HEAD(&rmap->rmap_buckets[bkt], elem, hlink);
                LIST_INSERT_HEAD(&rmap->rmap_iter, elem, link);
                rmap->rmap_cnt++;
        } else {
                if (elem->value && rmap->rmap_destroy_value)
                        rmap->rmap_destroy_value((void *)elem->value);
                if (rmap->rmap_destroy_key)
                        rmap->rmap_destroy_key(key);
        }

        elem->value = value; /* takes ownership of value */
}

const void *rd_map_get (rd_map_t *rmap, const void *key) {
        const rd_map_elem_t skel = { .key = (void *)key,
                                     .hash = rmap->rmap_hash(key) };
        rd_map_elem_t *elem;

        if (!(elem = rd_map_find(rmap, NULL, &skel)))
                return NULL;

        return elem->value;
}


void rd_map_delete (rd_map_t *rmap, const void *key) {
        const rd_map_elem_t skel = { .key = (void *)key,
                                     .hash = rmap->rmap_hash(key) };
        rd_map_elem_t *elem;
        int bkt;

        if (!(elem = rd_map_find(rmap, &bkt, &skel)))
                return;

        rd_map_elem_destroy(rmap, elem);
}

void rd_map_iter_begin (const rd_map_t *rmap, rd_map_elem_t **elem) {
        *elem = LIST_FIRST(&rmap->rmap_iter);
}

size_t rd_map_cnt (rd_map_t *rmap) {
        return (size_t)rmap->rmap_cnt;
}

void rd_map_init (rd_map_t *rmap, size_t expected_cnt,
                  int (*cmp) (const void *a, const void *b),
                  unsigned int (*hash) (const void *key),
                  void (*destroy_key) (void *key),
                  void (*destroy_value) (void *value)) {
        static const int max_depth = 15;
        static const int bucket_sizes[] = {
                5,
                11,
                23,
                47,
                97,
                199, /* default */
                409,
                823,
                1741,
                3469,
                6949,
                14033,
                28411,
                57557,
                116731,
                236897,
                -1
        };
        int i;

        memset(rmap, 0, sizeof(*rmap));

        if (!expected_cnt) {
                rmap->rmap_bucket_cnt = 199;
        } else {
                /* Strive for a maximum depth of 15 elements per bucket, but
                 * limit the maximum bucket size.
                 * When a real need arise we'll change this to a dynamically
                 * growing hash map instead, but this will do for now. */
                for (i = 0 ; bucket_sizes[i] != -1 &&
                             (int)expected_cnt / max_depth > bucket_sizes[i];
                     i++)
                        rmap->rmap_bucket_cnt = bucket_sizes[i];
        }

        rmap->rmap_buckets = rd_calloc(rmap->rmap_bucket_cnt,
                                       sizeof(*rmap->rmap_buckets));
        rd_assert(rmap->rmap_buckets != NULL);

        rmap->rmap_cmp = cmp;
        rmap->rmap_hash = hash;
        rmap->rmap_destroy_key = destroy_key;
        rmap->rmap_destroy_value = destroy_value;
}

void rd_map_destroy (rd_map_t *rmap) {
        rd_map_elem_t *elem;

        while ((elem = LIST_FIRST(&rmap->rmap_iter)))
                rd_map_elem_destroy(rmap, elem);


        rd_free(rmap->rmap_buckets);
}



/**
 * Unit tests
 *
 */
#include "rdtime.h"
#include "rdunittest.h"
#include "rdcrc32.h"

static int ut_rd_map_cmp_str (const void *a, const void *b) {
        return strcmp((const char *)a, (const char *)b);
}

static unsigned int ut_rd_map_hash_str (const void *key) {
        return (unsigned int)rd_crc32(key, strlen(key));
}

int unittest_map (void) {
        rd_map_t rmap;
        int pass, i, r;
        int cnt = 100000;
        int exp_cnt = 0, get_cnt = 0, iter_cnt = 0;
        rd_map_elem_t *elem;
        rd_ts_t ts = rd_clock();
        rd_ts_t ts_get;

        rd_map_init(&rmap, cnt,
                    ut_rd_map_cmp_str,
                    ut_rd_map_hash_str,
                    rd_free,
                    rd_free);

        /* pass 0 is set,delete,overwrite,get
         * pass 1-5 is get */
        for (pass = 0 ; pass < 6 ; pass++) {
                if (pass == 1)
                        ts_get = rd_clock();

                for (i = 1 ; i < cnt ; i++) {
                        char key[10];
                        char val[64];
                        const char *val2;
                        rd_bool_t do_delete = !(i % 13);
                        rd_bool_t overwrite = !do_delete && !(i % 5);

                        rd_snprintf(key, sizeof(key), "key%d", i);
                        rd_snprintf(val, sizeof(val), "VALUE=%d!", i);

                        if (pass == 0) {
                                rd_map_set(&rmap, rd_strdup(key),
                                           rd_strdup(val));

                                if (do_delete)
                                        rd_map_delete(&rmap, key);
                        }

                        if (overwrite) {
                                rd_snprintf(val, sizeof(val),
                                            "OVERWRITE=%d!", i);
                                if (pass == 0)
                                        rd_map_set(&rmap, rd_strdup(key),
                                                   rd_strdup(val));
                        }

                        val2 = rd_map_get(&rmap, key);

                        if (do_delete)
                                RD_UT_ASSERT(!val2, "map_get pass %d "
                                             "returned value %s "
                                             "for deleted key %s",
                                             pass, val2, key);
                        else
                                RD_UT_ASSERT(val2 && !strcmp(val, val2),
                                             "map_get pass %d: "
                                             "expected value %s, not %s, "
                                             "for key %s",
                                             pass, val,
                                             val2 ? val2 : "NULL", key);

                        if (pass == 0 && !do_delete)
                                exp_cnt++;
                }

                if (pass >= 1)
                        get_cnt += cnt;
        }

        ts_get = rd_clock() - ts_get;
        RD_UT_SAY("%d map_get iterations took %.3fms = %"PRId64"us/get",
                  get_cnt, (float)ts_get / 1000.0,
                  ts_get / get_cnt);

        RD_MAP_FOREACH(elem, &rmap) {
                iter_cnt++;
        }

        r = (int)rd_map_cnt(&rmap);
        RD_UT_ASSERT(r == exp_cnt,
                     "expected %d map entries, not %d", exp_cnt, r);

        RD_UT_ASSERT(r == iter_cnt,
                     "map_cnt() = %d, iteration gave %d elements", r, iter_cnt);

        rd_map_destroy(&rmap);

        ts = rd_clock() - ts;
        RD_UT_SAY("Total time over %d entries took %.3fms",
                  cnt, (float)ts / 1000.0);

        RD_UT_PASS();
}
