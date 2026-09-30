#include "posagent_chat.h"
#include "posagent_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHAT_MAX_FINAL 8192
#define CHAT_MAX_NAME 128
#define CHAT_MAX_CALL_ID 128

posagent_status_t posagent_chat_default_transport(const char *, const char *, const char *, uint32_t,
                                                   char *, size_t, void *, posagent_result_t *);

struct posagent_chat_adapter {
    posagent_chat_config_t config;
    char request_json[POSAGENT_CHAT_MAX_REQUEST_SIZE];
    char response_json[POSAGENT_CHAT_MAX_RESPONSE_SIZE];
    char final_text[CHAT_MAX_FINAL];
    char tool_name[CHAT_MAX_NAME];
    char arguments_json[POSAGENT_MAX_TOOL_JSON_SIZE];
    char call_id[CHAT_MAX_CALL_ID];
    int has_tool_call;
};

typedef struct {
    char *buffer;
    size_t capacity;
    size_t used;
    int ok;
} json_writer_t;

static void put_text(json_writer_t *writer, const char *value) {
    size_t length = strlen(value);
    if (!writer->ok || length >= writer->capacity - writer->used) { writer->ok = 0; return; }
    memcpy(writer->buffer + writer->used, value, length);
    writer->used += length;
    writer->buffer[writer->used] = '\0';
}

static void put_char(json_writer_t *writer, char value) {
    if (!writer->ok || writer->used + 1 >= writer->capacity) { writer->ok = 0; return; }
    writer->buffer[writer->used++] = value;
    writer->buffer[writer->used] = '\0';
}

static void put_quoted(json_writer_t *writer, const char *value) {
    static const char hex[] = "0123456789abcdef";
    put_char(writer, '"');
    for (const unsigned char *p = (const unsigned char *)value; *p && writer->ok; ++p) {
        if (*p == '"' || *p == '\\') { put_char(writer, '\\'); put_char(writer, (char)*p); }
        else if (*p < 0x20) {
            put_text(writer, "\\u00");
            put_char(writer, hex[*p >> 4]);
            put_char(writer, hex[*p & 15]);
        } else put_char(writer, (char)*p);
    }
    put_char(writer, '"');
}

static posagent_status_t fail(posagent_result_t *result, posagent_status_t status, const char *message) {
    result->status = status;
    snprintf(result->message, sizeof(result->message), "%s", message);
    return status;
}

static int valid_history(const posagent_chat_config_t *config) {
    if (config->history_count == 0) return 0;
    if (config->history_count > 64 || config->history == NULL) return 0;
    for (size_t i = 0; i < config->history_count; ++i) {
        const posagent_chat_message_t *message = &config->history[i];
        if (message->role == NULL || message->content == NULL) return 0;
        if (strcmp(message->role, "system") != 0 && strcmp(message->role, "developer") != 0 &&
            strcmp(message->role, "user") != 0 && strcmp(message->role, "assistant") != 0) return 0;
    }
    return strcmp(config->history[config->history_count - 1].role, "user") == 0;
}

posagent_status_t posagent_chat_adapter_create(const posagent_chat_config_t *config, posagent_chat_adapter_t **out) {
    if (out != NULL) *out = NULL;
    if (config == NULL || out == NULL || config->endpoint_url == NULL || config->endpoint_url[0] == '\0' ||
        config->model == NULL || config->model[0] == '\0' || !valid_history(config)) return POSAGENT_ERR_INVALID_ARG;
    posagent_chat_adapter_t *adapter = (posagent_chat_adapter_t *)calloc(1, sizeof(*adapter));
    if (adapter == NULL) return POSAGENT_ERR_INTERNAL;
    adapter->config = *config;
    if (adapter->config.timeout_ms == 0) adapter->config.timeout_ms = 30000;
    if (adapter->config.transport == NULL) adapter->config.transport = posagent_chat_default_transport;
    *out = adapter;
    return POSAGENT_OK;
}

