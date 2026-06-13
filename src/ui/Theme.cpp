#include "ui/Theme.h"

#include <QApplication>
#include <QFile>

namespace Theme {

void apply(const QString &key)
{
    QString path = QStringLiteral(":/theme.qss");
    if (!key.isEmpty()) {
        const QString candidate = QStringLiteral(":/themes/%1.qss").arg(key);
        if (QFile::exists(candidate)) {
            path = candidate;
        }
    }
    QFile qss(path);
    if (qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qApp->setStyleSheet(QString::fromUtf8(qss.readAll()));
    }
}

} // namespace Theme
