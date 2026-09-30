#include "posagent_chat.h"
#include "../src/posagent_json.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); return 1; \
} } while (0)

typedef struct { int calls; int tool_calls; int mode; } fixture_t;

static posagent_status_t double_tool(const char *arguments, char *output, size_t capacity,
    size_t *size, void *user_data, posagent_result_t *result) {
    fixture_t *fixture = (fixture_t *)user_data;
    ++fixture->tool_calls;
    if (strcmp(arguments, "{\"value\":7}") != 0) return POSAGENT_ERR_TOOL;
    int written = snprintf(output, capacity, "{\"value\":14}");
    if (written < 0 || (size_t)written >= capacity) return POSAGENT_ERR_BUFFER_TOO_SMALL;
    *size = (size_t)written;
    result->status = POSAGENT_OK;
    return POSAGENT_OK;
}

static posagent_status_t mock_transport(const char *endpoint, const char *api_key, const char *body,
    uint32_t timeout_ms, char *output, size_t capacity, void *user_data, posagent_result_t *result) {
    fixture_t *fixture = (fixture_t *)user_data;
    ++fixture->calls;
    if (strcmp(endpoint, "https://example.test/v1/chat/completions") != 0 ||
        strcmp(api_key, "test-key") != 0 || timeout_ms != 5000) return POSAGENT_ERR_INTERNAL;
    posagent_json_doc_t *doc = posagent_json_parse(body, POSAGENT_CHAT_MAX_REQUEST_SIZE);
    if (doc == NULL) return POSAGENT_ERR_INVALID_RESPONSE;
    char value[128];
    int model = posagent_json_object_get(doc, 0, "model");
    int messages = posagent_json_object_get(doc, 0, "messages");
    int tools = posagent_json_object_get(doc, 0, "tools");
    int parallel = posagent_json_object_get(doc, 0, "parallel_tool_calls");
    int valid = posagent_json_copy_string(doc, model, value, sizeof(value)) && strcmp(value, "test-model") == 0 &&
        posagent_json_array_count(doc, tools) == 1 && posagent_json_copy_raw(doc, parallel, value, sizeof(value)) &&
        strcmp(value, "false") == 0;
    if (fixture->calls == 1) {
        int last = posagent_json_array_item(doc, messages, 1);
        valid = valid && posagent_json_array_count(doc, messages) == 2 &&
            posagent_json_copy_string(doc, posagent_json_object_get(doc, last, "content"), value, sizeof(value)) &&
            strcmp(value, "Double seven") == 0;
    } else if (fixture->calls == 2) {
        int assistant = posagent_json_array_item(doc, messages, 2);
        int tool = posagent_json_array_item(doc, messages, 3);
        int calls = posagent_json_object_get(doc, assistant, "tool_calls");
        int call = posagent_json_array_item(doc, calls, 0);
        valid = valid && posagent_json_array_count(doc, messages) == 4 &&
            posagent_json_copy_string(doc, posagent_json_object_get(doc, call, "id"), value, sizeof(value)) &&
            strcmp(value, "call_1") == 0 &&
            posagent_json_copy_string(doc, posagent_json_object_get(doc, tool, "tool_call_id"), value, sizeof(value)) &&
            strcmp(value, "call_1") == 0 &&
            posagent_json_copy_string(doc, posagent_json_object_get(doc, tool, "content"), value, sizeof(value)) &&
            strcmp(value, "{\"value\":14}") == 0;
    }
    posagent_json_free(doc);
    if (!valid) return POSAGENT_ERR_INVALID_RESPONSE;
    if (fixture->mode == 2) {
        result->status = POSAGENT_ERR_TIMEOUT;
        snprintf(result->message, sizeof(result->message), "mock transport timeout");
        return POSAGENT_ERR_TIMEOUT;
    }
    const char *reply = (fixture->calls == 1 && fixture->mode != 3) || fixture->mode == 4 ?
        "{\"choices\":[{\"finish_reason\":\"tool_calls\",\"message\":{\"role\":\"assistant\",\"content\":null,\"tool_calls\":[{\"id\":\"call_1\",\"type\":\"function\",\"function\":{\"name\":\"double\",\"arguments\":\"{\\\"value\\\":7}\"}}]}}]}" :
        "{\"choices\":[{\"finish_reason\":\"stop\",\"message\":{\"role\":\"assistant\",\"content\":\"The doubled value is 14.\"}}]}";
    if (fixture->mode == 1) reply = "{\"choices\":[{\"finish_reason\":\"tool_calls\",\"message\":{\"role\":\"assistant\",\"tool_calls\":[{},{}]}}]}";
    if (fixture->mode == 5) reply = "{\"choices\":[{\"finish_reason\":\"length\",\"message\":{\"role\":\"assistant\",\"content\":\"partial\"}}]}";
    if (strlen(reply) >= capacity) return POSAGENT_ERR_BUFFER_TOO_SMALL;
    strcpy(output, reply);
    return POSAGENT_OK;
}

