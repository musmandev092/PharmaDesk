#include "ui/ActivationDialog.h"

#include "Branding.h"
#include "domain/Licensing.h"

#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
// A human note for the state the app is in when activation is required.
QString stateNote(const QString &state)
{
    if (state == QLatin1String("expired")) {
        return QStringLiteral(
            "This computer's license has expired. Enter a renewed license below.");
    }
    if (state == QLatin1String("wrong_machine")) {
        return QStringLiteral(
            "The installed license is for a different computer. Activate this one below.");
    }
    if (state == QLatin1String("bad_signature") || state == QLatin1String("invalid")) {
        return QStringLiteral("The installed license is invalid. Enter a valid license below.");
    }
    return QStringLiteral("This copy of %1 must be activated for this computer.")
        .arg(Branding::productName());
}
} // namespace

ActivationDialog::ActivationDialog(const QString &initialState, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(Branding::productName() + QStringLiteral(" — Activation"));
    setModal(true);
    setMinimumWidth(560);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 20);
    root->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("Activate %1").arg(Branding::productName()), this);
    title->setObjectName(QStringLiteral("h1"));
    root->addWidget(title);

    auto *note = new QLabel(stateNote(initialState), this);
    note->setObjectName(QStringLiteral("muted"));
    note->setWordWrap(true);
    root->addWidget(note);

    auto *steps
        = new QLabel(QStringLiteral("1. Send the activation request below to your vendor.\n"
                                    "2. They send back a license file.\n"
                                    "3. Load it with “Load license file…” (or paste it) and click "
                                    "Activate."),
                     this);
    steps->setObjectName(QStringLiteral("muted"));
    root->addWidget(steps);

    auto *code = new QLabel(
        QStringLiteral("This computer's code:  <b>%1</b>").arg(Licensing::currentCode()), this);
    code->setTextFormat(Qt::RichText);
    code->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(code);

    root->addWidget(
        new QLabel(QStringLiteral("Activation request (send this to your vendor):"), this));
    m_request = new QPlainTextEdit(Licensing::buildRequest(), this);
    m_request->setReadOnly(true);
    m_request->setFixedHeight(72);
    m_request->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 11px;"));
    root->addWidget(m_request);

    auto *reqBtns = new QHBoxLayout;
    auto *copy = new QPushButton(QStringLiteral("Copy request"), this);
    copy->setProperty("variant", "secondary");
    auto *saveReq = new QPushButton(QStringLiteral("Save request…"), this);
    saveReq->setProperty("variant", "secondary");
    reqBtns->addWidget(copy);
    reqBtns->addWidget(saveReq);
    reqBtns->addStretch();
    root->addLayout(reqBtns);

    root->addWidget(new QLabel(
        QStringLiteral("License from your vendor (load a file, or paste it here):"), this));
    m_license = new QPlainTextEdit(this);
    m_license->setPlaceholderText(
        QStringLiteral("Paste the license text here, or use “Load license file…”"));
    m_license->setFixedHeight(96);
    m_license->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 11px;"));
    root->addWidget(m_license);

    auto *bottom = new QHBoxLayout;
    auto *load = new QPushButton(QStringLiteral("Load license file…"), this);
    load->setProperty("variant", "secondary");
    bottom->addWidget(load);
    bottom->addStretch();
    auto *quit = new QPushButton(QStringLiteral("Quit"), this);
    auto *activate = new QPushButton(QStringLiteral("Activate"), this);
    activate->setDefault(true);
    bottom->addWidget(quit);
    bottom->addWidget(activate);
    root->addLayout(bottom);

    connect(copy, &QPushButton::clicked, this, &ActivationDialog::copyRequest);
    connect(saveReq, &QPushButton::clicked, this, &ActivationDialog::saveRequest);
    connect(load, &QPushButton::clicked, this, &ActivationDialog::loadFile);
    connect(activate, &QPushButton::clicked, this, &ActivationDialog::activate);
    connect(quit, &QPushButton::clicked, this, &QDialog::reject);
}

void ActivationDialog::copyRequest()
{
    QApplication::clipboard()->setText(m_request->toPlainText());
}

void ActivationDialog::saveRequest()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save activation request"), QStringLiteral("pharmadesk-request.txt"),
        QStringLiteral("Text (*.txt)"));
    if (path.isEmpty()) {
        return;
    }
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        f.write(m_request->toPlainText().toUtf8());
    }
}

void ActivationDialog::loadFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Load license file"), QString(),
        QStringLiteral("License (*.lic *.txt *.json);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_license->setPlainText(QString::fromUtf8(f.readAll()));
    }
}

void ActivationDialog::activate()
{
    const Licensing::InstallResult r = Licensing::installLicense(m_license->toPlainText());
    if (r.ok) {
        QMessageBox::information(this, Branding::productName(), QStringLiteral("Activated."));
        accept();
    } else {
        QMessageBox::warning(this, QStringLiteral("Activation failed"), r.message);
    }
}
