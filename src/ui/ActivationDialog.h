#pragma once

#include <QDialog>

class QPlainTextEdit;

// Shown at startup only when the app is licensed-but-not-activated (gated by
// Licensing::enforced()). The user copies the activation request to the vendor and
// loads back the license file the vendor sends. The app stays locked until a valid
// license for THIS machine is installed (Activate) or the user Quits.
class ActivationDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ActivationDialog(const QString &initialState = QStringLiteral("unactivated"),
                              QWidget *parent = nullptr);

private slots:
    void copyRequest();
    void saveRequest();
    void loadFile();
    void activate();

private:
    QPlainTextEdit *m_request = nullptr;
    QPlainTextEdit *m_license = nullptr;
};
