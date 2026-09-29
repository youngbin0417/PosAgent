#ifndef POSAGENT_H
#define POSAGENT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    POSAGENT_OK = 0,
    POSAGENT_ERR_INVALID_ARG = 1,
    POSAGENT_ERR_NOT_FOUND = 2,
    POSAGENT_ERR_TIMEOUT = 3,
    POSAGENT_ERR_TOOL = 4,
    POSAGENT_ERR_MAX_STEPS = 5,
    POSAGENT_ERR_CANCELLED = 6,
    POSAGENT_ERR_INTERNAL = 7,
    POSAGENT_ERR_BUFFER_TOO_SMALL = 8,
    POSAGENT_ERR_INVALID_RESPONSE = 9
} posagent_status_t;

#define POSAGENT_MAX_TOOL_JSON_SIZE 4096

typedef int32_t posagent_node_id_t;
typedef int32_t posagent_tool_id_t;

typedef struct posagent_graph posagent_graph_t;
typedef struct posagent_context posagent_context_t;
typedef struct posagent_result posagent_result_t;
typedef struct posagent_trace_event posagent_trace_event_t;
typedef struct posagent_tool posagent_tool_t;
typedef struct posagent_agent_tool posagent_agent_tool_t;
typedef struct posagent_agent_tool_info posagent_agent_tool_info_t;
typedef struct posagent_agent_tool_result posagent_agent_tool_result_t;
typedef struct posagent_agent_model_request posagent_agent_model_request_t;
typedef struct posagent_agent_response posagent_agent_response_t;

typedef void (*posagent_node_fn_t)(void *state, void *user_data, posagent_result_t *result);
typedef void (*posagent_tool_fn_t)(const void *input, void *output, void *user_data, posagent_result_t *result);
typedef int (*posagent_route_fn_t)(void *state, void *user_data);
typedef void (*posagent_trace_cb_t)(const posagent_trace_event_t *event, void *user_data);
typedef posagent_status_t (*posagent_agent_tool_fn_t)(const char *arguments_json, char *output_json, size_t output_capacity, size_t *output_size, void *user_data, posagent_result_t *result);
typedef posagent_status_t (*posagent_agent_model_fn_t)(const posagent_agent_model_request_t *request, void *user_data, posagent_agent_response_t *response, posagent_result_t *result);

typedef enum {
    POSAGENT_AGENT_TOOL_CALL = 1,
    POSAGENT_AGENT_FINAL = 2
} posagent_agent_action_t;

enum {
    POSAGENT_TRACE_NODE_START = 1,
    POSAGENT_TRACE_NODE_END = 2,
    POSAGENT_TRACE_TOOL_START = 3,
    POSAGENT_TRACE_TOOL_END = 4,
    POSAGENT_TRACE_ERROR = 5,
    POSAGENT_TRACE_GRAPH_END = 6,
    POSAGENT_TRACE_MODEL_START = 7,
    POSAGENT_TRACE_MODEL_END = 8
};

struct posagent_result {
    posagent_status_t status;
    int32_t code;
    char message[128];
};

struct posagent_trace_event {
    uint64_t execution_id;
    int type;
    posagent_node_id_t node_id;
    uint64_t timestamp_ms;
    int32_t code;
    const char *message;
};

struct posagent_tool {
    posagent_tool_id_t id;
    const char *name;
    const char *description;
    posagent_tool_fn_t callback;
    void *user_data;
    size_t input_size;
    size_t output_size;
};

struct posagent_agent_tool {
    const char *name;
    const char *description;
    /* Borrowed and immutable for the graph lifetime; only the documented schema subset is supported. */
    const char *arguments_schema_json;
    posagent_agent_tool_fn_t callback;
    void *user_data;
};

struct posagent_agent_tool_info {
    const char *name;
    const char *description;
    const char *arguments_schema_json;
};

struct posagent_agent_tool_result {
    const char *name;
    const char *output_json;
    posagent_status_t status;
    int32_t code;
    const char *message;
};

struct posagent_agent_model_request {
    const void *state;
    const posagent_agent_tool_info_t *tools;
    size_t tool_count;
    const posagent_agent_tool_result_t *last_tool_result;
    uint32_t turn_index;
};

struct posagent_agent_response {
    posagent_agent_action_t action;
    const char *final_text;
    const char *tool_name;
    const char *arguments_json;
};

posagent_status_t posagent_graph_create(posagent_graph_t **graph_out);
posagent_status_t posagent_graph_destroy(posagent_graph_t *graph);
posagent_status_t posagent_graph_set_max_steps(posagent_graph_t *graph, uint32_t max_steps);
posagent_status_t posagent_graph_add_node(posagent_graph_t *graph, posagent_node_id_t id, const char *name, posagent_node_fn_t callback, void *user_data);
posagent_status_t posagent_graph_add_edge(posagent_graph_t *graph, posagent_node_id_t from_node, posagent_node_id_t to_node);
posagent_status_t posagent_graph_add_conditional_edge(posagent_graph_t *graph, posagent_node_id_t from_node, posagent_node_id_t true_node, posagent_node_id_t false_node, posagent_route_fn_t callback, void *user_data);
posagent_status_t posagent_graph_set_start_node(posagent_graph_t *graph, posagent_node_id_t node_id);

posagent_status_t posagent_context_create(const posagent_graph_t *graph, void *state, posagent_context_t **ctx_out);
posagent_status_t posagent_context_destroy(posagent_context_t *ctx);
posagent_status_t posagent_context_execute(posagent_context_t *ctx, posagent_result_t *result);
posagent_status_t posagent_context_set_trace_callback(posagent_context_t *ctx, posagent_trace_cb_t callback, void *user_data);

posagent_status_t posagent_tool_register(posagent_graph_t *graph, const posagent_tool_t *tool);
posagent_status_t posagent_tool_invoke(const posagent_tool_t *tool, const void *input, void *output, posagent_result_t *result);
posagent_status_t posagent_agent_tool_register(posagent_graph_t *graph, const posagent_agent_tool_t *tool);
posagent_status_t posagent_context_run_agent(posagent_context_t *ctx, posagent_agent_model_fn_t model_callback, void *model_user_data, uint32_t max_model_turns, char *final_response, size_t final_capacity, posagent_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* POSAGENT_H */
