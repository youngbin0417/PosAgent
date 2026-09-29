#include "posagent.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return 1; \
        } \
    } while (0)

typedef struct {
    int value;
    int selected_path;
} test_state_t;

static void start_node(void *state, void *user_data, posagent_result_t *result) {
    test_state_t *test_state = (test_state_t *)state;
    test_state->value = *(const int *)user_data;
    result->status = POSAGENT_OK;
    result->code = 0;
    (void)snprintf(result->message, sizeof(result->message), "start");
}

static void decision_node(void *state, void *user_data, posagent_result_t *result) {
    (void)state;
    (void)user_data;
    result->status = POSAGENT_OK;
    result->code = 0;
    (void)snprintf(result->message, sizeof(result->message), "decision");
}

static void true_node(void *state, void *user_data, posagent_result_t *result) {
    test_state_t *test_state = (test_state_t *)state;
    test_state->selected_path = 1;
    result->status = POSAGENT_OK;
    result->code = 0;
    (void)snprintf(result->message, sizeof(result->message), "true");
    (void)user_data;
}

static void false_node(void *state, void *user_data, posagent_result_t *result) {
    test_state_t *test_state = (test_state_t *)state;
    test_state->selected_path = 2;
    result->status = POSAGENT_OK;
    result->code = 0;
    (void)snprintf(result->message, sizeof(result->message), "false");
    (void)user_data;
}

static int route(void *state, void *user_data) {
    const test_state_t *test_state = (const test_state_t *)state;
    (void)user_data;
    return test_state->value > 10;
}

static int run_branch_case(int input, int expected_path) {
    posagent_graph_t *graph = NULL;
    posagent_context_t *context = NULL;
    posagent_result_t result = {0};
    test_state_t state = {0};
    posagent_status_t status;

    status = posagent_graph_create(&graph);
    CHECK(status == POSAGENT_OK);
    CHECK(posagent_graph_add_node(graph, 1, "start", start_node, &input) == POSAGENT_OK);
    CHECK(posagent_graph_add_node(graph, 2, "decision", decision_node, NULL) == POSAGENT_OK);
    CHECK(posagent_graph_add_node(graph, 3, "true", true_node, NULL) == POSAGENT_OK);
    CHECK(posagent_graph_add_node(graph, 4, "false", false_node, NULL) == POSAGENT_OK);
    CHECK(posagent_graph_add_edge(graph, 1, 2) == POSAGENT_OK);
    CHECK(posagent_graph_add_conditional_edge(graph, 2, 3, 4, route, NULL) == POSAGENT_OK);
    CHECK(posagent_graph_set_start_node(graph, 1) == POSAGENT_OK);
    CHECK(posagent_context_create(graph, &state, &context) == POSAGENT_OK);

    status = posagent_context_execute(context, &result);
    CHECK(status == POSAGENT_OK);
    CHECK(result.status == POSAGENT_OK);
    CHECK(state.value == input);
    CHECK(state.selected_path == expected_path);

    posagent_context_destroy(context);
    posagent_graph_destroy(graph);
    return 0;
}

static void double_tool(const void *input, void *output, void *user_data, posagent_result_t *result) {
    *(int *)output = *(const int *)input * 2;
    result->status = POSAGENT_OK;
    result->code = 0;
    (void)snprintf(result->message, sizeof(result->message), "doubled");
    (void)user_data;
}

static int test_standalone_tool_callback(void) {
    posagent_tool_t tool = { 1, "double", "double integer", double_tool, NULL, sizeof(int), sizeof(int) };
    posagent_result_t result = {0};
    int input = 9;
    int output = 0;

    CHECK(posagent_tool_invoke(&tool, &input, &output, &result) == POSAGENT_OK);
    CHECK(output == 18);
    return 0;
}

typedef struct {
    const char *requested_tool;
    const char *arguments_json;
    int calls;
    int received_tool_result;
    int received_tool_spec;
} mock_model_state_t;

typedef struct {
    int calls;
} mock_tool_state_t;

static posagent_status_t mock_json_tool(const char *arguments_json, char *output_json, size_t output_capacity, size_t *output_size, void *user_data, posagent_result_t *result) {
    mock_tool_state_t *tool_state = (mock_tool_state_t *)user_data;
    ++tool_state->calls;

    if (strcmp(arguments_json, "{\"value\":9}") != 0) {
        result->status = POSAGENT_ERR_TOOL;
        (void)snprintf(result->message, sizeof(result->message), "invalid value argument");
        return POSAGENT_ERR_TOOL;
    }

    int written = snprintf(output_json, output_capacity, "{\"value\":18}");
    if (written < 0 || (size_t)written >= output_capacity) {
        result->status = POSAGENT_ERR_BUFFER_TOO_SMALL;
        (void)snprintf(result->message, sizeof(result->message), "tool output buffer too small");
        return POSAGENT_ERR_BUFFER_TOO_SMALL;
    }

    *output_size = (size_t)written;
    result->status = POSAGENT_OK;
    result->code = 0;
    (void)snprintf(result->message, sizeof(result->message), "value doubled");
    return POSAGENT_OK;
}

