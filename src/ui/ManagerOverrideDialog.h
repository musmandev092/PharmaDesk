#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;

// Reusable modal that captures a manager/admin PIN (and, when needed, a
// prescriber license number) to authorize a gated checkout action — applying a
// discount as a cashier, or witnessing a controlled-substance dispense. The
// caller verifies the PIN against the user repository; this dialog only collects
// the inputs.
class ManagerOverrideDialog : public QDialog
{
    Q_OBJECT
public:
    ManagerOverrideDialog(const QString &title, const QString &message, bool needLicense,
                          QWidget *parent = nullptr);

    QString pin() const;
    QString license() const; // empty when the dialog was built without needLicense

private:
    QLineEdit *m_license = nullptr; // null unless needLicense was true
    QLineEdit *m_pin = nullptr;
};
