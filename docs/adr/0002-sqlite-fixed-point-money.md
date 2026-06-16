# ADR-0002: SQLite + fixed-point decimal money

**Status:** Accepted

## Context
The source app used PostgreSQL with `DECIMAL(12,2)` money and `DECIMAL(12,4)` cost, and a
bcmath `Money` class doing HALF_UP rounding at storage boundaries. The target is a single PC
with no DB server. Money must match the PHP results bit-for-bit (a medical retail system).

## Decision
- Persist in **SQLite** (`QSQLITE`), with type mappings: `SERIAL`→`INTEGER PRIMARY KEY`,
  Postgres `ENUM`→`TEXT + CHECK`, `TIMESTAMPTZ`/`DATE`→ISO-8601 `TEXT`, `BOOLEAN`→`0/1`,
  `JSONB`→`TEXT`.
- Represent money as a **fixed-point decimal** (`domain/Money`): an `int64` scaled to 4
  decimal places internally, serialized as canonical decimal **TEXT** (scale-2 money,
  scale-4 cost), rounded HALF_UP at every storage boundary. **Never `REAL`/float.**

## Consequences
- **+** No floating-point drift; results match the PHP semantics; arithmetic is
  overflow-checked.
- **+** 4-dp cost (which integer-paisa cannot hold) is representable.
- **−** Money columns are TEXT, so you must **never** do arithmetic on them in SQL (SQLite
  would coerce to double). All money math happens in C++ `Money`; values are written as
  strings. (A regression of this rule was fixed — see finding H5.)
- Enforced by `tests/cases/money_tests.cpp` and the INV-7 float-drift invariant.