static posagent_status_t mock_model_callback(const posagent_agent_model_request_t *request, void *user_data, posagent_agent_response_t *response, posagent_result_t *result) {
    mock_model_state_t *model_state = (mock_model_state_t *)user_data;
    ++model_state->calls;

    if (request->turn_index == 0 && request->tool_count == 1 &&
        strcmp(request->tools[0].name, "double") == 0 && request->tools[0].arguments_schema_json != NULL) {
        model_state->received_tool_spec = 1;
        response->action = POSAGENT_AGENT_TOOL_CALL;
        response->tool_name = model_state->requested_tool;
        response->arguments_json = model_state->arguments_json;
        return POSAGENT_OK;
    }

    const posagent_agent_tool_result_t *last_tool_result = request->last_tool_result;
    if (request->turn_index == 1 && last_tool_result != NULL && last_tool_result->status == POSAGENT_OK &&
        strcmp(last_tool_result->name, "double") == 0 && strcmp(last_tool_result->output_json, "{\"value\":18}") == 0) {
        model_state->received_tool_result = 1;
        response->action = POSAGENT_AGENT_FINAL;
        response->final_text = "Mock model received value 18";
        return POSAGENT_OK;
    }

    result->status = POSAGENT_ERR_INVALID_RESPONSE;
    (void)snprintf(result->message, sizeof(result->message), "mock model did not receive expected tool result");
    return POSAGENT_ERR_INVALID_RESPONSE;
}

static int run_agent_case(const char *tool_name, const char *arguments_json, uint32_t max_model_turns, size_t final_capacity, posagent_status_t expected_status, int expected_tool_calls, int expected_model_calls) {
    posagent_graph_t *graph = NULL;
    posagent_context_t *context = NULL;
    posagent_result_t result = {0};
    mock_model_state_t model_state = { tool_name, arguments_json, 0, 0, 0 };
    mock_tool_state_t tool_state = { 0 };
    posagent_agent_tool_t tool = {
        "double",
        "double a test value",
        "{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"integer\"}},\"required\":[\"value\"],\"additionalProperties\":false}",
        mock_json_tool,
        &tool_state
    };
    char final_response[64] = {0};

    CHECK(posagent_graph_create(&graph) == POSAGENT_OK);
    CHECK(posagent_agent_tool_register(graph, &tool) == POSAGENT_OK);
    CHECK(posagent_agent_tool_register(graph, &tool) == POSAGENT_ERR_INVALID_ARG);
    CHECK(posagent_context_create(graph, NULL, &context) == POSAGENT_OK);

    posagent_status_t status = posagent_context_run_agent(
        context, mock_model_callback, &model_state, max_model_turns,
        final_response, final_capacity, &result);

    CHECK(status == expected_status);
    CHECK(result.status == expected_status);
    CHECK(tool_state.calls == expected_tool_calls);
    CHECK(model_state.calls == expected_model_calls);
    CHECK(model_state.received_tool_spec == 1);
    if (expected_status == POSAGENT_OK) {
        CHECK(model_state.received_tool_result == 1);
        CHECK(strcmp(final_response, "Mock model received value 18") == 0);
    }

    posagent_context_destroy(context);
    posagent_graph_destroy(graph);
    return 0;
}

static int test_model_tool_round_trip(void) {
    CHECK(run_agent_case("double", "{\"value\":9}", 3, 64, POSAGENT_OK, 1, 2) == 0);
    return 0;
}

