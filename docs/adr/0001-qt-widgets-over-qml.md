# ADR-0001: Qt Widgets (not QML) for the UI

**Status:** Accepted

## Context
PharmaDesk is a data-dense desktop POS (tables, forms, dialogs, printing) for a single
AlmaLinux PC, ported from a web app. We need a native toolkit with strong table/forms,
printing, and a small runtime footprint.

## Decision
Use **Qt 6 Widgets**, not QML/Qt Quick.

## Consequences
- **+** Mature table/model views (`QTableView`, `QSqlTableModel`), native printing
  (`PrintSupport`), straightforward keyboard/mouse desktop UX, smaller dependency surface.
- **+** Styling via a single app-wide QSS (`resources/theme.qss`) reusing the web app's
  design tokens.
- **−** Widgets render differently from HTML/CSS, so visual parity with the web app is
  "faithful, not pixel-perfect" (accepted in CLAUDE.md).
- QML would suit touch/animation-heavy UIs, which this is not.
