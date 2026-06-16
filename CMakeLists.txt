cmake_minimum_required(VERSION 3.16)
project(pharmadesk VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Surface warnings (maintainability). Not -Werror so a stray third-party/MOC
# warning never blocks a build on the deployment box.
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra)
endif()
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Prefer Qt6 (the AlmaLinux 10 deployment target). Fall back to Qt5 so the
# project still builds on the current dev machine (Arch ships Qt 5.15) until
# Qt6 is installed. The component set is identical for both.
set(QT_COMPONENTS Widgets Sql PrintSupport)
find_package(Qt6 QUIET COMPONENTS ${QT_COMPONENTS})
if (Qt6_FOUND)
    set(QT_VERSION_MAJOR 6)
    message(STATUS "Building against Qt6")
else()
    find_package(Qt5 REQUIRED COMPONENTS ${QT_COMPONENTS})
    set(QT_VERSION_MAJOR 5)
    message(STATUS "Qt6 not found — falling back to Qt5 (dev only; ship with Qt6)")
endif()

# bcrypt comes from the OS libxcrypt (crypt_r / crypt_gensalt_rn). No third-party
# code is vendored; this is the same implementation PHP's password_hash uses.
find_library(CRYPT_LIBRARY NAMES crypt)
if (NOT CRYPT_LIBRARY)
    message(FATAL_ERROR "libcrypt (libxcrypt) not found — required for bcrypt.")
endif()

# zlib (ubiquitous system lib; Qt already links it transitively) is used to inflate
# the DEFLATE streams inside an .xlsx workbook for the medicine catalog importer.
find_package(ZLIB REQUIRED)

# ── Shared core: all app code except main() ─────────────────────────────────
# Used by the GUI app, the headless smoke test, and the screenshot harness.
add_library(pharmadesk_core STATIC
    src/MainWindow.cpp
    src/domain/AppPaths.cpp
    src/domain/DesktopIntegration.cpp
    src/domain/Log.cpp
    src/domain/Bcrypt.cpp
    src/domain/PinPolicy.cpp
    src/domain/Money.cpp
    src/domain/SaleCalculator.cpp
    src/domain/Fefo.cpp
    src/domain/MedicineForm.cpp
    src/domain/CostBlender.cpp
    src/domain/BarcodeParser.cpp
    src/domain/ControlledSubstancePolicy.cpp
    src/domain/DiscountAuthorizationPolicy.cpp
    src/data/SessionRepository.cpp
    src/data/ReturnRepository.cpp
    src/data/StockAdjustmentRepository.cpp
    src/data/AnalyticsRepository.cpp
    src/data/ParkedCartRepository.cpp
    src/data/EscPosRenderer.cpp
    src/service/ReturnService.cpp
    src/service/StockAdjuster.cpp
    src/service/Reconciler.cpp
    src/service/CupsRawPrinter.cpp
    src/service/CupsStatus.cpp
    src/service/AlternativesFinder.cpp
    src/service/ZReportArchive.cpp
    src/ui/SessionsPage.cpp
    src/ui/ZReportDialog.cpp
    src/ui/ReturnsPage.cpp
    src/ui/PlReportWidget.cpp
    src/ui/TopSellersWidget.cpp
    src/ui/TaxReportWidget.cpp
    src/ui/RefundsReportWidget.cpp
    src/ui/ComplianceReportWidget.cpp
    src/ui/VelocityReportWidget.cpp
    src/ui/NarcoticRegisterWidget.cpp
    src/ui/GrnDetailDialog.cpp
    src/ui/ParkedBillsDialog.cpp
    src/data/Database.cpp
    src/data/SettingsRepository.cpp
    src/data/UserRepository.cpp
    src/data/Audit.cpp
    src/data/MedicineRepository.cpp
    src/data/CatalogImporter.cpp
    src/data/XlsxReader.cpp
    src/data/BatchRepository.cpp
    src/data/InventoryRepository.cpp
    src/data/SupplierRepository.cpp
    src/data/GrnRepository.cpp
    src/data/SalesReportRepository.cpp
    src/data/SaleRepository.cpp
    src/data/AuditRepository.cpp
    src/service/SaleService.cpp
    src/service/GrnService.cpp
    src/service/BackupService.cpp
    src/ui/UiUtil.cpp
    src/ui/Theme.cpp
    src/ui/DashboardPage.cpp
    src/ui/SetupWizard.cpp
    src/ui/LoginDialog.cpp
    src/ui/setup/SetupPages.cpp
    src/ui/MedicinesPage.cpp
    src/ui/MedicineDialog.cpp
    src/ui/StockInDialog.cpp
    src/ui/PosTerminalPage.cpp
    src/ui/ReceiptDialog.cpp
    src/ui/InventoryPage.cpp
    src/ui/SupplierDialog.cpp
    src/ui/GrnLineDialog.cpp
    src/ui/GrnDialog.cpp
    src/ui/PurchasingPage.cpp
    src/ui/ReportsPage.cpp
    src/ui/AdminPage.cpp
    src/ui/UserDialog.cpp
    src/ui/ChangePinDialog.cpp
    src/ui/ManagerOverrideDialog.cpp
    src/ui/AdjustmentsPage.cpp
    src/ui/AlternativesDialog.cpp
)
target_include_directories(pharmadesk_core PUBLIC src)
# Bake the project version in so the app can detect first-install vs. upgrade
# and integrate into the desktop accordingly (see DesktopIntegration).
target_compile_definitions(pharmadesk_core PUBLIC PHARMADESK_VERSION="${PROJECT_VERSION}")
target_link_libraries(pharmadesk_core PUBLIC
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::PrintSupport
    ${CRYPT_LIBRARY}
    ZLIB::ZLIB
)

