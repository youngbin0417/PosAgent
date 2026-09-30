#include "posagent_chat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tool_calls = 0;

static posagent_status_t validation_tool(const char *arguments_json, char *output_json,
    size_t output_capacity, size_t *output_size, void *user_data, posagent_result_t *result) {
    const char output[] = "{\"marker\":\"POSAGENT_TOOL_OK_72\"}";
    (void)arguments_json;
    (void)user_data;
    if (sizeof(output) > output_capacity) {
        result->status = POSAGENT_ERR_BUFFER_TOO_SMALL;
        return result->status;
    }
    memcpy(output_json, output, sizeof(output));
    *output_size = sizeof(output) - 1;
    ++tool_calls;
    return POSAGENT_OK;
}

int main(int argc, char **argv) {
    const char *endpoint = getenv("POSAGENT_CHAT_ENDPOINT");
    const char *model = getenv("POSAGENT_CHAT_MODEL");
    const char *api_key = getenv("POSAGENT_CHAT_API_KEY");
    int tool_mode = argc == 2 && strcmp(argv[1], "--tool") == 0;
    if (endpoint == NULL || model == NULL || argc != 2) {
        fprintf(stderr, "Usage: set POSAGENT_CHAT_ENDPOINT and POSAGENT_CHAT_MODEL, then run with one prompt or --tool\n");
        return 2;
    }

    const char *prompt = tool_mode
        ? "Call get_validation_marker, then reply with the marker field from its result. Do not invent a marker."
        : argv[1];
    posagent_chat_message_t messages[] = { { "user", prompt } };
    posagent_chat_config_t config = { endpoint, api_key, model, messages, 1, 90000, NULL, NULL };
    posagent_chat_adapter_t *adapter = NULL;
    posagent_graph_t *graph = NULL;
    posagent_context_t *context = NULL;
    posagent_result_t result = {0};
    char final[8192];
    int exit_code = 1;
    posagent_agent_tool_t tool = {
        "get_validation_marker",
        "Return a marker that is available only in the tool result",
        "{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}",
        validation_tool,
        NULL
    };

    if (posagent_chat_adapter_create(&config, &adapter) != POSAGENT_OK ||
        posagent_graph_create(&graph) != POSAGENT_OK ||
        posagent_context_create(graph, NULL, &context) != POSAGENT_OK) {
        fprintf(stderr, "Could not initialize the chat adapter\n");
        goto cleanup;
    }
    if (tool_mode && posagent_agent_tool_register(graph, &tool) != POSAGENT_OK) {
        fprintf(stderr, "Could not register the validation tool\n");
        goto cleanup;
    }
    if (posagent_context_run_agent(context, posagent_chat_model_callback, adapter, 2,
        final, sizeof(final), &result) != POSAGENT_OK) {
        fprintf(stderr, "Chat request failed: status=%d code=%d %s\n", result.status, result.code, result.message);
        goto cleanup;
    }
    if (tool_mode && (tool_calls != 1 || strstr(final, "POSAGENT_TOOL_OK_72") == NULL)) {
        fprintf(stderr, "Chat tool round trip did not return the expected marker\n");
        goto cleanup;
    }
    puts(final);
    exit_code = 0;
cleanup:
    posagent_context_destroy(context);
    posagent_graph_destroy(graph);
    posagent_chat_adapter_destroy(adapter);
    return exit_code;
}
