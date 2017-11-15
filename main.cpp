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
#include <functional>
#include <iostream>
#ifdef Q_OS_WIN
#include <io.h>
#include <fcntl.h>
#endif


const std::map<const QString, const double> to_mm_factor = {
    {"mm", 1.0},
    {"cm", 10.0},
    {"in", 25.4},
    {"pt", 0.352778},
};

/**
 * @brief cssLengthToMm
 * @param s CSS length value
 * @return length in mm
 */
double cssLengthToMm(QString s) {
    double value = s.left(s.length() - 2).toDouble();
    try {
        auto unit = s.right(2);
        return value * to_mm_factor.at(unit);
    } catch (const std::out_of_range&) {
        return 0.0;
    }
}

/**
 * @brief getCssPageLayout
 * @param page web page to query for css page layout
 * @param cb callback invoked after querying page layout
 */
void getCssPageLayout(QWebEnginePage& page, std::function<void(QPageLayout)> cb)
{
    constexpr auto jscode = R"JSCODE(
    (function getPrintPageStyle() {
        var pageStyle = {
           "size": "a4",
           "margin-top": "0mm",
           "margin-left": "0mm",
           "margin-right": "0mm",
           "margin-bottom": "0mm",
        };

        function applyCSSPageRule(rule) {
           for (var property in pageStyle) {
               var value = rule.style.getPropertyValue(property);
               pageStyle[property] = (value) ? value : pageStyle[property];
           }
        }

        function hasMediaType(cssMediaRule, type) {
           for (var i=0; i < cssMediaRule.media.length; i++)
               if (cssMediaRule.media[i] === type)
                   return true;
           return false;
        }

        function loopRules(rules) {
           for (var i = 0; i < rules.length; i++) {
               var rule = rules[i];
               if (rule instanceof CSSMediaRule && hasMediaType(rule, "print")) {
                   loopRules(rule.cssRules);
               } else if (rule instanceof CSSPageRule) {
                   applyCSSPageRule(rule);
               }
           }
        }
        for (var i = 0; i < document.styleSheets.length; i++) {
           loopRules(document.styleSheets[i].cssRules);
        }
        return pageStyle;
    })()
    )JSCODE";
    page.runJavaScript(jscode, [cb](const QVariant &result){
        auto style = result.toMap();
        auto default_margin_mm = cssLengthToMm(style["margins"].toString());
        std::vector<double> margins_mm;
        for(const auto& margin_name: {"margin-left", "margin-top", "margin-right", "margin-bottom"}) {
            double margin_mm = default_margin_mm;
            QString margin_str(style[margin_name].toString());
            if (!margin_str.isEmpty())
                margin_mm = cssLengthToMm(margin_str);
            margins_mm.emplace_back(margin_mm);
        }
        QPageLayout layout(QPageSize(QPageSize::A4), QPageLayout::Portrait,
                           QMarginsF(margins_mm[0], margins_mm[1], margins_mm[2], margins_mm[3]), QPageLayout::Millimeter);
        cb(layout);
    });
}

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
            getCssPageLayout(page, [&](QPageLayout layout){
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
                    }, layout);
                } else {
                    // print to named printer
                    auto printer_info = QPrinterInfo::printerInfo(output_name);
                    if (printer_info.isNull()) {
                        qCritical() << "Printer not found";
                        QCoreApplication::exit(EXIT_FAILURE);
                        return;
                    }
                    QPrinter* printer = new QPrinter(printer_info);
                    qDebug() << "setPageLayout" << printer->setPageLayout(layout);
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
    });

    return a.exec();
}
