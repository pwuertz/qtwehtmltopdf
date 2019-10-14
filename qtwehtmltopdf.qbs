import qbs

QtApplication {
    name: "qtwehtmltopdf"
    consoleApplication: true
    Depends {
        name: "Qt"
        submodules: ["core", "webengine", "webenginewidgets", "printsupport"]
    }
    cpp.cxxLanguageVersion: "c++17"
    files: ["main.cpp"]

    // Deploy
    Group {
        name: "Application binary"
        fileTagsFilter: "application"
        qbs.install: true
        qbs.installDir: "."
    }
}
