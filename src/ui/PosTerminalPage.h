#pragma once

#include <QSqlDatabase>
#include <QVector>
#include <QWidget>

class QLineEdit;
class QTableWidget;
class QComboBox;
class QLabel;
class QPushButton;

// POS terminal: search a medicine → add to cart → checkout → receipt. Stock is
// decremented via SaleService (FEFO, audited, transactional). Supports selling
// by base unit or by pack (factor = units_per_purchase), park/resume of bills,
// and barcode-scan search.
class PosTerminalPage : public QWidget
{
    Q_OBJECT
public:
    PosTerminalPage(QSqlDatabase db, qint64 userId, QWidget *parent = nullptr);

private slots:
    void runSearch();
    void addUnit(); // add the selected result as a single base unit
    void addPack(); // add the selected result as one purchase pack
    void recompute();
    void completeSale();
    void clearCart();
    void parkBill();
    void resumeBill();
    void showAlternatives();

private:
    struct CartLine
    {
        qint64 medicineId;
        QString name;
        QString unitMrp;   // decimal string, per BASE unit
        int qty;           // count of the sold unit (base unit or pack)
        int factor;        // base units per sold unit (1 = base, N = pack)
        QString unitLabel; // display label for the sold unit
    };

    void addSelected(bool asPack);
    void renderCart();

    QSqlDatabase m_db;
    qint64 m_userId;
    QString m_role; // role of the logged-in cashier (drives discount self-auth)

    QLineEdit *m_search = nullptr;
    QTableWidget *m_results = nullptr;
    QPushButton *m_alternatives = nullptr;
    QTableWidget *m_cart = nullptr;
    QLabel *m_subtotal = nullptr;
    QLineEdit *m_discount = nullptr;
    QLabel *m_grand = nullptr;
    QComboBox *m_payment = nullptr;
    QLineEdit *m_tendered = nullptr;
    QLabel *m_change = nullptr;
    QPushButton *m_complete = nullptr;

    QVector<CartLine> m_cartLines;
};
