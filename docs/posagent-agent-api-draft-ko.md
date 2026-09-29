# PosAgent 에이전트 API 초안

## 1. 목적

이 문서는 현재 C API를 LangGraph의 핵심 사용 경험(상태 그래프, 분기, 도구 루프)에 참고해 발전시키기 위한 초안이다. LangGraph API를 그대로 복제하는 문서가 아니며, 아래 API는 **제안일 뿐 현재 구현에 포함되지 않는다.** C ABI의 메모리 소유권과 오류 계약을 우선한다.

현재 `include/posagent.h`에는 `posagent_agent_tool_register()`와 `posagent_context_run_agent()`를 포함한 동기 mock-loop API가 구현되어 있다. 이 문서의 builder/compile 기반 시그니처는 향후 v2 형태의 제안이며 현재 헤더와 동일하지 않다.

## 2. 사용 모델

개발자는 호스트 상태를 정의하고 노드 콜백과 그래프 전이를 등록한다. 컴파일된 그래프는 재사용하고, 실행별 context는 별도로 생성한다. 모델/도구 호출은 노드에서 명시적으로 수행한다.

```text
builder -> compile -> context(state) -> invoke -> result
                                  model node -> tool node -> model node
```

## 3. 제안 타입

```c
#include <stddef.h>
#include <stdint.h>

typedef struct posagent_builder posagent_builder_t;
typedef struct posagent_graph posagent_graph_t;
typedef struct posagent_context posagent_context_t;

typedef int32_t posagent_node_id_t;
typedef int32_t posagent_status_t;

typedef struct {
    posagent_status_t status;
    int32_t code;
    char message[192];
} posagent_result_t;

typedef struct {
    const char *name;
    const char *description;
    const char *arguments_schema_json;
} posagent_tool_spec_t;

typedef struct {
    const char *tool_name;
    const char *arguments_json;
    const char *call_id;
} posagent_tool_call_t;

typedef enum {
    POSAGENT_MODEL_FINAL = 1,
    POSAGENT_MODEL_TOOL_CALLS = 2,
    POSAGENT_MODEL_ERROR = 3
} posagent_model_action_t;
```

문자열은 UTF-8이다. 권장 정책은 builder 등록 시 문자열을 복사하고 compile 시 graph가 필요한 설정을 독립적으로 소유하는 것이다. 이 정책을 따르면 builder는 compile 이후 해제할 수 있고 graph는 builder와 별도로 수명을 가진다.

## 4. 제안 콜백 계약

```c
typedef posagent_status_t (*posagent_node_fn_t)(
    void *state,
    void *user_data,
    posagent_context_t *context,
    posagent_result_t *result);

typedef posagent_status_t (*posagent_route_fn_t)(
    const void *state,
    void *user_data,
    posagent_node_id_t *next_node,
    posagent_result_t *result);

typedef posagent_status_t (*posagent_tool_fn_t)(
    const char *arguments_json,
    char *result_json,
    size_t result_capacity,
    size_t *result_size,
    void *user_data,
    posagent_result_t *result);

typedef posagent_status_t (*posagent_model_fn_t)(
    const char *request_json,
    char *response_json,
    size_t response_capacity,
    size_t *response_size,
    void *user_data,
    posagent_result_t *result);
```

현재 노드 콜백은 `void`를 반환하므로 오류 상태가 result를 통해서만 전달된다. 초안은 오류 전파가 반환값으로 명확하도록 바꾸지만, 기존 공개 API와 호환되지 않으므로 버전/ABI 변경으로 다뤄야 한다. 현재 `include/posagent.h`를 조용히 덮어쓰지 않는다.

모델 콜백은 provider SDK 타입을 공개하지 않는다. request/response는 UTF-8 JSON envelope이며, 모델 adapter가 공급자별 응답을 다음 공통 형식으로 정규화한다.

```json
{"action":"tool_calls","calls":[{"call_id":"c1","name":"get_status","arguments":{"unit":"A"}}]}
{"action":"final","content":"현재 상태는 정상입니다."}
{"action":"error","code":42,"message":"model request failed"}
```

런타임은 공통 응답 파싱과 tool 인자 검증을 담당한다. JSON 파서 의존성을 런타임에 포함할지, 호스트 제공 parser interface로 둘지는 구현 전 결정한다.

## 5. 제안 빌더 및 실행 API

