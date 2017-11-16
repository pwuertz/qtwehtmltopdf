Qt WebEngine HTML to PDF converter
----------------------------------

`qtwehtmltopdf` is a command line tool for converting HTML pages to PDF documents. It is based on the Qt WebEngine framework, which in turn uses the Chromium engine to render HTML content.

Basic usage
-----------

For creating PDF from HTML simply call `qtwehtmltopdf` using the URL as first, and a target filename as second command line argument.
```
# Save PDF to file
qtwehtmltopdf http://google.com google.pdf
```

When using `-` as filename the PDF is written to stdout, allowing other applications to call into `qtwehtmltopdf` and retrieve the PDF data.
```
# Write PDF to stdout
qtwehtmltopdf http://google.com -
```

Building
--------

`qtwehtmltopdf` is a Qt console application which should compile on any platform with a recent Qt5 SDK.
