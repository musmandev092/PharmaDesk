// DB-backed + pure tests for the Phase-4 hardware/pharmacology helpers:
//   1) EscPosRenderer — receipt byte-stream (init/cut bytes, header upper-casing,
//      labels, receipt number, non-trivial length). Pure, no DB.
//   2) ZReportArchive — sign + store a CLOSED session's Z-report once
//      (idempotent: second call returns the same signature from the archive).
//   3) AlternativesFinder — same-generic peers (excludes self), on-hand surfaced,
//      no peers for a unique generic.
//
// The module creates its own "TPRINT_"-prefixed medicines / batches / sessions
// with a per-run unique-id counter, and does not depend on other modules.

#include "framework/TestStats.h"

#include "data/BatchRepository.h"
#include "data/EscPosRenderer.h"
#include "data/MedicineRepository.h"
#include "data/SessionRepository.h"
#include "service/AlternativesFinder.h"
#include "service/SaleService.h"
#include "service/ZReportArchive.h"

#include <QByteArray>
#include <QDate>
#include <QSqlQuery>
#include <QString>
#include <QVariant>
#include <QVector>

namespace pharmadesk_tests {

namespace {

// Counter so every SKU / batch number is unique across the whole run.
int g_uniq = 0;

QString uniqueSku()
{
    return QStringLiteral("TPRINT_SKU_%1").arg(++g_uniq, 5, 10, QLatin1Char('0'));
}

QString uniqueBatch()
{
    return QStringLiteral("TPRINT_B_%1").arg(++g_uniq, 5, 10, QLatin1Char('0'));
}

// Create an active medicine with the given generic/brand; returns its id (or -1).
qint64 makeMedicine(QSqlDatabase db, qint64 userId, const QString &generic, const QString &brand)
{
    MedicineRepository repo(db);
    MedicineDraft d;
    d.sku = uniqueSku();
    d.brandName = brand;
    d.genericName = generic;
    d.strength = QStringLiteral("500mg");
    d.form = QStringLiteral("TABLET");
    d.purchaseUnit = QStringLiteral("BOX");
    d.baseUnit = QStringLiteral("TABLET");
    d.unitsPerPurchase = 10;
    d.manufacturer = QStringLiteral("TPrint Labs");
    d.isActive = true;
    return repo.create(d, userId);
}

// Add one far-future batch of stock to a medicine. Returns batch id (or -1).
qint64 addBatch(QSqlDatabase db, qint64 userId, qint64 medId, int qty, const QString &mrp)
{
    BatchRepository repo(db);
    StockInDraft d;
    d.medicineId = medId;
    d.batchNumber = uniqueBatch();
    d.expiry = QDate::currentDate().addYears(2);
    d.quantity = qty;
    d.costPerUnit = QStringLiteral("1.0000");
    d.mrpPerUnit = mrp;
    return repo.addStock(d, userId);
}

} // namespace

TestStats run_printing_tests(QSqlDatabase db, qint64 userId)
{
    TestStats s;
    s.module = QStringLiteral("printing");

    // ====================================================================
    // 1) EscPosRenderer — receipt byte stream (pure, hand-built SaleResult).
    // ====================================================================
    {
        SaleResult sale;
        sale.ok = true;
        sale.saleId = 1;
        sale.receiptNumber = QStringLiteral("INV-TEST-0001");
        sale.subtotal = QStringLiteral("100.00");
        sale.discountTotal = QStringLiteral("0.00");
        sale.grandTotal = QStringLiteral("100.00");
        sale.amountTendered = QStringLiteral("100.00");
        sale.changeReturned = QStringLiteral("0.00");
        sale.paymentMode = QStringLiteral("CASH");

        SaleResultLine ln;
        ln.name = QStringLiteral("Panadol 500mg");
        ln.qty = 2;
        ln.unitLabel = QStringLiteral("TABLET");
        ln.unitMrp = QStringLiteral("50.00");
        ln.lineTotal = QStringLiteral("100.00");
        sale.lines = {ln};

        EscPosHeader header;
        header.pharmacyName = QStringLiteral("city care");
        header.address = QStringLiteral("123 Main Road");
        header.phone = QStringLiteral("042-1234567");
        header.ntn = QStringLiteral("NTN-0001");
        header.cashierName = QStringLiteral("Test Admin");
        header.soldAt = QStringLiteral("09/06/26 14:30");
        header.returnPolicy = QStringLiteral("No returns on opened medicines.");
        header.footerMessage = QStringLiteral("Get well soon!");

        const QByteArray bytes = EscPosRenderer::receipt(sale, header);

        // Starts with ESC @ (init).
        s.check(bytes.startsWith(QByteArray::fromHex("1B40")),
                QStringLiteral("escpos: starts with init bytes ESC @"));
        // Pharmacy name upper-cased.
        s.check(bytes.contains(QByteArray("CITY CARE")),
                QStringLiteral("escpos: pharmacy name upper-cased"));
        // Totals label.
        s.check(bytes.contains(QByteArray("PAYABLE")),
                QStringLiteral("escpos: contains PAYABLE label"));
        // Receipt number echoed.
        s.check(bytes.contains(QByteArray("INV-TEST-0001")),
                QStringLiteral("escpos: contains receipt number"));
        // Ends with the full cut command GS V A <n>.
        s.check(bytes.contains(QByteArray::fromHex("1D564103")),
                QStringLiteral("escpos: contains paper-cut bytes GS V A 0x03"));
        // Non-trivial output.
        s.check(bytes.size() > 100,
                QStringLiteral("escpos: byte stream is non-trivially long (>100)"));
    }

    // ====================================================================
    // 2) ZReportArchive — sign + store a CLOSED session once (idempotent).
    // ====================================================================
    {
        SessionRepository sessions(db);
        const qint64 sessionId = sessions.openSession(userId, QStringLiteral("500.00"));
        s.check(sessionId > 0, QStringLiteral("zreport: session opened"));

        const bool closed = sessions.closeSession(sessionId, QStringLiteral("500.00"), userId);
        s.check(closed, QStringLiteral("zreport: session closed"));

        if (sessionId > 0 && closed) {
            ZReportArchive archive(db, userId);

            // First sign-and-store: freshly computed, persisted.
            const ZReportSignature first = archive.signAndStore(sessionId);
            s.check(first.ok, QStringLiteral("zreport: first signAndStore ok"));
            s.check(first.error.isEmpty(), QStringLiteral("zreport: first signAndStore no error"));
            s.check(first.signatureHex.size() == 64,
                    QStringLiteral("zreport: signature is 64 hex chars (SHA-256)"));
            s.check(!first.fromArchive, QStringLiteral("zreport: first call not from archive"));

            // Second call: idempotent, returned from the archive, identical sig.
            const ZReportSignature second = archive.signAndStore(sessionId);
            s.check(second.ok, QStringLiteral("zreport: second signAndStore ok"));
            s.check(second.fromArchive, QStringLiteral("zreport: second call served from archive"));
            s.check(second.signatureHex == first.signatureHex,
                    QStringLiteral("zreport: idempotent signature identical"));

            // Exactly one archive row for this session.
            QSqlQuery q(db);
            q.prepare(QStringLiteral("SELECT COUNT(*) FROM z_report_archive WHERE session_id = ?"));
            q.addBindValue(sessionId);
            int rows = -1;
            if (q.exec() && q.next()) {
                rows = q.value(0).toInt();
            }
            s.check(rows == 1, QStringLiteral("zreport: exactly one archive row for session"));
        }
    }

    // ====================================================================
    // 3) AlternativesFinder — same-generic peers, on-hand surfaced, none for
    //    a unique generic.
    // ====================================================================
    {
        // Two medicines share generic "Paracetamol"; B holds stock.
        const QString sharedGeneric = QStringLiteral("Paracetamol");
        const qint64 idA = makeMedicine(db, userId, sharedGeneric, QStringLiteral("TPrint Calpol"));
        const qint64 idB
            = makeMedicine(db, userId, sharedGeneric, QStringLiteral("TPrint Panadol"));
        // C has a unique generic — no same-generic peer.
        const QString uniqueGeneric = QStringLiteral("TPrintUniqueGeneric_%1").arg(++g_uniq);
        const qint64 idC = makeMedicine(db, userId, uniqueGeneric, QStringLiteral("TPrint Solo"));
        s.check(idA > 0 && idB > 0 && idC > 0, QStringLiteral("alt: three medicines created"));

        // Give B saleable stock so its on-hand is positive.
        const qint64 batchB = addBatch(db, userId, idB, 60, QStringLiteral("12.00"));
        s.check(batchB > 0, QStringLiteral("alt: B stocked"));

        AlternativesFinder finder(db);

        // Alternatives to A include B (and never A itself).
        const QVector<AlternativeMedicine> altsForA = finder.forMedicine(idA);
        bool foundB = false;
        bool foundSelf = false;
        int onHandB = -1;
        for (const AlternativeMedicine &alt : altsForA) {
            if (alt.id == idB) {
                foundB = true;
                onHandB = alt.onHand;
            }
            if (alt.id == idA) {
                foundSelf = true;
            }
        }
        s.check(foundB, QStringLiteral("alt: A's alternatives include same-generic B"));
        s.check(!foundSelf, QStringLiteral("alt: A's alternatives exclude A itself"));
        s.check(foundB && onHandB > 0, QStringLiteral("alt: B alternative reports onHand > 0"));

        // C has a unique generic → no same-generic peers.
        const QVector<AlternativeMedicine> altsForC = finder.forMedicine(idC);
        s.check(altsForC.isEmpty(), QStringLiteral("alt: unique-generic C has no alternatives"));
    }

    return s;
}

} // namespace pharmadesk_tests
