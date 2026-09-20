#include <QApplication>
#include <QCommandLineParser>
#include <QPalette>
#include <QStyleFactory>
#include <QIcon>
#include <QFile>
#include <QIODevice>
#include <iostream>

#include "mainwindow.hpp"
#include "ipc_client.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Hamzex Configurator"));
    app.setApplicationVersion(QStringLiteral("2.0.0"));
    app.setOrganizationName(QStringLiteral("Hamzex"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Hamzex CS2 Native Configurator & Live Hot-Reload GUI"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption applyOpt(QStringList() << QStringLiteral("a") << QStringLiteral("apply"),
                                QStringLiteral("Send given JSON config file directly to running CS2 runtime via IPC"),
                                QStringLiteral("file"));
    parser.addOption(applyOpt);

    QCommandLineOption pingOpt(QStringList() << QStringLiteral("p") << QStringLiteral("ping"),
                               QStringLiteral("Test IPC connection to running CS2 runtime and exit"));
    parser.addOption(pingOpt);

    parser.process(app);

    if (parser.isSet(pingOpt)) {
        IpcClient client;
        quint64 ver = 0;
        bool ok = client.ping(&ver);
        if (ok) {
            std::cout << "CS2 runtime IPC is active (Version: " << ver << ")\n";
            return 0;
        } else {
            std::cout << "CS2 runtime IPC is not responding (socket: " << client.socketPath().toStdString() << ")\n";
            return 1;
        }
    }

    if (parser.isSet(applyOpt)) {
        QString filePath = parser.value(applyOpt);
        QByteArray jsonBytes;
        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly)) {
            std::cerr << "Failed to read config file: " << filePath.toStdString() << "\n";
            return 1;
        }
        jsonBytes = f.readAll();
        IpcClient client;
        quint64 ver = 0;
        QString err;
        if (client.applyConfig(jsonBytes, &ver, &err)) {
            std::cout << "Successfully applied config (Version: " << ver << ")\n";
            return 0;
        } else {
            std::cerr << "Apply error: " << err.toStdString() << "\n";
            return 1;
        }
    }

    // Set modern dark style
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(33, 37, 43));
    darkPalette.setColor(QPalette::WindowText, QColor(220, 223, 228));
    darkPalette.setColor(QPalette::Base, QColor(24, 26, 31));
    darkPalette.setColor(QPalette::AlternateBase, QColor(33, 37, 43));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(24, 26, 31));
    darkPalette.setColor(QPalette::ToolTipText, QColor(220, 223, 228));
    darkPalette.setColor(QPalette::Text, QColor(220, 223, 228));
    darkPalette.setColor(QPalette::Button, QColor(40, 44, 52));
    darkPalette.setColor(QPalette::ButtonText, QColor(220, 223, 228));
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(97, 175, 239));
    darkPalette.setColor(QPalette::Highlight, QColor(77, 120, 180));
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(100, 105, 115));
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(100, 105, 115));

    app.setPalette(darkPalette);

    app.setStyleSheet(QStringLiteral(
        "QMainWindow { background-color: #21252b; }"
        "QTabWidget::pane { border: 1px solid #3e4451; background-color: #282c34; border-radius: 4px; }"
        "QTabBar::tab { background: #21252b; color: #abb2bf; padding: 8px 16px; margin-right: 2px; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
        "QTabBar::tab:selected { background: #282c34; color: #61afef; font-weight: bold; border-bottom: 2px solid #61afef; }"
        "QGroupBox { font-weight: bold; border: 1px solid #3e4451; border-radius: 6px; margin-top: 12px; padding-top: 12px; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 10px; padding: 0 4px; color: #abb2bf; }"
        "QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox { background-color: #1b1d23; border: 1px solid #3e4451; border-radius: 4px; padding: 4px 8px; color: #dcdfe4; }"
        "QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus { border: 1px solid #61afef; }"
        "QPushButton { background-color: #353b45; border: 1px solid #4b5263; border-radius: 4px; padding: 6px 12px; color: #dcdfe4; }"
        "QPushButton:hover { background-color: #3e4451; border-color: #5c6370; }"
        "QPushButton:pressed { background-color: #2c313a; }"
        "QPushButton:disabled { background-color: #21252b; color: #5c6370; border-color: #2c313a; }"
        "QTableWidget { background-color: #1b1d23; gridline-color: #2c313a; border: 1px solid #3e4451; border-radius: 4px; }"
        "QHeaderView::section { background-color: #282c34; color: #abb2bf; padding: 4px 6px; border: 1px solid #3e4451; font-weight: bold; }"
        "QListWidget { background-color: #1b1d23; border: 1px solid #3e4451; border-radius: 4px; padding: 2px; }"
        "QListWidget::item { padding: 4px 8px; border-radius: 2px; }"
        "QListWidget::item:selected { background-color: #383e4a; color: #61afef; }"
        "QStatusBar { background-color: #1e2227; color: #98c379; }"
    ));

    MainWindow win;
    win.show();

    return app.exec();
}
