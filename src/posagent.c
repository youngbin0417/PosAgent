#include "posagent.h"
#include "posagent_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_NODES 16
#define MAX_EDGES 32

typedef struct {
    posagent_node_id_t id;
    const char *name;
    posagent_node_fn_t callback;
    void *user_data;
} posagent_node_entry_t;

typedef struct {
    posagent_node_id_t from_node;
    posagent_node_id_t to_node;
} posagent_edge_entry_t;

typedef struct {
    posagent_node_id_t from_node;
    posagent_node_id_t true_node;
    posagent_node_id_t false_node;
    posagent_route_fn_t callback;
    void *user_data;
} posagent_conditional_edge_entry_t;

struct posagent_graph {
    posagent_node_entry_t nodes[MAX_NODES];
    int node_count;
    posagent_edge_entry_t edges[MAX_EDGES];
    int edge_count;
    posagent_conditional_edge_entry_t cond_edges[MAX_EDGES];
    int cond_edge_count;
    posagent_node_id_t start_node;
    uint32_t max_steps;
    posagent_tool_t tools[MAX_NODES];
    int tool_count;
    posagent_agent_tool_t agent_tools[MAX_NODES];
    int agent_tool_count;
};

struct posagent_context {
    const posagent_graph_t *graph;
    void *state;
    posagent_trace_cb_t trace_callback;
    void *trace_user_data;
    uint64_t execution_id;
    uint32_t step_count;
};

static uint64_t posagent_now_ms(void) {
    return (uint64_t)(clock() * 1000ULL / CLOCKS_PER_SEC);
}

static void posagent_emit_trace(posagent_context_t *ctx, int type, posagent_node_id_t node_id, int32_t code, const char *message) {
    if (ctx == NULL || ctx->trace_callback == NULL) {
        return;
    }

    posagent_trace_event_t event;
    event.execution_id = ctx->execution_id;
    event.type = type;
    event.node_id = node_id;
    event.timestamp_ms = posagent_now_ms();
    event.code = code;
    event.message = message;
    ctx->trace_callback(&event, ctx->trace_user_data);
}

static const posagent_node_entry_t *posagent_find_node(const posagent_graph_t *graph, posagent_node_id_t node_id) {
    for (int i = 0; i < graph->node_count; ++i) {
        if (graph->nodes[i].id == node_id) {
            return &graph->nodes[i];
        }
    }
    return NULL;
}

static const posagent_agent_tool_t *posagent_find_agent_tool(const posagent_graph_t *graph, const char *name) {
    for (int i = 0; i < graph->agent_tool_count; ++i) {
        if (strcmp(graph->agent_tools[i].name, name) == 0) {
            return &graph->agent_tools[i];
        }
    }
    return NULL;
}

posagent_status_t posagent_graph_create(posagent_graph_t **graph_out) {
    if (graph_out == NULL) {
        return POSAGENT_ERR_INVALID_ARG;
    }

    posagent_graph_t *graph = (posagent_graph_t *)calloc(1, sizeof(posagent_graph_t));
    if (graph == NULL) {
        return POSAGENT_ERR_INTERNAL;
    }

    graph->max_steps = 16;
    *graph_out = graph;
    return POSAGENT_OK;
}

posagent_status_t posagent_graph_destroy(posagent_graph_t *graph) {
    free(graph);
    return POSAGENT_OK;
}

posagent_status_t posagent_graph_set_max_steps(posagent_graph_t *graph, uint32_t max_steps) {
    if (graph == NULL) {
        return POSAGENT_ERR_INVALID_ARG;
    }
    graph->max_steps = max_steps;
    return POSAGENT_OK;
}

posagent_status_t posagent_graph_add_node(posagent_graph_t *graph, posagent_node_id_t id, const char *name, posagent_node_fn_t callback, void *user_data) {
    if (graph == NULL || name == NULL || callback == NULL) {
        return POSAGENT_ERR_INVALID_ARG;
    }
    if (graph->node_count >= MAX_NODES) {
        return POSAGENT_ERR_INTERNAL;
    }

    posagent_node_entry_t *node = &graph->nodes[graph->node_count++];
    node->id = id;
    node->name = name;
    node->callback = callback;
    node->user_data = user_data;
    return POSAGENT_OK;
}

posagent_status_t posagent_graph_add_edge(posagent_graph_t *graph, posagent_node_id_t from_node, posagent_node_id_t to_node) {
    if (graph == NULL) {
        return POSAGENT_ERR_INVALID_ARG;
    }
    if (graph->edge_count >= MAX_EDGES) {
        return POSAGENT_ERR_INTERNAL;
    }

    posagent_edge_entry_t *edge = &graph->edges[graph->edge_count++];
    edge->from_node = from_node;
    edge->to_node = to_node;
    return POSAGENT_OK;
}

