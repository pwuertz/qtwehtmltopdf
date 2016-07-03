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
    });
    parser.addPositionalArgument("url", "URL to HTML document");
    parser.addPositionalArgument("output", "Path to PDF or '-' for stdout");
    parser.process(a);

    // check number of positional args
    auto args_url_output = parser.positionalArguments();
    if (args_url_output.size() != 2)
        parser.showHelp(1);
    QUrl url_report(args_url_output[0]);
    QString pdf_target(args_url_output[1]);

    // load page
    QWebEnginePage page;
    page.load(url_report);
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
            page.printToPdf([&](const QByteArray& data){
                if (!data.size()) {
                    qCritical() << "Error printing page";
                    QCoreApplication::exit(-1);
                    return;
                }
                if (pdf_target != "-") {
                    // save to file
                    QFile file(pdf_target);
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
        });
    });

    return a.exec();
}
