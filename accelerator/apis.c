/*
 * Copyright (C) 2018-2019 BiiLabs Co., Ltd. and Contributors
 * All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the MIT license. A copy of the license can be found in the file
 * "LICENSE" at the root of this distribution.
 */

#include "apis.h"
#include "map/mode.h"
#include "utils/handles/lock.h"
#include "mam/api/api.h"

#define APIS_LOGGER "apis"

static logger_id_t apis_logger_id;
lock_handle_t cjson_lock;

void apis_logger_init() { apis_logger_id = logger_helper_enable(APIS_LOGGER, LOGGER_DEBUG, true); }

int apis_logger_release() {
  logger_helper_release(apis_logger_id);
  if (logger_helper_destroy() != RC_OK) {
    log_critical(apis_logger_id, "[%s:%d] Destroying logger failed %s.\n", __func__, __LINE__, APIS_LOGGER);
    return EXIT_FAILURE;
  }

  return 0;
}

double diff_time(struct timespec start, struct timespec end) {
  struct timespec diff;
  if (end.tv_nsec - start.tv_nsec < 0) {
    diff.tv_sec = end.tv_sec - start.tv_sec - 1;
    diff.tv_nsec = end.tv_nsec - start.tv_nsec + 1000000000;
  } else {
    diff.tv_sec = end.tv_sec - start.tv_sec;
    diff.tv_nsec = end.tv_nsec - start.tv_nsec;
  }
  return (diff.tv_sec + diff.tv_nsec / 1000000000.0);
}

void test_time_start(struct timespec* start) { clock_gettime(CLOCK_REALTIME, start); }

void test_time_end(struct timespec* start, struct timespec* end, double* sum) {
  clock_gettime(CLOCK_REALTIME, end);
  double difference = diff_time(*start, *end);
#if defined(ENABLE_STAT)
  printf("%lf\n", difference);
#endif
  *sum += difference;
}

status_t api_mam_send_message(const iota_config_t* const iconf, const iota_client_service_t* const service,
                              char const* const payload, char** json_result) {
  status_t ret = SC_OK;
  mam_api_t mam;
  const bool last_packet = true;
  bundle_transactions_t* bundle = NULL;
  bundle_transactions_new(&bundle);
  tryte_t channel_id[MAM_CHANNEL_ID_TRYTE_SIZE];
  tryte_t epid[MAM_CHANNEL_ID_TRYTE_SIZE];
  trit_t msg_id[MAM_MSG_ID_SIZE];
  ta_send_mam_req_t* req = send_mam_req_new();
  ta_send_mam_res_t* res = send_mam_res_new();

  // Creating MAM API
  if (mam_api_init(&mam, (tryte_t*)SEED) != RC_OK) {
      ret = SC_MAM_FAILED_INIT;
      log_error(apis_logger_id, "[%s:%d:%s]\n", __func__, __LINE__, "SC_MAM_FAILED_INIT");
      goto done;
  }

  if (map_channel_create(&mam, channel_id, 2)) {
      ret = SC_MAM_NULL;
      log_error(apis_logger_id, "[%s:%d:%s]\n", __func__, __LINE__, "SC_MAM_NULL");
      goto done;
    }
  // Creating channel
  
  double sum = 0;
  for (int i = 0; i < 15; i++) {
    struct timespec start_time, end_time;
    test_time_start(&start_time);
    if (mam_api_endpoint_create(&mam, i, channel_id, epid)) {
      ret = SC_MAM_NULL;
      log_error(apis_logger_id, "[%s:%d:%s]\n", __func__, __LINE__, "SC_MAM_NULL");
      goto done;
    }
    test_time_end(&start_time, &end_time, &sum);
    printf("depth = %d, time = %f \n", i, sum);
  }
  

done:
  // Destroying MAM API
  if (ret != SC_MAM_FAILED_INIT) {
    if (mam_api_save(&mam, iconf->mam_file_path, NULL, 0) != RC_OK) {
      log_error(apis_logger_id, "[%s:%d:%s]\n", __func__, __LINE__, "SC_MAM_FILE_SAVE");
    }
    if (mam_api_destroy(&mam)) {
      ret = SC_MAM_FAILED_DESTROYED;
      log_error(apis_logger_id, "[%s:%d:%s]\n", __func__, __LINE__, "SC_MAM_FAILED_DESTROYED");
    }
  }
  bundle_transactions_free(&bundle);
  send_mam_req_free(&req);
  send_mam_res_free(&res);
  return ret;
}
