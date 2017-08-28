#include <QApplication>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QtWebEngine/QtWebEngine>
#include <QWebEnginePage>
#include <QPageLayout>
#include <QPageSize>
#include <QMarginsF>
#include <QDebug>
#include <QCommandLineParser>
#include <QPrinterInfo>
#include <QPrinter>
#include <memory>
#include <iostream>
#ifdef Q_OS_WIN
#include <io.h>
#include <fcntl.h>
#endif


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // initialize webengine and delete cache
    QtWebEngine::initialize();
    QDir cache_dir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    cache_dir.removeRecursively();


    QCommandLineParser parser;
    parser.setApplicationDescription("Qt WebEngine HTML to PDF");
    parser.addHelpOption();
    parser.addOptions({
        {"L", "Margin <left> (unused)", "left-margin"},
        {"R", "Margin <right> (unused)", "right-margin"},
        {"T", "Margin <top> (unused)", "top-margin"},
        {"B", "Margin <bottom> (unused)", "bottom-margin"},
        {"print-media-type", "(unused)"},
        {"printer", "Send output to printer instead of PDF file"},
        {"list-printers", "Show list of available printers and exit"},
    });
    parser.addPositionalArgument("url", "URL to HTML document");
    parser.addPositionalArgument("output", "PDF filename, printer name or '-' for stdout");
    parser.process(a);

    // if list-printers is set, show list and exit
    if (parser.isSet("list-printers")) {
        foreach (const QString &name, QPrinterInfo::availablePrinterNames())
            qInfo().quote() << name;
        return 0;
    }

    // check number of positional args
    auto args_url_output = parser.positionalArguments();
    if (args_url_output.size() != 2)
        parser.showHelp(1);
    QUrl document_url(args_url_output[0]);
    QString output_name(args_url_output[1]);

    // load page
    QWebEnginePage page;
    page.load(document_url);
    QObject::connect(&page, &QWebEnginePage::loadFinished, &page, [&](bool ok){
        if (!ok) {
            qCritical() << "Error loading page";
            QCoreApplication::exit(-1);
            return;
        }

        // if page loaded, queue print request (add time for evaluating js)
        // TODO: how to ensure that page finished js processing?
        QTimer::singleShot(100, [&](){
            QPageLayout pageLayout(QPageSize(QPageSize::A4), QPageLayout::Portrait, QMarginsF( 0, 0, 0, 0 ));
            if (!parser.isSet("printer")) {
                // print to PDF file
                page.printToPdf([&](const QByteArray& data){
                    if (!data.size()) {
                        qCritical() << "Error printing page";
                        QCoreApplication::exit(-1);
                        return;
                    }
                    if (output_name != "-") {
                        // save to file
                        QFile file(output_name);
                        file.open(QIODevice::WriteOnly);
                        file.write(data);
                        file.close();
                        QCoreApplication::exit(0);
                    } else {
                        // write to stdout
                        QFile file;
    #ifdef Q_OS_WIN
                        setmode(fileno(stdout), O_BINARY);
    #endif
                        file.open(fileno(stdout), QIODevice::OpenMode(QIODevice::OpenModeFlag::WriteOnly));
                        file.write(data);
                        file.close();
                        QCoreApplication::exit(0);
                    }
                }, pageLayout);
            } else {
                // print to named printer
                auto printer_info = QPrinterInfo::printerInfo(output_name);
                if (printer_info.isNull()) {
                    qCritical() << "Printer not found";
                    QCoreApplication::exit(EXIT_FAILURE);
                    return;
                }
                QPrinter* printer = new QPrinter(printer_info);
                qDebug() << "setPageLayout" << printer->setPageLayout(pageLayout);
                page.print(printer, [printer](bool success){
                    delete printer;
                    if (success)
                        QCoreApplication::exit(EXIT_SUCCESS);
                    else {
                        qCritical() << "Error printing page";
                        QCoreApplication::exit(EXIT_FAILURE);
                    }
                });
            }
        });
    });

    return a.exec();
}
