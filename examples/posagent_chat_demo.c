#include "posagent_chat.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    const char *endpoint = getenv("POSAGENT_CHAT_ENDPOINT");
    const char *model = getenv("POSAGENT_CHAT_MODEL");
    const char *api_key = getenv("POSAGENT_CHAT_API_KEY");
    if (endpoint == NULL || model == NULL || argc != 2) {
        fprintf(stderr, "Usage: set POSAGENT_CHAT_ENDPOINT and POSAGENT_CHAT_MODEL, then run with one prompt\n");
        return 2;
    }

    posagent_chat_message_t messages[] = { { "user", argv[1] } };
    posagent_chat_config_t config = { endpoint, api_key, model, messages, 1, 30000, NULL, NULL };
    posagent_chat_adapter_t *adapter = NULL;
    posagent_graph_t *graph = NULL;
    posagent_context_t *context = NULL;
    posagent_result_t result = {0};
    char final[8192];
    int exit_code = 1;

    if (posagent_chat_adapter_create(&config, &adapter) != POSAGENT_OK ||
        posagent_graph_create(&graph) != POSAGENT_OK ||
        posagent_context_create(graph, NULL, &context) != POSAGENT_OK) {
        fprintf(stderr, "Could not initialize the chat adapter\n");
        goto cleanup;
    }
    if (posagent_context_run_agent(context, posagent_chat_model_callback, adapter, 2,
        final, sizeof(final), &result) != POSAGENT_OK) {
        fprintf(stderr, "Chat request failed: status=%d code=%d %s\n", result.status, result.code, result.message);
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