posagent_status_t posagent_graph_add_conditional_edge(posagent_graph_t *graph, posagent_node_id_t from_node, posagent_node_id_t true_node, posagent_node_id_t false_node, posagent_route_fn_t callback, void *user_data) {
    if (graph == NULL || callback == NULL) {
        return POSAGENT_ERR_INVALID_ARG;
    }
    if (graph->cond_edge_count >= MAX_EDGES) {
        return POSAGENT_ERR_INTERNAL;
    }

    posagent_conditional_edge_entry_t *edge = &graph->cond_edges[graph->cond_edge_count++];
    edge->from_node = from_node;
    edge->true_node = true_node;
    edge->false_node = false_node;
    edge->callback = callback;
    edge->user_data = user_data;
    return POSAGENT_OK;
}

posagent_status_t posagent_graph_set_start_node(posagent_graph_t *graph, posagent_node_id_t node_id) {
    if (graph == NULL) {
        return POSAGENT_ERR_INVALID_ARG;
    }
    graph->start_node = node_id;
    return POSAGENT_OK;
}

posagent_status_t posagent_context_create(const posagent_graph_t *graph, void *state, posagent_context_t **ctx_out) {
    if (graph == NULL || ctx_out == NULL) {
        return POSAGENT_ERR_INVALID_ARG;
    }

    posagent_context_t *ctx = (posagent_context_t *)calloc(1, sizeof(posagent_context_t));
    if (ctx == NULL) {
        return POSAGENT_ERR_INTERNAL;
    }

    ctx->graph = graph;
    ctx->state = state;
    ctx->execution_id = 1;
    *ctx_out = ctx;
    return POSAGENT_OK;
}

posagent_status_t posagent_context_destroy(posagent_context_t *ctx) {
    free(ctx);
    return POSAGENT_OK;
}

posagent_status_t posagent_context_set_trace_callback(posagent_context_t *ctx, posagent_trace_cb_t callback, void *user_data) {
    if (ctx == NULL) {
        return POSAGENT_ERR_INVALID_ARG;
    }
    ctx->trace_callback = callback;
    ctx->trace_user_data = user_data;
    return POSAGENT_OK;
}

static posagent_node_id_t posagent_next_node_from_graph(const posagent_graph_t *graph, posagent_node_id_t current_node, void *state) {
    for (int i = 0; i < graph->cond_edge_count; ++i) {
        const posagent_conditional_edge_entry_t *edge = &graph->cond_edges[i];
        if (edge->from_node == current_node) {
            int choice = edge->callback(state, edge->user_data);
            return choice ? edge->true_node : edge->false_node;
        }
    }

    for (int i = 0; i < graph->edge_count; ++i) {
        const posagent_edge_entry_t *edge = &graph->edges[i];
        if (edge->from_node == current_node) {
            return edge->to_node;
        }
    }
    return -1;
}

posagent_status_t posagent_context_execute(posagent_context_t *ctx, posagent_result_t *result) {
    if (ctx == NULL || result == NULL) {
        return POSAGENT_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(*result));
    const posagent_graph_t *graph = ctx->graph;
    posagent_node_id_t current = graph->start_node;
    posagent_result_t node_result = { POSAGENT_OK, 0, "" };

    for (uint32_t step = 0; step < graph->max_steps; ++step) {
        const posagent_node_entry_t *node = posagent_find_node(graph, current);
        if (node == NULL) {
            result->status = POSAGENT_ERR_NOT_FOUND;
            snprintf(result->message, sizeof(result->message), "node %d not found", current);
            posagent_emit_trace(ctx, POSAGENT_TRACE_ERROR, current, POSAGENT_ERR_NOT_FOUND, result->message);
            return POSAGENT_ERR_NOT_FOUND;
        }

        posagent_emit_trace(ctx, POSAGENT_TRACE_NODE_START, current, 0, node->name);
        node->callback(ctx->state, node->user_data, &node_result);
        posagent_emit_trace(ctx, POSAGENT_TRACE_NODE_END, current, node_result.code, node_result.message);

        if (node_result.status != POSAGENT_OK) {
            result->status = node_result.status;
            result->code = node_result.code;
            snprintf(result->message, sizeof(result->message), "%s", node_result.message);
            posagent_emit_trace(ctx, POSAGENT_TRACE_ERROR, current, node_result.code, node_result.message);
            return node_result.status;
        }

        ctx->step_count = step + 1;
        posagent_node_id_t next = posagent_next_node_from_graph(graph, current, ctx->state);
        if (next < 0) {
            result->status = POSAGENT_OK;
            result->code = 0;
            snprintf(result->message, sizeof(result->message), "graph execution completed");
            posagent_emit_trace(ctx, POSAGENT_TRACE_GRAPH_END, current, 0, "completed");
            return POSAGENT_OK;
        }

        current = next;
    }

    result->status = POSAGENT_ERR_MAX_STEPS;
    result->code = POSAGENT_ERR_MAX_STEPS;
    snprintf(result->message, sizeof(result->message), "maximum steps reached");
    posagent_emit_trace(ctx, POSAGENT_TRACE_ERROR, current, POSAGENT_ERR_MAX_STEPS, result->message);
    return POSAGENT_ERR_MAX_STEPS;
}

