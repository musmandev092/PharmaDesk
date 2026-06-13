#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QStringList>

// Tiny assertion accumulator shared by every test module. Each module function
// has the signature:
//
//     pharmadesk_tests::TestStats run_<module>_tests(QSqlDatabase db, qint64 userId);
//
// and returns its own stats. The runner (run_all.cpp) calls them all and
// aggregates. `db` is an open, bootstrapped SQLite connection in test mode;
// `userId` is a seeded ADMIN. Modules must create their OWN data with unique
// identifiers and must not assume a clean database or depend on other modules.
namespace pharmadesk_tests {

struct TestStats
{
    QString module;
    int passed = 0;
    int failed = 0;
    QStringList failures; // "module: case name" for each failure

    void check(bool cond, const QString &name)
    {
        if (cond) {
            ++passed;
        } else {
            ++failed;
            failures << (module + QStringLiteral(": ") + name);
        }
    }
};

} // namespace pharmadesk_tests