posagent_status_t posagent_chat_adapter_destroy(posagent_chat_adapter_t *adapter) {
    free(adapter);
    return POSAGENT_OK;
}

static int build_request(posagent_chat_adapter_t *adapter, const posagent_agent_model_request_t *request) {
    json_writer_t writer = { adapter->request_json, sizeof(adapter->request_json), 0, 1 };
    writer.buffer[0] = '\0';
    put_text(&writer, "{\"model\":");
    put_quoted(&writer, adapter->config.model);
    put_text(&writer, ",\"stream\":false,\"messages\":[");
    for (size_t i = 0; i < adapter->config.history_count; ++i) {
        const posagent_chat_message_t *message = &adapter->config.history[i];
        if (i != 0) put_char(&writer, ',');
        put_text(&writer, "{\"role\":"); put_quoted(&writer, message->role);
        put_text(&writer, ",\"content\":"); put_quoted(&writer, message->content);
        put_char(&writer, '}');
    }
    if (request->turn_index == 1) {
        if (!adapter->has_tool_call || request->last_tool_result == NULL ||
            request->last_tool_result->name == NULL || request->last_tool_result->output_json == NULL ||
            strcmp(request->last_tool_result->name, adapter->tool_name) != 0) return 0;
        put_text(&writer, ",{\"role\":\"assistant\",\"content\":null,\"tool_calls\":[{\"id\":");
        put_quoted(&writer, adapter->call_id);
        put_text(&writer, ",\"type\":\"function\",\"function\":{\"name\":");
        put_quoted(&writer, adapter->tool_name);
        put_text(&writer, ",\"arguments\":");
        put_quoted(&writer, adapter->arguments_json);
        put_text(&writer, "}}]},{\"role\":\"tool\",\"tool_call_id\":");
        put_quoted(&writer, adapter->call_id);
        put_text(&writer, ",\"content\":");
        put_quoted(&writer, request->last_tool_result->output_json);
        put_char(&writer, '}');
    }
    put_char(&writer, ']');
    if (request->tool_count != 0) {
        put_text(&writer, ",\"parallel_tool_calls\":false,\"tools\":[");
        for (size_t i = 0; i < request->tool_count; ++i) {
            const posagent_agent_tool_info_t *tool = &request->tools[i];
            if (tool->name == NULL || tool->arguments_schema_json == NULL) return 0;
            if (i != 0) put_char(&writer, ',');
            put_text(&writer, "{\"type\":\"function\",\"function\":{\"name\":");
            put_quoted(&writer, tool->name);
            put_text(&writer, ",\"description\":");
            put_quoted(&writer, tool->description != NULL ? tool->description : "");
            put_text(&writer, ",\"parameters\":");
            put_text(&writer, tool->arguments_schema_json);
            put_text(&writer, "}}");
        }
        put_char(&writer, ']');
    }
    put_char(&writer, '}');
    return writer.ok;
}

static int read_string(const posagent_json_doc_t *doc, int object, const char *key, char *out, size_t capacity) {
    return posagent_json_copy_string(doc, posagent_json_object_get(doc, object, key), out, capacity);
}