posagent_status_t posagent_tool_register(posagent_graph_t *graph, const posagent_tool_t *tool) {
    if (graph == NULL || tool == NULL) {
        return POSAGENT_ERR_INVALID_ARG;
    }
    if (graph->tool_count >= MAX_NODES) {
        return POSAGENT_ERR_INTERNAL;
    }

    graph->tools[graph->tool_count++] = *tool;
    return POSAGENT_OK;
}

posagent_status_t posagent_tool_invoke(const posagent_tool_t *tool, const void *input, void *output, posagent_result_t *result) {
    if (tool == NULL || result == NULL) {
        return POSAGENT_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(*result));
    tool->callback(input, output, tool->user_data, result);
    return result->status;
}

posagent_status_t posagent_agent_tool_register(posagent_graph_t *graph, const posagent_agent_tool_t *tool) {
    if (graph == NULL || tool == NULL || tool->name == NULL || tool->name[0] == '\0' ||
        tool->arguments_schema_json == NULL || tool->callback == NULL ||
        !posagent_json_schema_supported(tool->arguments_schema_json)) {
        return POSAGENT_ERR_INVALID_ARG;
    }
    if (graph->agent_tool_count >= MAX_NODES) {
        return POSAGENT_ERR_INTERNAL;
    }
    if (posagent_find_agent_tool(graph, tool->name) != NULL) {
        return POSAGENT_ERR_INVALID_ARG;
    }

    graph->agent_tools[graph->agent_tool_count++] = *tool;
    return POSAGENT_OK;
}

