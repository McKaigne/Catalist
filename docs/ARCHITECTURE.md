# Catalist Architectural Decision Records (ADRs)

## ADR-001: Self-Documenting Code over Inline Code Comments
- **Context:** Inline comments explaining step-by-step logic drifted over time.
- **Decision:** Remove comments explaining *what* code does. Express intent
  through semantic function names and boolean predicates. Reserve comments
  strictly for *why*, invariants, and thread-safety contracts.

## ADR-002: Strong Types and Optionality over Magic Sentinels
- **Context:** Magic values (`-1`, `"0000-00-00"`) caused error-prone checks.
- **Decision:** Use `std::optional<T>` and `Result<T>` instead of magic sentinels.

## ADR-003: Decoupled Multi-Provider Subsystem
- **Context:** Remote search routines were split across ad-hoc curl routines.
- **Decision:** Unify external providers behind the `IProvider` interface,
  managed by `ProviderRegistry`.

## ADR-004: Explicit Unix Filter and Standard Stream Discipline
- **Context:** Terminal tools must integrate smoothly into shell pipelines.
- **Decision:** Formatted tables go to `stdout` only on interactive TTYs;
  `--plain`, `--ids`, and `--json` produce clean machine-readable streams;
  all diagnostic and error messages route strictly to `stderr`.