static posagent_status_t parse_response(posagent_chat_adapter_t *adapter, posagent_agent_response_t *response,
                                        posagent_result_t *result, uint32_t turn) {
    posagent_json_doc_t *doc = posagent_json_parse(adapter->response_json, sizeof(adapter->response_json));
    if (doc == NULL) return fail(result, POSAGENT_ERR_INVALID_RESPONSE, "provider returned invalid JSON");
    int choices = posagent_json_object_get(doc, 0, "choices");
    int choice = posagent_json_array_item(doc, choices, 0);
    int message = posagent_json_object_get(doc, choice, "message");
    int calls = posagent_json_object_get(doc, message, "tool_calls");
    char role[16], finish[32];
    if (!posagent_json_is_object(doc, 0) || posagent_json_array_count(doc, choices) != 1 ||
        !posagent_json_is_object(doc, choice) || !posagent_json_is_object(doc, message) ||
        !read_string(doc, message, "role", role, sizeof(role)) || strcmp(role, "assistant") != 0 ||
        !read_string(doc, choice, "finish_reason", finish, sizeof(finish))) {
        posagent_json_free(doc);
        return fail(result, POSAGENT_ERR_INVALID_RESPONSE, "provider response has no single message");
    }
    int count = posagent_json_array_count(doc, calls);
    if (count > 0) {
        int call = posagent_json_array_item(doc, calls, 0);
        int function = posagent_json_object_get(doc, call, "function");
        char type[16];
        if (count != 1 || turn != 0 || strcmp(finish, "tool_calls") != 0 || !posagent_json_is_object(doc, call) ||
            !read_string(doc, call, "id", adapter->call_id, sizeof(adapter->call_id)) ||
            !read_string(doc, call, "type", type, sizeof(type)) || strcmp(type, "function") != 0 ||
            !posagent_json_is_object(doc, function) ||
            !read_string(doc, function, "name", adapter->tool_name, sizeof(adapter->tool_name)) ||
            !read_string(doc, function, "arguments", adapter->arguments_json, sizeof(adapter->arguments_json)) ||
            adapter->call_id[0] == '\0' || adapter->tool_name[0] == '\0') {
            posagent_json_free(doc);
            return fail(result, POSAGENT_ERR_INVALID_RESPONSE, "provider returned unsupported tool calls");
        }
        adapter->has_tool_call = 1;
        response->action = POSAGENT_AGENT_TOOL_CALL;
        response->tool_name = adapter->tool_name;
        response->arguments_json = adapter->arguments_json;
    } else {
        if ((calls >= 0 && !posagent_json_is_null(doc, calls) && !posagent_json_is_array(doc, calls)) ||
            strcmp(finish, "stop") != 0 ||
            !read_string(doc, message, "content", adapter->final_text, sizeof(adapter->final_text))) {
            posagent_json_free(doc);
            return fail(result, POSAGENT_ERR_INVALID_RESPONSE, "provider response has no text content");
        }
        response->action = POSAGENT_AGENT_FINAL;
        response->final_text = adapter->final_text;
    }
    posagent_json_free(doc);
    return POSAGENT_OK;
}

posagent_status_t posagent_chat_model_callback(const posagent_agent_model_request_t *request, void *user_data,
                                               posagent_agent_response_t *response, posagent_result_t *result) {
    posagent_chat_adapter_t *adapter = (posagent_chat_adapter_t *)user_data;
    if (request == NULL || adapter == NULL || response == NULL || result == NULL) return POSAGENT_ERR_INVALID_ARG;
    memset(response, 0, sizeof(*response));
    memset(result, 0, sizeof(*result));
    if (request->turn_index > 1 || (request->turn_index == 0 && adapter->has_tool_call))
        return fail(result, POSAGENT_ERR_INVALID_RESPONSE, "adapter supports one tool call per run");
    if (!build_request(adapter, request)) return fail(result, POSAGENT_ERR_BUFFER_TOO_SMALL, "chat request exceeds configured limit");
    adapter->response_json[0] = '\0';
    posagent_status_t status = adapter->config.transport(adapter->config.endpoint_url, adapter->config.api_key,
        adapter->request_json, adapter->config.timeout_ms, adapter->response_json,
        sizeof(adapter->response_json), adapter->config.transport_user_data, result);
    if (status != POSAGENT_OK) {
        result->status = status;
        if (result->message[0] == '\0') snprintf(result->message, sizeof(result->message), "chat transport failed");
        return status;
    }
    if (memchr(adapter->response_json, '\0', sizeof(adapter->response_json)) == NULL)
        return fail(result, POSAGENT_ERR_BUFFER_TOO_SMALL, "provider response exceeds configured limit");
    return parse_response(adapter, response, result, request->turn_index);
}