# ── GUI application ─────────────────────────────────────────────────────────
# resources.qrc is compiled into each executable (not the static lib) so Qt
# auto-registers it — resources in a static lib need an explicit Q_INIT_RESOURCE.
add_executable(pharmadesk src/main.cpp resources/resources.qrc)
target_link_libraries(pharmadesk PRIVATE pharmadesk_core)

install(TARGETS pharmadesk RUNTIME DESTINATION bin)
install(FILES packaging/pharmadesk.desktop DESTINATION share/applications)
install(FILES packaging/pharmadesk.png
        DESTINATION share/icons/hicolor/256x256/apps)

# ── Catalog import CLI (load the legacy Postgres medicines CSV) ──────────────
add_executable(pharmadesk_import tools/import_catalog.cpp)
target_link_libraries(pharmadesk_import PRIVATE pharmadesk_core)

# ── Headless smoke test + screenshot harness ────────────────────────────────
option(BUILD_TESTING "Build the smoke test + screenshot harness" ON)
if (BUILD_TESTING)
    add_executable(pharmadesk_smoke tests/smoke.cpp resources/resources.qrc)
    target_link_libraries(pharmadesk_smoke PRIVATE pharmadesk_core)

    add_executable(pharmadesk_shots tests/screenshots.cpp resources/resources.qrc)
    target_link_libraries(pharmadesk_shots PRIVATE pharmadesk_core)

    add_executable(pharmadesk_barcode_test tests/barcode_test.cpp)
    target_link_libraries(pharmadesk_barcode_test PRIVATE pharmadesk_core)

    # ── Full automated suite (1000+ assertions across every module) ─────────
    add_executable(pharmadesk_tests
        tests/run_all.cpp
        tests/cases/money_tests.cpp
        tests/cases/salecalc_tests.cpp
        tests/cases/costblender_tests.cpp
        tests/cases/fefo_tests.cpp
        tests/cases/barcode_tests.cpp
        tests/cases/pinpolicy_tests.cpp
        tests/cases/sale_tests.cpp
        tests/cases/returns_tests.cpp
        tests/cases/sessions_tests.cpp
        tests/cases/grn_tests.cpp
        tests/cases/medicine_tests.cpp
        tests/cases/inventory_tests.cpp
        tests/cases/analytics_tests.cpp
        tests/cases/parked_tests.cpp
        tests/cases/schema_tests.cpp
        tests/cases/catalog_tests.cpp
        tests/cases/adjustment_tests.cpp
        tests/cases/printing_tests.cpp
        tests/cases/auth_tests.cpp
        resources/resources.qrc
    )
    target_include_directories(pharmadesk_tests PRIVATE tests)
    target_link_libraries(pharmadesk_tests PRIVATE pharmadesk_core)

    # Register the suites with CTest so a release can be GATED on green tests
    # (`ctest` previously ran nothing). Both binaries already exit non-zero on
    # any failed assertion (run_all.cpp / barcode_test), so CTest fails correctly
    # with no test rewrites. Offscreen platform + a timeout keep it headless/CI-safe.
    enable_testing()
    add_test(NAME suite COMMAND pharmadesk_tests)
    add_test(NAME barcode COMMAND pharmadesk_barcode_test)
    set_tests_properties(suite barcode PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 300)
endif()
