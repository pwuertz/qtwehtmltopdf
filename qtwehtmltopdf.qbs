import qbs

QtApplication {
    name: "qtwehtmltopdf"
    consoleApplication: true
    Depends {
        name: "Qt"
        submodules: ["core", "webengine", "webenginewidgets", "printsupport"]
    }
    files: ["main.cpp"]

    // Deploy
    Group {
        name: "Application binary"
        fileTagsFilter: "application"
        qbs.install: true
        qbs.installDir: "."
    }
}
