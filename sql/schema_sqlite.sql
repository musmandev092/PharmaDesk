-- =============================================================================
-- PharmaDesk — SQLite schema (port of ../db/schema/init.sql, Postgres 17)
-- =============================================================================
-- Run once on a fresh database (Database::bootstrap). Type mapping per CLAUDE.md:
--   SERIAL / BIGSERIAL  -> INTEGER PRIMARY KEY AUTOINCREMENT
--   Postgres ENUM       -> TEXT + CHECK (col IN (...))
--   TIMESTAMPTZ / DATE  -> TEXT (ISO-8601; CURRENT_TIMESTAMP defaults)
--   DECIMAL(12,2|4)     -> TEXT (decimal string; money is scale-2/cost scale-4,
--                          see domain Money port — never REAL/float)
--   BOOLEAN             -> INTEGER (0/1)
--   JSONB               -> TEXT (JSON)
-- Foreign keys require `PRAGMA foreign_keys = ON` (set by Database on open).
-- =============================================================================

-- ── branches ────────────────────────────────────────────────────────────────
CREATE TABLE "branches" (
    "id"         INTEGER PRIMARY KEY AUTOINCREMENT,
    "name"       TEXT    NOT NULL,
    "address"    TEXT,
    "phone"      TEXT,
    "is_active"  INTEGER NOT NULL DEFAULT 1,
    "created_at" TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updated_at" TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- ── users ─────────────────────────────────────────────────────────────────
CREATE TABLE "users" (
    "id"              INTEGER PRIMARY KEY AUTOINCREMENT,
    "full_name"       TEXT    NOT NULL,
    "username"        TEXT    NOT NULL COLLATE NOCASE,
    "pin_hash"        TEXT    NOT NULL,
    "password_hash"   TEXT,
    "role"            TEXT    NOT NULL CHECK ("role" IN ('ADMIN','MANAGER','CASHIER')),
    "is_active"       INTEGER NOT NULL DEFAULT 1,
    "last_login_at"   TEXT,
    "pin_changed_at"  TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "must_rotate_pin" INTEGER NOT NULL DEFAULT 0,
    "created_at"      TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updated_at"      TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "deleted_at"      TEXT,
    "branch_id"       INTEGER NOT NULL DEFAULT 1,
    FOREIGN KEY ("branch_id") REFERENCES "branches"("id") ON DELETE RESTRICT ON UPDATE CASCADE
);
CREATE UNIQUE INDEX "users_username_key"        ON "users"("username");
CREATE        INDEX "users_role_active_deleted" ON "users"("role","is_active","deleted_at");

-- Phase 4: PIN history (no repeat of last 5 per user).
CREATE TABLE "pin_history" (
    "id"         INTEGER PRIMARY KEY AUTOINCREMENT,
    "user_id"    INTEGER NOT NULL,
    "pin_hash"   TEXT    NOT NULL,
    "changed_at" TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY ("user_id") REFERENCES "users"("id") ON DELETE CASCADE
);
CREATE INDEX "pin_history_user_changed_idx" ON "pin_history"("user_id","changed_at" DESC);

-- ── suppliers ───────────────────────────────────────────────────────────────
CREATE TABLE "suppliers" (
    "id"            INTEGER PRIMARY KEY AUTOINCREMENT,
    "name"          TEXT    NOT NULL,
    "contact_phone" TEXT,
    "ntn"           TEXT,
    "address"       TEXT,
    "booker_name"   TEXT,
    "salesman_name" TEXT,
    "payment_terms" TEXT    DEFAULT 'CASH',
    "notes"         TEXT,
    "is_active"     INTEGER NOT NULL DEFAULT 1,
    "created_at"    TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updated_at"    TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "deleted_at"    TEXT
);
CREATE INDEX "suppliers_name_active_deleted" ON "suppliers"("name","is_active","deleted_at");

-- ── medicines ───────────────────────────────────────────────────────────────
-- MedicineForm enum (~55 values) is enforced in the C++ domain layer rather than
-- a CHECK list here, to keep the schema readable and the form list in one place.
CREATE TABLE "medicines" (
    "id"                    INTEGER PRIMARY KEY AUTOINCREMENT,
    "sku"                   TEXT    NOT NULL,
    "primary_barcode"       TEXT,
    "brand_name"            TEXT    NOT NULL,
    "generic_name"          TEXT    NOT NULL,
    "strength"              TEXT,
    "form"                  TEXT    NOT NULL,
    "form_custom"           TEXT,
    "manufacturer"          TEXT,
    "therapeutic_category"  TEXT,
    "purchase_unit"         TEXT    NOT NULL,
    "base_unit"             TEXT    NOT NULL,
    "units_per_purchase"    INTEGER NOT NULL,
    "prescription_required" INTEGER NOT NULL DEFAULT 0,
    "controlled_schedule"   TEXT    NOT NULL DEFAULT 'NONE'
                                CHECK ("controlled_schedule" IN ('NONE','SCHEDULE_G','SCHEDULE_H','NARCOTIC')),
    "tax_code_value"        TEXT    NOT NULL DEFAULT 'EXEMPT'
                                CHECK ("tax_code_value" IN ('EXEMPT','STANDARD_18','REDUCED','ZERO_RATED')),
    "reorder_level"         INTEGER DEFAULT 0,
    "reorder_quantity"      INTEGER DEFAULT 0,
    "reorder_unit"          TEXT    NOT NULL DEFAULT 'PURCHASE',
    "is_active"             INTEGER NOT NULL DEFAULT 1,
    "created_at"            TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updated_at"            TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "deleted_at"            TEXT
);
-- Partial-unique (live rows only), mirroring db/triggers/phase8_medicines_partial_unique.sql:
-- soft-deleted rows may keep their old sku/barcode so history resolves, while
-- live duplicates are still rejected.
CREATE UNIQUE INDEX "medicines_sku_key"            ON "medicines"("sku")             WHERE "deleted_at" IS NULL;
CREATE UNIQUE INDEX "medicines_primary_barcode_key" ON "medicines"("primary_barcode") WHERE "deleted_at" IS NULL AND "primary_barcode" IS NOT NULL;
CREATE        INDEX "medicines_active_deleted"     ON "medicines"("is_active","deleted_at");

-- ── batches ─────────────────────────────────────────────────────────────────
CREATE TABLE "batches" (
    "id"              INTEGER PRIMARY KEY AUTOINCREMENT,
    "branch_id"       INTEGER NOT NULL DEFAULT 1,
    "medicine_id"     INTEGER NOT NULL,
    "supplier_id"     INTEGER,
    "grn_id"          INTEGER,
    "batch_number"    TEXT    NOT NULL,
    "distributor_ref" TEXT,
    "expiry_date"     TEXT    NOT NULL,
    "received_qty"    INTEGER NOT NULL,
    "foc_qty"         INTEGER NOT NULL DEFAULT 0,
    "current_qty"     INTEGER NOT NULL,
    "cost_per_unit"   TEXT    NOT NULL,
    "mrp_per_unit"    TEXT    NOT NULL,
    "is_quarantined"  INTEGER NOT NULL DEFAULT 0,
    "is_expired"      INTEGER NOT NULL DEFAULT 0,
    "notes"           TEXT,
    "created_at"      TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updated_at"      TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT "batch_qty_nonneg"          CHECK ("current_qty"  >= 0),
    CONSTRAINT "batch_received_qty_nonneg" CHECK ("received_qty" >= 0),
    CONSTRAINT "batch_foc_qty_nonneg"      CHECK ("foc_qty"      >= 0),
    FOREIGN KEY ("branch_id")   REFERENCES "branches"("id")      ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY ("medicine_id") REFERENCES "medicines"("id")     ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY ("supplier_id") REFERENCES "suppliers"("id")     ON DELETE SET NULL ON UPDATE CASCADE,
    FOREIGN KEY ("grn_id")      REFERENCES "grn_documents"("id") ON DELETE SET NULL ON UPDATE CASCADE
);
CREATE        INDEX "batches_medicine_expiry" ON "batches"("medicine_id","expiry_date");
CREATE        INDEX "batches_expiry"          ON "batches"("expiry_date");
CREATE UNIQUE INDEX "batches_med_batch_expiry_key" ON "batches"("medicine_id","batch_number","expiry_date");

-- ── grn_documents ─────────────────────────────────────────────────────────
CREATE TABLE "grn_documents" (
    "id"                 INTEGER PRIMARY KEY AUTOINCREMENT,
    "branch_id"          INTEGER NOT NULL DEFAULT 1,
    "grn_number"         TEXT    NOT NULL,
    "supplier_id"        INTEGER NOT NULL,
    "invoice_number"     TEXT,
    "invoice_date"       TEXT,
    "invoice_image_path" TEXT,
    "received_by"        INTEGER NOT NULL,
    "posted_by"          INTEGER,
    "posted_at"          TEXT,
    "subtotal"           TEXT    NOT NULL DEFAULT '0',
    "tax_total"          TEXT    NOT NULL DEFAULT '0',
    "discount_total"     TEXT    NOT NULL DEFAULT '0',
    "grand_total"        TEXT    NOT NULL DEFAULT '0',
    "status"             TEXT    NOT NULL DEFAULT 'DRAFT'
                             CHECK ("status" IN ('DRAFT','POSTED','CANCELLED')),
    "notes"              TEXT,
    "created_at"         TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updated_at"         TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY ("branch_id")   REFERENCES "branches"("id")  ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY ("supplier_id") REFERENCES "suppliers"("id") ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY ("received_by") REFERENCES "users"("id")     ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY ("posted_by")   REFERENCES "users"("id")     ON DELETE SET NULL ON UPDATE CASCADE
);
CREATE UNIQUE INDEX "grn_documents_number_key"      ON "grn_documents"("grn_number");
CREATE        INDEX "grn_documents_supplier_invdate" ON "grn_documents"("supplier_id","invoice_date");
CREATE        INDEX "grn_documents_status"          ON "grn_documents"("status");

CREATE TABLE "grn_lines" (
    "id"                INTEGER PRIMARY KEY AUTOINCREMENT,
    "grn_id"            INTEGER NOT NULL,
    "medicine_id"       INTEGER NOT NULL,
    "batch_id"          INTEGER,
    "batch_number"      TEXT    NOT NULL,
    "expiry_date"       TEXT    NOT NULL,
    "paid_qty"          INTEGER NOT NULL,
    "foc_qty"           INTEGER NOT NULL DEFAULT 0,
    "qty_in_base_units" INTEGER NOT NULL,
    "unit_cost"         TEXT    NOT NULL,
    "mrp_per_base_unit" TEXT    NOT NULL,
    "line_total"        TEXT    NOT NULL,
    "created_at"        TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY ("grn_id")      REFERENCES "grn_documents"("id") ON DELETE CASCADE  ON UPDATE CASCADE,
    FOREIGN KEY ("medicine_id") REFERENCES "medicines"("id")     ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY ("batch_id")    REFERENCES "batches"("id")       ON DELETE SET NULL ON UPDATE CASCADE
);
CREATE INDEX "grn_lines_grn_id_idx" ON "grn_lines"("grn_id");

-- ── sales ───────────────────────────────────────────────────────────────────
CREATE TABLE "sales" (
    "id"                        INTEGER PRIMARY KEY AUTOINCREMENT,
    "branch_id"                 INTEGER NOT NULL DEFAULT 1,
    "receipt_number"            TEXT    NOT NULL,
    "cashier_id"                INTEGER NOT NULL,
    "cashier_session_id"        INTEGER,
    "customer_name"             TEXT,
    "customer_phone"            TEXT,
    "customer_remarks"          TEXT,
    "has_controlled_drug"       INTEGER NOT NULL DEFAULT 0,
    "doctor_name"               TEXT,
    "prescriber_license_number" TEXT,
    "narcotic_witness_user_id"  INTEGER,
    "narcotic_witness_at"       TEXT,
    "patient_name"              TEXT,
    "patient_phone"             TEXT,
    "patient_address"           TEXT,
    "prescription_image_path"   TEXT,
    "subtotal"                  TEXT    NOT NULL,
    "tax_total"                 TEXT    NOT NULL DEFAULT '0',
    "discount_total"            TEXT    NOT NULL DEFAULT '0',
    "discount_authorized_by"    INTEGER,
    "pos_service_fee"           TEXT    NOT NULL DEFAULT '0',
    "grand_total"               TEXT    NOT NULL,
    "amount_tendered"           TEXT,
    "change_returned"           TEXT,
    "payment_mode"              TEXT    NOT NULL DEFAULT 'CASH'
                                    CHECK ("payment_mode" IN ('CASH','CARD','OTHER')),
    "status"                    TEXT    NOT NULL DEFAULT 'COMPLETED'
                                    CHECK ("status" IN ('COMPLETED','VOIDED','REFUNDED_PARTIAL','REFUNDED_FULL')),
    "sold_at"                   TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "created_at"                TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY ("branch_id")              REFERENCES "branches"("id")        ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY ("cashier_id")             REFERENCES "users"("id")           ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY ("cashier_session_id")     REFERENCES "cashier_sessions"("id") ON DELETE SET NULL ON UPDATE CASCADE,
    FOREIGN KEY ("discount_authorized_by") REFERENCES "users"("id")           ON DELETE SET NULL ON UPDATE CASCADE
);
CREATE UNIQUE INDEX "sales_receipt_number_key"   ON "sales"("receipt_number");
CREATE        INDEX "sales_sold_at"              ON "sales"("sold_at");
CREATE        INDEX "sales_cashier_sold_at"      ON "sales"("cashier_id","sold_at");
CREATE        INDEX "sales_customer_phone"       ON "sales"("customer_phone");
CREATE        INDEX "sales_status"               ON "sales"("status");

CREATE TABLE "sale_items" (
    "id"                INTEGER PRIMARY KEY AUTOINCREMENT,
    "sale_id"           INTEGER NOT NULL,
    "medicine_id"       INTEGER NOT NULL,
    "batch_id"          INTEGER NOT NULL,
    "qty_in_base_units" INTEGER NOT NULL,
    "sold_unit_label"   TEXT    NOT NULL,
    "sold_unit_factor"  INTEGER NOT NULL DEFAULT 1,
    "qty_sold_display"  INTEGER NOT NULL,
    "unit_cost"         TEXT    NOT NULL,
    "unit_mrp"          TEXT    NOT NULL,
    "line_subtotal"     TEXT    NOT NULL,
    "line_tax"          TEXT    NOT NULL DEFAULT '0',
    "line_discount"     TEXT    NOT NULL DEFAULT '0',
    "line_total"        TEXT    NOT NULL,
    "created_at"        TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY ("sale_id")     REFERENCES "sales"("id")     ON DELETE CASCADE  ON UPDATE CASCADE,
    FOREIGN KEY ("medicine_id") REFERENCES "medicines"("id") ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY ("batch_id")    REFERENCES "batches"("id")   ON DELETE RESTRICT ON UPDATE CASCADE
);
CREATE INDEX "sale_items_sale_id"     ON "sale_items"("sale_id");
CREATE INDEX "sale_items_medicine_id" ON "sale_items"("medicine_id");
CREATE INDEX "sale_items_batch_id"    ON "sale_items"("batch_id");

-- ── returns ─────────────────────────────────────────────────────────────────
CREATE TABLE "returns" (
    "id"                 INTEGER PRIMARY KEY AUTOINCREMENT,
    "branch_id"          INTEGER NOT NULL DEFAULT 1,
    "return_number"      TEXT    NOT NULL,
    "original_sale_id"   INTEGER NOT NULL,
    "sale_item_id"       INTEGER NOT NULL,
    "medicine_id"        INTEGER NOT NULL,
    "batch_id"           INTEGER NOT NULL,
    "qty_returned_units" INTEGER NOT NULL,
    "refund_amount"      TEXT    NOT NULL,
    "reason"             TEXT    NOT NULL
                             CHECK ("reason" IN ('CUSTOMER_CHANGED_MIND','DAMAGED','WRONG_ITEM','ADVERSE_REACTION','EXPIRED','OTHER')),
    "physical_condition" TEXT,
    "initiated_by"       INTEGER NOT NULL,
    "authorized_by"      INTEGER,
    "status"             TEXT    NOT NULL DEFAULT 'PENDING_REVIEW'
                             CHECK ("status" IN ('PENDING_REVIEW','APPROVED_RESTOCK','RETURN_TO_SUPPLIER','WRITE_OFF')),
    "adjudicated_by"     INTEGER,
    "adjudicated_at"     TEXT,
    "adjudication_notes" TEXT,
    "created_at"         TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updated_at"         TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY ("branch_id")        REFERENCES "branches"("id")    ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY ("original_sale_id") REFERENCES "sales"("id")       ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY ("sale_item_id")     REFERENCES "sale_items"("id")  ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY ("medicine_id")      REFERENCES "medicines"("id")   ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY ("batch_id")         REFERENCES "batches"("id")     ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY ("initiated_by")     REFERENCES "users"("id")       ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY ("authorized_by")    REFERENCES "users"("id")       ON DELETE SET NULL ON UPDATE CASCADE,
    FOREIGN KEY ("adjudicated_by")   REFERENCES "users"("id")       ON DELETE SET NULL ON UPDATE CASCADE
);
CREATE UNIQUE INDEX "returns_return_number_key" ON "returns"("return_number");
CREATE        INDEX "returns_status"            ON "returns"("status");
CREATE        INDEX "returns_original_sale_id"  ON "returns"("original_sale_id");

-- ── stock_adjustments ─────────────────────────────────────────────────────
CREATE TABLE "stock_adjustments" (
    "id"                INTEGER PRIMARY KEY AUTOINCREMENT,
    "branch_id"         INTEGER NOT NULL DEFAULT 1,
    "adjustment_number" TEXT    NOT NULL,
    "medicine_id"       INTEGER NOT NULL,
    "batch_id"          INTEGER NOT NULL,
    "qty_delta"         INTEGER NOT NULL,
    "reason"            TEXT    NOT NULL
                            CHECK ("reason" IN ('DAMAGE','EXPIRY_WRITEOFF','SHRINKAGE','COUNT_CORRECTION','SAMPLE','DONATION','OTHER')),
    "notes"             TEXT,
    "performed_by"      INTEGER NOT NULL,
    "cost_impact"       TEXT,
    "created_at"        TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY ("branch_id")    REFERENCES "branches"("id")  ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY ("medicine_id")  REFERENCES "medicines"("id") ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY ("batch_id")     REFERENCES "batches"("id")   ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY ("performed_by") REFERENCES "users"("id")     ON DELETE RESTRICT ON UPDATE CASCADE
);
CREATE UNIQUE INDEX "stock_adjustments_number_key" ON "stock_adjustments"("adjustment_number");
CREATE        INDEX "stock_adjustments_created_at" ON "stock_adjustments"("created_at");
CREATE        INDEX "stock_adjustments_batch_id"   ON "stock_adjustments"("batch_id");

-- ── inventory_movements (stock ledger) ─────────────────────────────────────
CREATE TABLE "inventory_movements" (
    "id"            INTEGER PRIMARY KEY AUTOINCREMENT,
    "branch_id"     INTEGER NOT NULL DEFAULT 1,
    "medicine_id"   INTEGER NOT NULL,
    "batch_id"      INTEGER NOT NULL,
    "movement_type" TEXT    NOT NULL
                        CHECK ("movement_type" IN ('GRN_RECEIPT','SALE','RETURN_QUARANTINE','RETURN_RESTOCK','ADJUSTMENT','WRITE_OFF','EXPIRY_BLOCK')),
    "qty_delta"     INTEGER NOT NULL,
    "qty_before"    INTEGER NOT NULL,
    "qty_after"     INTEGER NOT NULL,
    "ref_table"     TEXT,
    "ref_id"        INTEGER,
    "performed_by"  INTEGER,
    "created_at"    TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY ("branch_id")    REFERENCES "branches"("id")  ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY ("medicine_id")  REFERENCES "medicines"("id") ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY ("batch_id")     REFERENCES "batches"("id")   ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY ("performed_by") REFERENCES "users"("id")     ON DELETE SET NULL ON UPDATE CASCADE
);
CREATE INDEX "inv_movements_batch_created"    ON "inventory_movements"("batch_id","created_at");
CREATE INDEX "inv_movements_medicine_created" ON "inventory_movements"("medicine_id","created_at");
CREATE INDEX "inv_movements_ref"              ON "inventory_movements"("ref_table","ref_id");

-- ── cashier_sessions ─────────────────────────────────────────────────────
CREATE TABLE "cashier_sessions" (
    "id"                      INTEGER PRIMARY KEY AUTOINCREMENT,
    "branch_id"               INTEGER NOT NULL DEFAULT 1,
    "cashier_id"              INTEGER NOT NULL,
    "opened_at"               TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "closed_at"               TEXT,
    "opening_float"           TEXT    NOT NULL DEFAULT '0',
    "expected_cash_in_drawer" TEXT,
    "counted_cash"            TEXT,
    "cash_variance"           TEXT,
    "total_cash_sales"        TEXT    NOT NULL DEFAULT '0',
    "total_refunds_paid"      TEXT    NOT NULL DEFAULT '0',
    "total_sales_count"       INTEGER NOT NULL DEFAULT 0,
    "status"                  TEXT    NOT NULL DEFAULT 'OPEN'
                                  CHECK ("status" IN ('OPEN','CLOSED','RECONCILED')),
    "z_report_pdf_path"       TEXT,
    "notes"                   TEXT,
    "created_at"              TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updated_at"              TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY ("branch_id")  REFERENCES "branches"("id") ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY ("cashier_id") REFERENCES "users"("id")    ON DELETE RESTRICT ON UPDATE CASCADE
);
CREATE INDEX "cashier_sessions_cashier_opened" ON "cashier_sessions"("cashier_id","opened_at");
CREATE INDEX "cashier_sessions_status"         ON "cashier_sessions"("status");

-- ── audit_log ───────────────────────────────────────────────────────────────
-- Append-only is enforced in the C++ Audit layer (no UPDATE/DELETE) rather than
-- the Postgres trigger; prev_hmac/row_hmac kept for parity (Phase 6).
CREATE TABLE "audit_log" (
    "id"           INTEGER PRIMARY KEY AUTOINCREMENT,
    "timestamp"    TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "user_id"      INTEGER,
    "action_type"  TEXT    NOT NULL,
    "entity_type"  TEXT    NOT NULL,
    "entity_id"    INTEGER,
    "before_value" TEXT,
    "after_value"  TEXT,
    "reason"       TEXT,
    "ip_address"   TEXT,
    "user_agent"   TEXT,
    "prev_hmac"    TEXT    NOT NULL DEFAULT '',
    "row_hmac"     TEXT    NOT NULL DEFAULT ''
);
CREATE INDEX "audit_log_timestamp"       ON "audit_log"("timestamp" DESC);
CREATE INDEX "audit_log_user_timestamp"  ON "audit_log"("user_id","timestamp" DESC);
CREATE INDEX "audit_log_entity"          ON "audit_log"("entity_type","entity_id");
CREATE INDEX "audit_log_action"          ON "audit_log"("action_type");

-- Append-only enforcement (port of db/triggers/audit_log_immutable.sql). The
-- Postgres version blocks UPDATE/DELETE and chains an HMAC; on single-PC SQLite
-- we keep the immutability guarantee with BEFORE triggers that RAISE(ABORT).
-- The HMAC chain (prev_hmac/row_hmac) is deferred to the Phase-6 Audit port.
CREATE TRIGGER "audit_log_no_update" BEFORE UPDATE ON "audit_log"
BEGIN
    SELECT RAISE(ABORT, 'audit_log is append-only: UPDATE is not allowed');
END;
CREATE TRIGGER "audit_log_no_delete" BEFORE DELETE ON "audit_log"
BEGIN
    SELECT RAISE(ABORT, 'audit_log is append-only: DELETE is not allowed');
END;

-- ── settings ─────────────────────────────────────────────────────────────
CREATE TABLE "settings" (
    "key"         TEXT PRIMARY KEY,
    "value"       TEXT NOT NULL,
    "description" TEXT,
    "updated_at"  TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updated_by"  INTEGER,
    FOREIGN KEY ("updated_by") REFERENCES "users"("id") ON DELETE SET NULL ON UPDATE CASCADE
);

-- ── rate_limits ─────────────────────────────────────────────────────────
CREATE TABLE "rate_limits" (
    "key"      TEXT PRIMARY KEY,
    "count"    INTEGER NOT NULL DEFAULT 1,
    "reset_at" TEXT    NOT NULL
);

-- ── parked_carts (suspended POS bills; native to the desktop port) ─────────
CREATE TABLE "parked_carts" (
    "id"          INTEGER PRIMARY KEY AUTOINCREMENT,
    "branch_id"   INTEGER DEFAULT 1,
    "cashier_id"  INTEGER,
    "label"       TEXT,
    "items_json"  TEXT    NOT NULL,
    "created_at"  TEXT    DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX "idx_parked_carts_cashier" ON "parked_carts"("cashier_id", "created_at" DESC);

-- ── seed: default branch (every table defaults branch_id = 1) ──────────────
-- The first-run wizard updates this row's name to the operator's pharmacy.
INSERT INTO "branches" ("id","name","is_active") VALUES (1, 'Main', 1)
    ON CONFLICT ("id") DO NOTHING;
