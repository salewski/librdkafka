/*
 * librdkafka - The Apache Kafka C/C++ library
 *
 * Copyright (c) 2015 Magnus Edenhill
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
#ifndef _RDKAFKA_ASSIGNOR_H_
#define _RDKAFKA_ASSIGNOR_H_


typedef struct rd_kafka_group_member_s {
        rd_kafka_topic_partition_list_t *rkgm_subscription;
        rd_kafka_topic_partition_list_t *rkgm_assignment;
        rd_kafka_topic_partition_list_t *rkgm_owned;
        rd_list_t                        rkgm_eligible;
        rd_kafkap_str_t                 *rkgm_member_id;
        rd_kafkap_str_t                 *rkgm_group_instance_id;
        rd_kafkap_bytes_t               *rkgm_userdata;
        rd_kafkap_bytes_t               *rkgm_member_metadata;
} rd_kafka_group_member_t;


int rd_kafka_group_member_cmp (const void *_a, const void *_b);

int
rd_kafka_group_member_find_subscription (rd_kafka_t *rk,
					 const rd_kafka_group_member_t *rkgm,
					 const char *topic);


/**
 * Structure to hold metadata for a single topic and all its
 * subscribing members.
 */
typedef struct rd_kafka_assignor_topic_s {
        const rd_kafka_metadata_topic_t *metadata;
        rd_list_t members;     /* rd_kafka_group_member_t * */
} rd_kafka_assignor_topic_t;


int rd_kafka_assignor_topic_cmp (const void *_a, const void *_b);


typedef struct rd_kafka_assignor_s {
        rd_kafkap_str_t   *rkas_protocol_type;
        rd_kafkap_str_t   *rkas_protocol_name;

	int                rkas_enabled;

        rd_kafka_resp_err_t (*rkas_assign_cb) (
                struct rd_kafka_assignor_s *rkas,
                rd_kafka_t *rk,
                const char *member_id,
                const rd_kafka_metadata_t *metadata,
                rd_kafka_group_member_t *members,
                size_t member_cnt,
                rd_kafka_assignor_topic_t **eligible_topics,
                size_t eligible_topic_cnt,
                char *errstr,
                size_t errstr_size,
                void *opaque);

        rd_kafkap_bytes_t *(*rkas_get_metadata_cb) (
                struct rd_kafka_assignor_s *rkas,
                void *assignor_state,
                const rd_list_t *topics,
                const rd_kafka_topic_partition_list_t *owned_partitions);

        void (*rkas_on_assignment_cb) (
                const struct rd_kafka_assignor_s *rkas,
                void **assignor_state,
                const rd_kafka_topic_partition_list_t *assignment,
                const rd_kafkap_bytes_t *assignment_userdata,
                const rd_kafka_consumer_group_metadata_t *rkcgm);

        void (*rkas_destroy_state) (void *assignor_state);

        void *rkas_opaque;
} rd_kafka_assignor_t;


rd_kafkap_bytes_t *
rd_kafka_consumer_protocol_member_metadata_new (const rd_list_t *topics,
                                                const void *userdata,
                                                size_t userdata_size,
                                                const rd_kafka_topic_partition_list_t
                                                *owned_partitions);

rd_kafkap_bytes_t *
rd_kafka_assignor_get_metadata_with_empty_userdata (rd_kafka_assignor_t *rkas,
                                                    void *assignor_state,
                                                    const rd_list_t *topics,
                                                    const rd_kafka_topic_partition_list_t
                                                    *owned_partitions);


void rd_kafka_assignor_update_subscription (rd_kafka_assignor_t *rkpas,
                                            const rd_kafka_topic_partition_list_t
                                            *subscription);


rd_kafka_resp_err_t
rd_kafka_assignor_run (struct rd_kafka_cgrp_s *rkcg,
                       rd_kafka_assignor_t *rkas,
                       rd_kafka_metadata_t *metadata,
                       rd_kafka_group_member_t *members, int member_cnt,
                       char *errstr, size_t errstr_size);

rd_kafka_assignor_t *
rd_kafka_assignor_find (rd_kafka_t *rk, const char *protocol);

int rd_kafka_assignors_init (rd_kafka_t *rk, char *errstr, size_t errstr_size);
void rd_kafka_assignors_term (rd_kafka_t *rk);



void rd_kafka_group_member_clear (rd_kafka_group_member_t *rkgm);


/**
 * rd_kafka_range_assignor.c
 */
rd_kafka_resp_err_t
rd_kafka_range_assignor_assign_cb (rd_kafka_assignor_t *rkas,
                                   rd_kafka_t *rk,
                                   const char *member_id,
                                   const rd_kafka_metadata_t *metadata,
                                   rd_kafka_group_member_t *members,
                                   size_t member_cnt,
                                   rd_kafka_assignor_topic_t **eligible_topics,
                                   size_t eligible_topic_cnt,
                                   char *errstr, size_t errstr_size,
                                   void *opaque);


/**
 * rd_kafka_roundrobin_assignor.c
 */
rd_kafka_resp_err_t
rd_kafka_roundrobin_assignor_assign_cb (rd_kafka_assignor_t *rkas,
                                        rd_kafka_t *rk,
					const char *member_id,
					const rd_kafka_metadata_t *metadata,
					rd_kafka_group_member_t *members,
					size_t member_cnt,
					rd_kafka_assignor_topic_t
					**eligible_topics,
					size_t eligible_topic_cnt,
					char *errstr, size_t errstr_size,
					void *opaque);

/**
 * rd_kafka_sticky_assignor.c
 */
rd_kafka_resp_err_t
rd_kafka_sticky_assignor_assign_cb (rd_kafka_assignor_t *rkas,
                                    rd_kafka_t *rk,
                                    const char *member_id,
                                    const rd_kafka_metadata_t *metadata,
                                    rd_kafka_group_member_t *members,
                                    size_t member_cnt,
                                    rd_kafka_assignor_topic_t
                                    **eligible_topics,
                                    size_t eligible_topic_cnt,
                                    char *errstr, size_t errstr_size,
                                    void *opaque);

void rd_kafka_sticky_assignor_on_assignment_cb (
                const rd_kafka_assignor_t *rkas,
                void **assignor_state,
                const rd_kafka_topic_partition_list_t *partitions,
                const rd_kafkap_bytes_t *userdata,
                const rd_kafka_consumer_group_metadata_t *rkcgm);

rd_kafkap_bytes_t *
rd_kafka_sticky_assignor_get_metadata (rd_kafka_assignor_t *rkas,
                                       void *assignor_state,
                                       const rd_list_t *topics,
                                       const rd_kafka_topic_partition_list_t
                                       *owned_partitions);

void rd_kafka_sticky_assignor_state_destroy (void *assignor_state);

#endif /* _RDKAFKA_ASSIGNOR_H_ */