static int run_case(int mode, posagent_status_t expected, int expected_transport_calls, int expected_tool_calls) {
    posagent_chat_message_t history[] = { { "system", "Use available tools" }, { "user", "Double seven" } };
    fixture_t fixture = {0, 0, mode};
    posagent_chat_config_t config = {
        "https://example.test/v1/chat/completions", "test-key", "test-model", history, 2,
        5000, mock_transport, &fixture
    };
    posagent_chat_adapter_t *adapter = NULL;
    posagent_graph_t *graph = NULL;
    posagent_context_t *context = NULL;
    posagent_result_t result = {0};
    char final[128] = {0};
    posagent_agent_tool_t tool = {
        "double", "double an integer", "{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"integer\"}},\"required\":[\"value\"],\"additionalProperties\":false}",
        double_tool, &fixture
    };
    CHECK(posagent_chat_adapter_create(&config, &adapter) == POSAGENT_OK);
    CHECK(posagent_graph_create(&graph) == POSAGENT_OK);
    CHECK(posagent_agent_tool_register(graph, &tool) == POSAGENT_OK);
    CHECK(posagent_context_create(graph, NULL, &context) == POSAGENT_OK);
    CHECK(posagent_context_run_agent(context, posagent_chat_model_callback, adapter, 3,
        final, sizeof(final), &result) == expected);
    CHECK(result.status == expected);
    CHECK(fixture.calls == expected_transport_calls);
    CHECK(fixture.tool_calls == expected_tool_calls);
    if (expected == POSAGENT_OK) CHECK(strcmp(final, "The doubled value is 14.") == 0);
    posagent_context_destroy(context);
    posagent_graph_destroy(graph);
    posagent_chat_adapter_destroy(adapter);
    return 0;
}

static int test_history_contract(void) {
    posagent_chat_message_t messages[] = { { "assistant", "old answer" } };
    posagent_chat_config_t config = {
        "https://example.test/v1/chat/completions", NULL, "test-model", messages, 1,
        0, mock_transport, NULL
    };
    posagent_chat_adapter_t *adapter = NULL;
    CHECK(posagent_chat_adapter_create(&config, &adapter) == POSAGENT_ERR_INVALID_ARG);
    CHECK(adapter == NULL);
    return 0;
}

int main(void) {
    CHECK(test_history_contract() == 0);
    CHECK(run_case(0, POSAGENT_OK, 2, 1) == 0);
    CHECK(run_case(1, POSAGENT_ERR_INVALID_RESPONSE, 1, 0) == 0);
    CHECK(run_case(2, POSAGENT_ERR_TIMEOUT, 1, 0) == 0);
    CHECK(run_case(3, POSAGENT_OK, 1, 0) == 0);
    CHECK(run_case(4, POSAGENT_ERR_INVALID_RESPONSE, 2, 1) == 0);
    CHECK(run_case(5, POSAGENT_ERR_INVALID_RESPONSE, 1, 0) == 0);
    puts("All PosAgent Chat adapter tests passed");
    return 0;
}

#undef CHECK