```c
posagent_status_t posagent_builder_create(posagent_builder_t **out);
posagent_status_t posagent_builder_add_node(
    posagent_builder_t *builder, posagent_node_id_t id, const char *name,
    posagent_node_fn_t callback, void *user_data);
posagent_status_t posagent_builder_add_edge(
    posagent_builder_t *builder, posagent_node_id_t from, posagent_node_id_t to);
posagent_status_t posagent_builder_add_route(
    posagent_builder_t *builder, posagent_node_id_t from,
    posagent_route_fn_t route, const posagent_node_id_t *allowed_targets,
    size_t target_count, void *user_data);
posagent_status_t posagent_builder_set_start(
    posagent_builder_t *builder, posagent_node_id_t node_id);
posagent_status_t posagent_builder_set_limits(
    posagent_builder_t *builder, uint32_t max_steps, uint32_t max_model_turns);
posagent_status_t posagent_builder_add_tool(
    posagent_builder_t *builder, const posagent_tool_spec_t *spec,
    posagent_tool_fn_t callback, void *user_data);
posagent_status_t posagent_builder_set_model_callback(
    posagent_builder_t *builder, posagent_model_fn_t callback,
    void *user_data);
posagent_status_t posagent_builder_compile(
    posagent_builder_t *builder, posagent_graph_t **out,
    posagent_result_t *result);
posagent_status_t posagent_builder_destroy(posagent_builder_t *builder);
posagent_status_t posagent_graph_destroy(posagent_graph_t *graph);

posagent_status_t posagent_context_create(
    const posagent_graph_t *graph, void *host_state, posagent_context_t **out);
posagent_status_t posagent_context_invoke(
    posagent_context_t *context, posagent_result_t *result);
posagent_status_t posagent_context_cancel(posagent_context_t *context);
posagent_status_t posagent_context_destroy(posagent_context_t *context);
```

`compile`은 시작 노드 존재, 모든 edge/route 목적지 등록 여부, 중복 ID, route target 목록, 도구 이름 중복 등을 실행 전에 확인한다. 컴파일된 graph는 불변으로 취급한다. context와 host state는 실행별이며 host state는 호스트가 소유한다. 컴파일된 graph는 builder의 설정을 독립적으로 소유하며, context가 참조 중인 graph는 먼저 해제하면 안 된다.

## 6. 제안 tool/model API

```c
posagent_status_t posagent_context_call_tool(
    posagent_context_t *context, const char *tool_name,
    const char *arguments_json, char *result_json,
    size_t result_capacity, size_t *result_size,
    posagent_result_t *result);
posagent_status_t posagent_context_call_model(
    posagent_context_t *context, const char *request_json,
    char *response_json, size_t response_capacity,
    size_t *response_size, posagent_result_t *result);
```

도구와 모델 callback은 builder에 설정하고 compile 시 graph에 고정한다. 실행 중 권한 제한이 필요하면 context별 allowlist를 추가할 수 있다. 위 시그니처는 개념 예시이며, 공개 헤더에 반영하기 전에 schema 검증, 문자열 소유권, callback timeout 처리 방식을 결정해야 한다.

## 7. API 의미와 오류 정책

- 빌드 단계 설정 오류는 `compile`에서 실패한다.
- 실행 중 노드/도구/모델 오류는 status와 상세 result에 기록한다.
- max_steps와 max_model_turns는 각각 제한한다.
- 미등록 도구는 실행하지 않고 `NOT_FOUND` 또는 전용 `TOOL_NOT_ALLOWED` 오류를 반환한다.
- 도구 결과는 host state 변경 또는 공통 대화 상태 업데이트를 통해 다음 노드에 전달한다.
- 취소는 협력형이다. callback이 취소 확인을 지원하지 않으면 강제 종료를 보장하지 않는다.
- trace callback은 실행 thread에서 호출되며, reentrant API 호출 가능 여부를 명시한다.

## 8. 권장 1차 범위

1차 API에는 순차 실행, 정적 노드 등록, 상태 포인터, 조건부 route, 등록 도구 조회/호출, mock 가능한 동기 모델 callback, trace, 최대 실행 제한만 넣는다. 병렬 실행, 체크포인트, 범용 memory store, 동적 코드 로딩은 미룬다.

## 9. 구현 전 결정 사항

- 현재 ABI를 유지하며 확장할지, `posagent_v2` 헤더/ABI로 분리할지
- JSON 파서 의존성을 코어에 포함할지
- 도구 스키마를 제한형 자체 타입으로 시작할지 JSON Schema 라이브러리를 쓸지
- 도구 등록부를 graph 정의에 둘지 context별 권한 정책으로 분리할지
- 모델 adapter가 공급자 응답을 내부 타입으로 정규화하는 방식
- 버퍼 크기 초과, timeout, 부분 tool call 실패의 기본 정책

이 결정과 테스트가 확정되기 전에는 현재 공개 헤더를 대규모로 변경하지 않는다.
