# Repository working rules

## Verification budget

- Default to incremental verification: build only affected targets, run directly affected
  tests, one representative live-data sample when applicable, and 3–5 focused API
  contracts.
- Run the complete CTest suite or full API contract suite only when a change touches a
  shared parser, transport, cache, API schema, packaging/deployment boundary, or when the
  user explicitly requests a full/stage-release regression.
- Documentation and evidence-only changes do not require service or full-suite regression.
- Keep tool output concise: report summaries and failures; do not print successful full
  logs unless they are needed to diagnose a failure.

## C++ source organization

- Keep command/orchestration translation units separate from domain parsing, validation,
  request templates, and protocol implementations.
- Treat 5,000 lines in a production `.cpp` as a soft split threshold. Do not add a new
  responsibility to a file already above that threshold without first extracting a
  cohesive module.
- Prefer typed registries/strategy tables for fixed command and contract dispatch. Keep
  large embedded request constants in a dedicated catalog module instead of control flow.
- A refactor must preserve the public schema and be checked with affected targets and
  focused contracts; line movement alone does not justify a full regression run.
- Treat 2,500 lines in a test `.cpp` as a soft split threshold. Keep one focused test
  executable when startup fixtures are shared, but place semantic domains in separate
  translation units and dispatch them through a named typed registry so failures identify
  their domain.
