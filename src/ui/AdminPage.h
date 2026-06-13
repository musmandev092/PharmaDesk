#pragma once

#include "data/UserRepository.h"

#include <QSqlDatabase>
#include <QWidget>

class QTableWidget;
class QLineEdit;
class QCheckBox;
class QLabel;
class QTabWidget;
class QPlainTextEdit;
class QComboBox;

// Admin & ops screen: Pharmacy settings, Users (admin-only management), Audit
// log viewer, Backup settings + Backup now, and Change-my-PIN.
class AdminPage : public QWidget
{
    Q_OBJECT
public:
    AdminPage(QSqlDatabase db, const UserRecord &user, QWidget *parent = nullptr);

    void reload();

private slots:
    void addUser();
    void editUser();
    void changeMyPin();
    void chooseBackupDir();
    void backupNow();
    void saveBackupSettings();
    void savePharmacy();
    void savePrinter();
    void refreshPrinterStatus();
    void testThermalPrint();
    void changeLogo();

private:
    qint64 selectedUserId() const;
    void reloadUsers();
    void reloadAudit();
    void loadBackupSettings();
    void loadPharmacy();
    void loadPrinter();

    // UI construction, split out of the constructor (one tab each).
    QWidget *buildPharmacyTab();
    QWidget *buildPrinterTab();
    QWidget *buildUsersTab();
    QWidget *buildAuditTab();
    QWidget *buildBackupTab();

    QSqlDatabase m_db;
    UserRecord m_user;
    bool m_isAdmin = false;

    // Pharmacy settings
    QLineEdit *m_phName = nullptr;
    QPlainTextEdit *m_phAddress = nullptr;
    QLineEdit *m_phPhone = nullptr;
    QLineEdit *m_phNtn = nullptr;
    QPlainTextEdit *m_phPolicy = nullptr;
    QLineEdit *m_phFooter = nullptr;
    QComboBox *m_theme = nullptr;
    QLabel *m_logoPreview = nullptr;
    QString m_logoPath;

    // Receipt printer (thermal / ESC-POS via CUPS) — dedicated Printer tab.
    QCheckBox *m_thermalEnabled = nullptr;
    QComboBox *m_thermalQueue = nullptr; // editable: detected queues + free text
    QLabel *m_cupsStatus = nullptr;
    QLabel *m_driverStatus = nullptr;
    QLabel *m_queueStatus = nullptr;
    QTableWidget *m_usbPrinters = nullptr;
    QLabel *m_usbEmpty = nullptr;

    QTableWidget *m_users = nullptr;
    QTableWidget *m_audit = nullptr;
    QLineEdit *m_backupDir = nullptr;
    QCheckBox *m_autoBackup = nullptr;
    QLabel *m_lastBackup = nullptr;
};
