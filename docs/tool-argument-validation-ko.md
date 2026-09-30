# 도구 인자 검증 계약

현재 런타임은 외부 JSON 라이브러리 없이 도구 인자의 JSON 구문과 제한된 스키마를 검증한다. 등록 시 지원하지 않는 스키마는 `POSAGENT_ERR_INVALID_ARG`로 거절한다. 모델이 반환한 인자가 JSON 구문 또는 등록 스키마에 맞지 않으면 도구 콜백을 호출하지 않고 `POSAGENT_ERR_INVALID_RESPONSE`를 반환한다.

## 지원하는 스키마

루트 스키마는 `{"type":"object"}`여야 한다. 각 스키마 객체에는 단일 문자열 `type`이 필수이며, 지원 타입은 `object`, `array`, `string`, `integer`, `number`, `boolean`, `null`이다.

- `object`: `properties` 객체, `required` 문자열 배열, `additionalProperties: false`를 지원한다. `required`의 이름은 `properties`에 있어야 한다. `additionalProperties`를 생략하면 추가 속성을 허용한다.
- `array`: `items`에 단일 스키마 객체가 필수다.
- 나머지 타입: 별도의 스키마 키워드를 지원하지 않는다.

`enum`, `minimum`, `pattern`, 조합 스키마 등 다른 JSON Schema 키워드는 지원하지 않으며 등록 시 거절한다. `integer`는 소수점과 지수 표기가 없는 JSON 숫자 토큰만 허용한다. 스키마 키워드와 속성 이름은 이스케이프하지 않은 ASCII로 작성한다. 모델 인자의 객체 키는 ASCII 또는 그에 해당하는 JSON 이스케이프를 사용할 수 있다. 객체 키의 중복은 거절한다.

스키마와 인자 JSON은 각각 최대 4095바이트이며, 파서는 최대 1024개 토큰과 32단계 중첩을 허용한다. 한계를 넘으면 스키마 등록 또는 인자 검증이 실패한다. 등록한 스키마 문자열은 graph가 복사하지 않으므로 graph 수명 동안 유지하고 변경하지 않아야 한다.

런타임 검증은 타입과 구조에 한정된다. 수치 범위, 업무 규칙, 권한 검사는 도구 콜백에서 수행한다. 도구 결과 JSON은 현재 구문 검증 대상이 아니며 크기와 NUL 종료만 확인한다.
