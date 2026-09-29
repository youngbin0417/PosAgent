#include "posagent.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int value;
    int tool_value;
    int branch_decision;
} demo_state_t;

static void node_start_fn(void *state, void *user_data, posagent_result_t *result) {
    demo_state_t *demo = (demo_state_t *)state;
    demo->value = 10;
    demo->branch_decision = 0;
    result->status = POSAGENT_OK;
    result->code = 0;
    snprintf(result->message, sizeof(result->message), "start node executed");
    (void)user_data;
}

static void node_tool_fn(void *state, void *user_data, posagent_result_t *result) {
    demo_state_t *demo = (demo_state_t *)state;
    demo->tool_value = demo->value + 5;
    result->status = POSAGENT_OK;
    result->code = 0;
    snprintf(result->message, sizeof(result->message), "tool value=%d", demo->tool_value);
    (void)user_data;
}

static void node_end_fn(void *state, void *user_data, posagent_result_t *result) {
    demo_state_t *demo = (demo_state_t *)state;
    demo->branch_decision = 1;
    result->status = POSAGENT_OK;
    result->code = 0;
    snprintf(result->message, sizeof(result->message), "end node reached: value=%d", demo->tool_value);
    (void)user_data;
}

static int route_decision_fn(void *state, void *user_data) {
    demo_state_t *demo = (demo_state_t *)state;
    (void)user_data;
    return demo->tool_value > 10;
}

static void trace_callback(const posagent_trace_event_t *event, void *user_data) {
    (void)user_data;
    printf("[TRACE] exec=%llu node=%d type=%d msg=%s\n",
           (unsigned long long)event->execution_id,
           event->node_id,
           event->type,
           event->message ? event->message : "");
}

static posagent_status_t demo_agent_tool(const char *arguments_json, char *output_json, size_t output_capacity, size_t *output_size, void *user_data, posagent_result_t *result) {
    if (strcmp(arguments_json, "{\"value\":7}") != 0) {
        result->status = POSAGENT_ERR_TOOL;
        snprintf(result->message, sizeof(result->message), "unexpected tool arguments");
        return POSAGENT_ERR_TOOL;
    }

    int written = snprintf(output_json, output_capacity, "{\"value\":14}");
    if (written < 0 || (size_t)written >= output_capacity) {
        result->status = POSAGENT_ERR_BUFFER_TOO_SMALL;
        snprintf(result->message, sizeof(result->message), "tool output buffer too small");
        return POSAGENT_ERR_BUFFER_TOO_SMALL;
    }

    *output_size = (size_t)written;
    result->status = POSAGENT_OK;
    result->code = 0;
    snprintf(result->message, sizeof(result->message), "value doubled");
    (void)user_data;
    return POSAGENT_OK;
}

static posagent_status_t demo_model_callback(const posagent_agent_model_request_t *request, void *user_data, posagent_agent_response_t *response, posagent_result_t *result) {
    (void)user_data;

    if (request->turn_index == 0 && request->tool_count == 1 &&
        strcmp(request->tools[0].name, "double_value") == 0) {
        response->action = POSAGENT_AGENT_TOOL_CALL;
        response->tool_name = "double_value";
        response->arguments_json = "{\"value\":7}";
        return POSAGENT_OK;
    }

    const posagent_agent_tool_result_t *last_tool_result = request->last_tool_result;
    if (request->turn_index == 1 && last_tool_result != NULL && last_tool_result->status == POSAGENT_OK &&
        strcmp(last_tool_result->name, "double_value") == 0 && strcmp(last_tool_result->output_json, "{\"value\":14}") == 0) {
        response->action = POSAGENT_AGENT_FINAL;
        response->final_text = "The doubled value is 14.";
        return POSAGENT_OK;
    }

    result->status = POSAGENT_ERR_INVALID_RESPONSE;
    snprintf(result->message, sizeof(result->message), "mock model did not receive the expected tool result");
    return POSAGENT_ERR_INVALID_RESPONSE;
}

#define CHECK_API(expression) \
    do { \
        status = (expression); \
        if (status != POSAGENT_OK) { \
            fprintf(stderr, "%s failed: %d\n", #expression, status); \
            goto cleanup; \
        } \
    } while (0)

int main(void) {
    posagent_graph_t *graph = NULL;
    posagent_context_t *ctx = NULL;
    demo_state_t state = {0};
    posagent_result_t result = {0};
    posagent_agent_tool_t tool = {
        "double_value",
        "double an integer value",
        "{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"integer\"}},\"required\":[\"value\"],\"additionalProperties\":false}",
        demo_agent_tool,
        NULL
    };
    posagent_status_t status = POSAGENT_OK;
    char final_response[128] = {0};
    int exit_code = 1;

    CHECK_API(posagent_graph_create(&graph));
    CHECK_API(posagent_graph_set_max_steps(graph, 8));
    CHECK_API(posagent_graph_add_node(graph, 1, "start", node_start_fn, NULL));
    CHECK_API(posagent_graph_add_node(graph, 2, "check", node_tool_fn, NULL));
    CHECK_API(posagent_graph_add_node(graph, 3, "end_true", node_end_fn, NULL));
    CHECK_API(posagent_graph_add_node(graph, 4, "end_false", node_end_fn, NULL));
    CHECK_API(posagent_graph_set_start_node(graph, 1));
    CHECK_API(posagent_graph_add_edge(graph, 1, 2));
    CHECK_API(posagent_graph_add_conditional_edge(graph, 2, 3, 4, route_decision_fn, NULL));
        CHECK_API(posagent_agent_tool_register(graph, &tool));
    CHECK_API(posagent_context_create(graph, &state, &ctx));
    CHECK_API(posagent_context_set_trace_callback(ctx, trace_callback, NULL));

    status = posagent_context_execute(ctx, &result);
    if (status != POSAGENT_OK) {
        fprintf(stderr, "graph execution failed: %d %s\n", status, result.message);
        goto cleanup;
    }

    printf("status=%d code=%d message=%s\n", status, result.code, result.message);
    printf("final state: value=%d tool_value=%d branch_decision=%d\n",
           state.value, state.tool_value, state.branch_decision);

        CHECK_API(posagent_context_run_agent(ctx, demo_model_callback, NULL, 3, final_response, sizeof(final_response), &result));
        printf("agent response: %s\n", final_response);
    exit_code = 0;

cleanup:
    posagent_context_destroy(ctx);
    posagent_graph_destroy(graph);
    return exit_code;
}

#undef CHECK_API
