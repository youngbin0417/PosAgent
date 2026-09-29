# PosAgent 확장 문서 안내

현재 PoC는 Windows에서 mock model -> 등록 도구 -> model 왕복과 제한된 JSON 스키마 인자 검증까지 확인했습니다. 실제 LLM provider/network adapter와 범용 JSON Schema 검증은 아직 없습니다. 아래 설계 문서는 그 다음 확장을 안내하며 일부 API는 제안입니다.

- [검증 현황 체크리스트](verification-status-ko.md): 실제 동작 확인 항목과 미구현 항목
- [도구 인자 검증 계약](tool-argument-validation-ko.md): 런타임이 지원하는 스키마와 오류 처리
- [모델 및 도구 어댑터 설계안](model-tool-adapter-design-ko.md): 모델 공급자 경계, 등록 도구 dispatch, 검증과 안전 정책
- [에이전트 API 초안](posagent-agent-api-draft-ko.md): 상태 그래프와 모델/도구 왕복을 위한 C API 제안
- [PoC 확장 계획](poc-expansion-roadmap-ko.md): 현재 기준선, 단계별 산출물 및 완료 기준

실제 기능 구현 전에는 API 초안의 ABI, JSON 처리, 메모리 소유권 결정을 먼저 확정해야 합니다.
