#ifndef MICROAGENT_C_API_H
#define MICROAGENT_C_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * MicroAgent-C 초기 공개 API 초안
 * 대상: Linux PoC 기준
 * 목적: 그래프 생성, 상태 실행, 도구 등록, 추적 콜백을 검증하는 최소 계약
 */

#define MA_OK 0
#define MA_ERR_INVALID_ARG 1
#define MA_ERR_NOT_FOUND 2
#define MA_ERR_TIMEOUT 3
#define MA_ERR_TOOL_EXEC 4
#define MA_ERR_GRAPH_STATE 5
#define MA_ERR_MAX_STEPS 6
#define MA_ERR_CANCELLED 7
#define MA_ERR_INTERNAL 8

typedef int32_t ma_status_t;
typedef int32_t ma_node_id_t;
typedef int32_t ma_tool_id_t;
typedef int32_t ma_edge_id_t;

typedef struct ma_graph ma_graph_t;
typedef struct ma_node ma_node_t;
typedef struct ma_edge ma_edge_t;
typedef struct ma_context ma_context_t;
typedef struct ma_tool_spec ma_tool_spec_t;
typedef struct ma_result ma_result_t;
typedef struct ma_trace_event ma_trace_event_t;

typedef void (*ma_node_cb_t)(void *state, void *user_data, ma_result_t *result);
typedef int (*ma_route_cb_t)(void *state, void *user_data);
typedef void (*ma_tool_cb_t)(const void *args, void *out, void *user_data, ma_result_t *result);
typedef void (*ma_trace_cb_t)(const ma_trace_event_t *event, void *user_data);

enum ma_result_code {
    MA_RESULT_SUCCESS = 0,
    MA_RESULT_ERROR = 1,
    MA_RESULT_TIMEOUT = 2,
    MA_RESULT_CANCELLED = 3,
    MA_RESULT_MAX_STEPS = 4,
    MA_RESULT_TOOL_NOT_FOUND = 5,
    MA_RESULT_INVALID_ARGS = 6
};

enum ma_trace_type {
    MA_TRACE_NODE_START = 1,
    MA_TRACE_NODE_END = 2,
    MA_TRACE_TOOL_START = 3,
    MA_TRACE_TOOL_END = 4,
    MA_TRACE_ERROR = 5,
    MA_TRACE_GRAPH_END = 6
};

struct ma_result {
    ma_status_t status;
    int32_t code;
    void *payload;
    size_t payload_size;
    char *message;
};

struct ma_trace_event {
    uint64_t execution_id;
    int32_t type;
    ma_node_id_t node_id;
    ma_tool_id_t tool_id;
    uint64_t timestamp_ms;
    int32_t code;
    const char *message;
};

struct ma_node {
    ma_node_id_t id;
    const char *name;
    ma_node_cb_t callback;
    void *user_data;
};

struct ma_edge {
    ma_edge_id_t id;
    ma_node_id_t from_node;
    ma_node_id_t to_node;
};

struct ma_tool_spec {
    ma_tool_id_t id;
    const char *name;
    const char *description;
    ma_tool_cb_t callback;
    void *user_data;
    size_t arg_size;
    size_t result_size;
};

/*
 * 그래프 생성 및 소멸
 */
ma_status_t ma_graph_create(ma_graph_t **graph_out);
ma_status_t ma_graph_destroy(ma_graph_t *graph);
ma_status_t ma_graph_add_node(ma_graph_t *graph, const ma_node_t *node);
ma_status_t ma_graph_add_edge(ma_graph_t *graph, const ma_edge_t *edge);
ma_status_t ma_graph_set_start_node(ma_graph_t *graph, ma_node_id_t node_id);
ma_status_t ma_graph_set_max_steps(ma_graph_t *graph, uint32_t max_steps);

/*
 * 실행 문맥
 */
ma_status_t ma_context_create(const ma_graph_t *graph, void *state, ma_context_t **ctx_out);
ma_status_t ma_context_destroy(ma_context_t *ctx);
ma_status_t ma_context_execute(ma_context_t *ctx, ma_result_t **result_out);
ma_status_t ma_context_cancel(ma_context_t *ctx);
ma_status_t ma_context_set_trace_callback(ma_context_t *ctx, ma_trace_cb_t callback, void *user_data);

/*
 * 도구 등록 및 실행
 */
ma_status_t ma_tool_register(ma_graph_t *graph, const ma_tool_spec_t *tool_spec);
ma_status_t ma_tool_invoke(const ma_tool_spec_t *tool_spec, const void *args, void *out, ma_result_t **result_out);

/*
 * 결과 해제
 */
void ma_result_destroy(ma_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* MICROAGENT_C_API_H */
