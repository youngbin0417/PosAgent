#ifndef POSAGENT_CHAT_H
#define POSAGENT_CHAT_H

#include "posagent.h"

#ifdef __cplusplus
extern "C" {
#endif

#define POSAGENT_CHAT_MAX_REQUEST_SIZE 32768
#define POSAGENT_CHAT_MAX_RESPONSE_SIZE 65536

typedef struct posagent_chat_adapter posagent_chat_adapter_t;

typedef struct {
    /* One of system, developer, user, assistant. Host owns both strings. */
    const char *role;
    const char *content;
} posagent_chat_message_t;

typedef posagent_status_t (*posagent_chat_transport_fn_t)(
    const char *endpoint_url, const char *api_key, const char *request_json,
    uint32_t timeout_ms, char *response_json, size_t response_capacity,
    void *user_data, posagent_result_t *result);

typedef struct {
    /* Borrowed strings and messages must remain valid throughout run_agent. */
    const char *endpoint_url;
    const char *api_key;
    const char *model;
    const posagent_chat_message_t *history;
    size_t history_count;
    uint32_t timeout_ms;
    /* NULL selects the Windows WinINet HTTPS transport. */
    posagent_chat_transport_fn_t transport;
    void *transport_user_data;
} posagent_chat_config_t;

posagent_status_t posagent_chat_adapter_create(const posagent_chat_config_t *config, posagent_chat_adapter_t **out);
posagent_status_t posagent_chat_adapter_destroy(posagent_chat_adapter_t *adapter);
posagent_status_t posagent_chat_model_callback(const posagent_agent_model_request_t *request, void *user_data,
                                               posagent_agent_response_t *response, posagent_result_t *result);

#ifdef __cplusplus
}
#endif

#endif