posagent_status_t posagent_context_run_agent(posagent_context_t *ctx, posagent_agent_model_fn_t model_callback, void *model_user_data, uint32_t max_model_turns, char *final_response, size_t final_capacity, posagent_result_t *result) {
    if (ctx == NULL || model_callback == NULL || max_model_turns == 0 || final_response == NULL || final_capacity == 0 || result == NULL) {
        return POSAGENT_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(*result));
    final_response[0] = '\0';

    posagent_agent_tool_result_t last_tool_result = { NULL, NULL, POSAGENT_OK, 0, NULL };
    posagent_result_t tool_result = { POSAGENT_OK, 0, "" };
    char tool_output[POSAGENT_MAX_TOOL_JSON_SIZE];
    posagent_agent_tool_info_t tool_info[MAX_NODES];

    for (int i = 0; i < ctx->graph->agent_tool_count; ++i) {
        tool_info[i].name = ctx->graph->agent_tools[i].name;
        tool_info[i].description = ctx->graph->agent_tools[i].description;
        tool_info[i].arguments_schema_json = ctx->graph->agent_tools[i].arguments_schema_json;
    }

    for (uint32_t turn = 0; turn < max_model_turns; ++turn) {
        posagent_agent_response_t response = { 0 };
        posagent_result_t model_result = { POSAGENT_OK, 0, "" };
        posagent_agent_model_request_t request = {
            ctx->state,
            tool_info,
            (size_t)ctx->graph->agent_tool_count,
            last_tool_result.name != NULL ? &last_tool_result : NULL,
            turn
        };

        posagent_emit_trace(ctx, POSAGENT_TRACE_MODEL_START, -1, 0, "model request");
        posagent_status_t status = model_callback(&request, model_user_data, &response, &model_result);
        if (status == POSAGENT_OK && model_result.status != POSAGENT_OK) {
            status = model_result.status;
        }
        const char *model_message = model_result.message;
        if (status == POSAGENT_OK && response.action == POSAGENT_AGENT_FINAL) {
            model_message = "final response";
        } else if (status == POSAGENT_OK && response.action == POSAGENT_AGENT_TOOL_CALL && response.tool_name != NULL) {
            model_message = response.tool_name;
        }
        posagent_emit_trace(ctx, POSAGENT_TRACE_MODEL_END, -1, status, model_message);

        if (status != POSAGENT_OK) {
            *result = model_result;
            result->status = status;
            if (result->message[0] == '\0') {
                snprintf(result->message, sizeof(result->message), "model callback failed");
            }
            posagent_emit_trace(ctx, POSAGENT_TRACE_ERROR, -1, status, result->message);
            return status;
        }

        if (response.action == POSAGENT_AGENT_FINAL) {
            if (response.final_text == NULL) {
                result->status = POSAGENT_ERR_INVALID_RESPONSE;
                snprintf(result->message, sizeof(result->message), "final response text is missing");
                posagent_emit_trace(ctx, POSAGENT_TRACE_ERROR, -1, result->status, result->message);
                return result->status;
            }

            size_t final_length = strlen(response.final_text);
            if (final_length >= final_capacity) {
                result->status = POSAGENT_ERR_BUFFER_TOO_SMALL;
                snprintf(result->message, sizeof(result->message), "final response buffer is too small");
                posagent_emit_trace(ctx, POSAGENT_TRACE_ERROR, -1, result->status, result->message);
                return result->status;
            }

            memcpy(final_response, response.final_text, final_length + 1);
            result->status = POSAGENT_OK;
            result->code = 0;
            snprintf(result->message, sizeof(result->message), "agent execution completed");
            posagent_emit_trace(ctx, POSAGENT_TRACE_GRAPH_END, -1, 0, "agent completed");
            return POSAGENT_OK;
        }

        if (response.action != POSAGENT_AGENT_TOOL_CALL || response.tool_name == NULL || response.tool_name[0] == '\0' || response.arguments_json == NULL) {
            result->status = POSAGENT_ERR_INVALID_RESPONSE;
            snprintf(result->message, sizeof(result->message), "model response is not a valid final or tool call");
            posagent_emit_trace(ctx, POSAGENT_TRACE_ERROR, -1, result->status, result->message);
            return result->status;
        }

        if (turn + 1 >= max_model_turns) {
            result->status = POSAGENT_ERR_MAX_STEPS;
            snprintf(result->message, sizeof(result->message), "model turn limit reached before tool dispatch");
            posagent_emit_trace(ctx, POSAGENT_TRACE_ERROR, -1, result->status, result->message);
            return result->status;
        }

        if (strlen(response.arguments_json) >= POSAGENT_MAX_TOOL_JSON_SIZE) {
            result->status = POSAGENT_ERR_INVALID_RESPONSE;
            snprintf(result->message, sizeof(result->message), "tool arguments exceed the configured size limit");
            posagent_emit_trace(ctx, POSAGENT_TRACE_ERROR, -1, result->status, result->message);
            return result->status;
        }

        const posagent_agent_tool_t *tool = posagent_find_agent_tool(ctx->graph, response.tool_name);
        if (tool == NULL) {
            result->status = POSAGENT_ERR_NOT_FOUND;
            snprintf(result->message, sizeof(result->message), "tool '%s' is not registered", response.tool_name);
            posagent_emit_trace(ctx, POSAGENT_TRACE_ERROR, -1, result->status, result->message);
            return result->status;
        }

        if (!posagent_json_arguments_match(tool->arguments_schema_json, response.arguments_json)) {
            result->status = POSAGENT_ERR_INVALID_RESPONSE;
            snprintf(result->message, sizeof(result->message), "tool arguments are invalid or do not match the registered schema");
            posagent_emit_trace(ctx, POSAGENT_TRACE_ERROR, -1, result->status, result->message);
            return result->status;
        }

        memset(tool_output, 0, sizeof(tool_output));
        memset(&tool_result, 0, sizeof(tool_result));
        tool_result.status = POSAGENT_OK;
        size_t output_size = 0;

        posagent_emit_trace(ctx, POSAGENT_TRACE_TOOL_START, -1, 0, tool->name);
        status = tool->callback(response.arguments_json, tool_output, sizeof(tool_output), &output_size, tool->user_data, &tool_result);
        if (status == POSAGENT_OK && tool_result.status != POSAGENT_OK) {
            status = tool_result.status;
        }
        if (status == POSAGENT_OK && (output_size >= sizeof(tool_output) || tool_output[output_size] != '\0')) {
            status = POSAGENT_ERR_BUFFER_TOO_SMALL;
            snprintf(tool_result.message, sizeof(tool_result.message), "tool output is invalid or too large");
        }
        posagent_emit_trace(ctx, POSAGENT_TRACE_TOOL_END, -1, status, tool->name);

        if (status != POSAGENT_OK) {
            result->status = status;
            result->code = tool_result.code;
            snprintf(result->message, sizeof(result->message), "%s", tool_result.message[0] != '\0' ? tool_result.message : "tool callback failed");
            posagent_emit_trace(ctx, POSAGENT_TRACE_ERROR, -1, status, result->message);
            return status;
        }

        last_tool_result.name = tool->name;
        last_tool_result.output_json = tool_output;
        last_tool_result.status = status;
        last_tool_result.code = tool_result.code;
        last_tool_result.message = tool_result.message;
    }

    result->status = POSAGENT_ERR_MAX_STEPS;
    snprintf(result->message, sizeof(result->message), "model turn limit reached");
    return result->status;
}
