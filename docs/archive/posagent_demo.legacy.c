#include "posagent.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_NODES 16
#define MAX_EDGES 32
#define MAX_TRACE 32

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

typedef struct {
    int value;
    int tool_value;
    int branch_decision;
} demo_state_t;

static void node_start_fn(void *state, void *user_data, posagent_result_t *result) {
    demo_state_t *demo = (demo_state_t *)state;
    demo->value = 10;
    demo->branch_decision = 0;
    if (result) {
        result->status = POSAGENT_OK;
        result->code = 0;
        snprintf(result->message, sizeof(result->message), "start node executed");
    }
    (void)user_data;
}

static void node_tool_fn(void *state, void *user_data, posagent_result_t *result) {
    demo_state_t *demo = (demo_state_t *)state;
    (void)user_data;
    demo->tool_value = demo->value + 5;
    if (result) {
        result->status = POSAGENT_OK;
        result->code = 0;
        snprintf(result->message, sizeof(result->message), "tool value=%d", demo->tool_value);
    }
}

static void node_end_fn(void *state, void *user_data, posagent_result_t *result) {
    demo_state_t *demo = (demo_state_t *)state;
    (void)user_data;
    demo->branch_decision = 1;
    if (result) {
        result->status = POSAGENT_OK;
        result->code = 0;
        snprintf(result->message, sizeof(result->message), "end node reached: value=%d", demo->tool_value);
    }
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

static void tool_callback(const void *input, void *output, void *user_data, posagent_result_t *result) {
    const int *value = (const int *)input;
    int *out = (int *)output;
    (void)user_data;

    *out = (*value) * 2;
    result->status = POSAGENT_OK;
    result->code = 0;
    snprintf(result->message, sizeof(result->message), "tool executed with value=%d", *value);
}

int main(void) {
    posagent_graph_t *graph = NULL;
    posagent_context_t *ctx = NULL;
    demo_state_t state = {0};
    posagent_result_t result = {0};
    posagent_result_t tool_result = {0};
    posagent_tool_t tool = { 1, "status_tool", "returns doubled value", tool_callback, NULL, sizeof(int), sizeof(int) };
    int input = 7;
    int output = 0;

    posagent_graph_create(&graph);
    posagent_graph_set_max_steps(graph, 8);
    posagent_graph_add_node(graph, 1, "start", node_start_fn, NULL);
    posagent_graph_add_node(graph, 2, "check", node_tool_fn, NULL);
    posagent_graph_add_node(graph, 3, "end_true", node_end_fn, NULL);
    posagent_graph_add_node(graph, 4, "end_false", node_end_fn, NULL);
    posagent_graph_set_start_node(graph, 1);
    posagent_graph_add_edge(graph, 1, 2);
    posagent_graph_add_conditional_edge(graph, 2, 3, 4, route_decision_fn, NULL);
    posagent_tool_register(graph, &tool);

    posagent_context_create(graph, &state, &ctx);
    posagent_context_set_trace_callback(ctx, trace_callback, NULL);

    posagent_tool_invoke(&tool, &input, &output, &tool_result);
    printf("tool result: status=%d value=%d message=%s\n", tool_result.status, output, tool_result.message);

    posagent_status_t status = posagent_context_execute(ctx, &result);
    printf("status=%d code=%d message=%s\n", status, result.code, result.message);
    printf("final state: value=%d tool_value=%d branch_decision=%d\n", state.value, state.tool_value, state.branch_decision);

    posagent_context_destroy(ctx);
    posagent_graph_destroy(graph);
    return status == POSAGENT_OK ? 0 : 1;
}
