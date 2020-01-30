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


double to_mm(const QString& unit)
{
    if (unit == "mm") { return 1.0; }
    if (unit == "cm") { return 10.0; }
    if (unit == "in") { return 25.4; }
    if (unit == "pt") { return 0.352778; }
    return 0.0;
}

/**
 * @brief cssLengthToMm
 * @param s CSS length value
 * @return length in mm
 */
double cssLengthToMm(const QString& s)
{
    const double value = s.left(s.length() - 2).toDouble();
    const QString unit = s.right(2);
    return value * to_mm(unit);
}

/**
 * @brief getCssPageLayout
 * @param page web page to query for css page layout
 * @param cb callback invoked after querying page layout
 */
void getCssPageLayout(QWebEnginePage& page, std::function<void(const QPageLayout&)> cb)
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
    page.runJavaScript(jscode, [cb=std::move(cb)](const QVariant& result) {
        const QVariantMap pageStyle = result.toMap();
        const double defaultMargin_mm = cssLengthToMm(pageStyle["margins"].toString());
        std::vector<double> margins_mm;
        for(const auto& marginName: {"margin-left", "margin-top", "margin-right", "margin-bottom"})
        {
            const QString marginCss(pageStyle[marginName].toString());
            const double margin_mm = (marginCss.isEmpty()) ? defaultMargin_mm
                                                           : cssLengthToMm(marginCss);
            margins_mm.push_back(margin_mm);
        }
        const QPageLayout layout(
            QPageSize(QPageSize::A4), QPageLayout::Portrait,
            QMarginsF(margins_mm[0], margins_mm[1], margins_mm[2], margins_mm[3]),
            QPageLayout::Millimeter
        );
        cb(layout);
    });
}

constexpr auto fileno_bin = [](auto fd) {
    #ifdef Q_OS_WIN
    _setmode(_fileno(fd), _O_BINARY);
    return _fileno(fd);
    #else
    return fileno(fd);
    #endif
};

QByteArray readStdin()
{
    QFile file;
    file.open(fileno_bin(stdin), QIODevice::OpenMode(QFile::ReadOnly));
    const QByteArray data = file.readAll();
    file.close();
    return data;
}

bool writePdf(const QString& outputFilename, const QByteArray& data)
{
    // Write to stdout
    if (outputFilename == "-") {
        QFile file;
        file.open(fileno_bin(stdout), QIODevice::OpenMode(QIODevice::OpenModeFlag::WriteOnly));
        file.write(data);
        file.close();
        return true;
    }
    // Write to file
    QFile file(outputFilename);
    bool ok = file.open(QIODevice::WriteOnly);
    ok &= file.write(data) == data.size();
    file.close();
    return ok;
}

void printToPDF(QWebEnginePage& page, const QPageLayout& layout, const QString& outputFilename)
{
    page.printToPdf([outputFilename](const QByteArray& pdfData) {
        if (pdfData.isEmpty()) {
            qCritical() << "Error printing page";
            QCoreApplication::exit(EXIT_FAILURE);
            return;
        }
        const bool ok = writePdf(outputFilename, pdfData);
        QCoreApplication::exit(ok ? EXIT_SUCCESS : EXIT_FAILURE);
    }, layout);
}

void printToPrinter(QWebEnginePage& page, const QPageLayout& layout, const QString& printerName)
{
    // Validate printer
    const auto printerInfo = QPrinterInfo::printerInfo(printerName);
    if (printerInfo.isNull()) {
        qCritical() << "Printer not found";
        QCoreApplication::exit(EXIT_FAILURE);
        return;
    }
    auto printer = std::make_shared<QPrinter>(printerInfo);
    qDebug() << "setPageLayout" << printer->setPageLayout(layout);
    page.print(printer.get(), [printer](const bool success) {
        if (!success) {
            qCritical() << "Error printing page";
        }
        QCoreApplication::exit(success ? EXIT_SUCCESS : EXIT_FAILURE);
    });
}


int main(int argc, char *argv[])
{
    QtWebEngine::initialize();
    QApplication app(argc, argv);

    // Delete WebEngine cache
    QDir cache_dir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    cache_dir.removeRecursively();


    QCommandLineParser parser;
    parser.setApplicationDescription("Qt WebEngine HTML to PDF");
    parser.addHelpOption();
    parser.addOptions({
        {"printer", "Send output to printer instead of PDF file"},
        {"list-printers", "Show list of available printers and exit"},
        {"post", "Send HTTP POST request with data from stdin"},
    });
    parser.addPositionalArgument("url", "URL to HTML document");
    parser.addPositionalArgument("output", "PDF filename, printer name or '-' for stdout");
    parser.process(app);

    // If list-printers is set, show list and exit
    if (parser.isSet("list-printers")) {
        const QStringList printerNames = QPrinterInfo::availablePrinterNames();
        for (const auto& name : printerNames) { qInfo().quote() << name; }
        return EXIT_SUCCESS;
    }

    // Check number of positional args
    const QStringList args_url_output = parser.positionalArguments();
    if (args_url_output.size() != 2) {
        parser.showHelp(1);
    }
    const QUrl documentUrl(args_url_output[0]);
    const QString outputName(args_url_output[1]);

    // Create HTTP request for given program arguments
    const QWebEngineHttpRequest request = [&]() {
        QWebEngineHttpRequest request { documentUrl };
        if (parser.isSet("post")) {
            request.setMethod(QWebEngineHttpRequest::Post);
            request.setHeader("Content-Type", "application/json");  // Fixed type for now
            request.setPostData(readStdin());
        }
        return request;
    }();

    // Create web page and PDF creation handlers
    QWebEnginePage page;
    QObject::connect(&page, &QWebEnginePage::loadFinished, &page, [&](const bool ok) {
        if (!ok) {
            qCritical() << "Error loading page";
            QCoreApplication::exit(EXIT_FAILURE);
            return;
        }
        // Queue print request after page finished loading
        // TODO: How to ensure that page finished js processing? -> Add some time as workaround
        QTimer::singleShot(100, &page, [&]() {
            getCssPageLayout(page, [&](const QPageLayout& layout) {
                if (!parser.isSet("printer")) {
                    printToPDF(page, layout, outputName);
                } else {
                    printToPrinter(page, layout, outputName);
                }
            });
        });
    });

    // Init process by loading the web page
    page.load(request);
    return QApplication::exec();
}