static int test_agent_loop_error_boundaries(void) {
    CHECK(run_agent_case("missing", "{\"value\":9}", 3, 64, POSAGENT_ERR_NOT_FOUND, 0, 1) == 0);
    CHECK(run_agent_case("double", "{\"value\":9}", 1, 64, POSAGENT_ERR_MAX_STEPS, 0, 1) == 0);
    CHECK(run_agent_case("double", "not-json", 3, 64, POSAGENT_ERR_INVALID_RESPONSE, 0, 1) == 0);
    CHECK(run_agent_case("double", "{\"value\":9", 3, 64, POSAGENT_ERR_INVALID_RESPONSE, 0, 1) == 0);
    CHECK(run_agent_case("double", "{\"value\":\"9\"}", 3, 64, POSAGENT_ERR_INVALID_RESPONSE, 0, 1) == 0);
    CHECK(run_agent_case("double", "{}", 3, 64, POSAGENT_ERR_INVALID_RESPONSE, 0, 1) == 0);
    CHECK(run_agent_case("double", "{\"value\":9,\"other\":1}", 3, 64, POSAGENT_ERR_INVALID_RESPONSE, 0, 1) == 0);
    CHECK(run_agent_case("double", "{\"value\":9,\"value\":9}", 3, 64, POSAGENT_ERR_INVALID_RESPONSE, 0, 1) == 0);
    CHECK(run_agent_case("double", "{\"value\":9.0}", 3, 64, POSAGENT_ERR_INVALID_RESPONSE, 0, 1) == 0);
    CHECK(run_agent_case("double", "{\"value\":9,}", 3, 64, POSAGENT_ERR_INVALID_RESPONSE, 0, 1) == 0);
    CHECK(run_agent_case("double", "{\"\\u0076alue\":9}", 3, 64, POSAGENT_ERR_TOOL, 1, 1) == 0);
    CHECK(run_agent_case("double", "{\"value\":8}", 3, 64, POSAGENT_ERR_TOOL, 1, 1) == 0);
    CHECK(run_agent_case("double", "{\"value\":9}", 3, 4, POSAGENT_ERR_BUFFER_TOO_SMALL, 1, 2) == 0);
    return 0;
}

static int test_schema_registration(void) {
    posagent_graph_t *graph = NULL;
    mock_tool_state_t state = {0};
    posagent_agent_tool_t tool = { "checked", "schema check", NULL, mock_json_tool, &state };
    CHECK(posagent_graph_create(&graph) == POSAGENT_OK);
    tool.arguments_schema_json = "{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"integer\"}},\"required\":[\"value\"],\"additionalProperties\":false}";
    CHECK(posagent_agent_tool_register(graph, &tool) == POSAGENT_OK);
    tool.name = "bad_syntax";
    tool.arguments_schema_json = "{\"type\":\"object\"";
    CHECK(posagent_agent_tool_register(graph, &tool) == POSAGENT_ERR_INVALID_ARG);
    tool.name = "unsupported_keyword";
    tool.arguments_schema_json = "{\"type\":\"object\",\"enum\":[{}]}";
    CHECK(posagent_agent_tool_register(graph, &tool) == POSAGENT_ERR_INVALID_ARG);
    tool.name = "unknown_required";
    tool.arguments_schema_json = "{\"type\":\"object\",\"properties\":{},\"required\":[\"value\"]}";
    CHECK(posagent_agent_tool_register(graph, &tool) == POSAGENT_ERR_INVALID_ARG);
    tool.name = "duplicate_keyword";
    tool.arguments_schema_json = "{\"type\":\"object\",\"type\":\"object\"}";
    CHECK(posagent_agent_tool_register(graph, &tool) == POSAGENT_ERR_INVALID_ARG);
    posagent_graph_destroy(graph);
    return 0;
}

static posagent_status_t failing_model_callback(const posagent_agent_model_request_t *request, void *user_data, posagent_agent_response_t *response, posagent_result_t *result) {
    result->status = POSAGENT_ERR_TIMEOUT;
    (void)snprintf(result->message, sizeof(result->message), "mock model timeout");
    (void)request;
    (void)user_data;
    (void)response;
    return POSAGENT_OK;
}

static int test_model_error_propagation(void) {
    posagent_graph_t *graph = NULL;
    posagent_context_t *context = NULL;
    posagent_result_t result = {0};
    char final_response[64] = {0};

    CHECK(posagent_graph_create(&graph) == POSAGENT_OK);
    CHECK(posagent_context_create(graph, NULL, &context) == POSAGENT_OK);
    CHECK(posagent_context_run_agent(context, failing_model_callback, NULL, 2,
                                     final_response, sizeof(final_response), &result) == POSAGENT_ERR_TIMEOUT);
    CHECK(result.status == POSAGENT_ERR_TIMEOUT);
    CHECK(strcmp(result.message, "mock model timeout") == 0);

    posagent_context_destroy(context);
    posagent_graph_destroy(graph);
    return 0;
}

int main(void) {
    CHECK(run_branch_case(11, 1) == 0);
    CHECK(run_branch_case(10, 2) == 0);
    CHECK(test_standalone_tool_callback() == 0);
    CHECK(test_model_tool_round_trip() == 0);
    CHECK(test_agent_loop_error_boundaries() == 0);
    CHECK(test_schema_registration() == 0);
    CHECK(test_model_error_propagation() == 0);
    puts("All PosAgent tests passed");
    return 0;
}

#undef CHECK
