// fluor — plan a fluorophore panel: dye spectra over your lasers and filters,
// and which dyes will bleed into each other's detectors.

#include <QGuiApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>
#include <cstdio>

#include "backend.h"
#include "cli.h"
#include "portalfilepicker.h"
#include "spectraplot.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    // Fluor themes itself, as Hype does; Qt's GTK platform theme would only
    // pull GTK (and its accessibility bridge) into a Qt Quick app.
    qputenv("QT_QPA_PLATFORMTHEME", "generic");
    // Commands and help draw no window, so they must not need a display, even
    // where the desktop exports QT_QPA_PLATFORM=wayland. Bare fluor prints
    // help, as a command line tool should; the launcher says fluor open.
    const bool command = argc == 1 || isCliCommand(argv[1]);
    bool windowless = command;
    for (int i = 1; i < argc; ++i) {
        const QByteArray argument(argv[i]);
        windowless = windowless || argument == "--help" || argument == "-h" || argument == "--version" || argument == "-v";
    }
    if (windowless)
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    app.setApplicationName("fluor");
    app.setApplicationVersion("0.1.0");

    // Matches the window to fluor.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("fluor");
    app.setWindowIcon(QIcon::fromTheme("fluor"));

    if (command)
        return runCli(app.arguments());

    QCommandLineParser args;
    args.setApplicationDescription("Plan fluorophore panels against your lasers and filters.\n\n" + cliSummary());
    args.addHelpOption();
    args.addVersionOption();
    args.addPositionalArgument("panel", "Fluor panel file");
    QCommandLineOption screenshotOption("screenshot", "Save a screenshot of the window and exit", "file");
    screenshotOption.setFlags(QCommandLineOption::HiddenFromHelp);
    args.addOption(screenshotOption);
    QStringList arguments = app.arguments();
    if (arguments.value(1) == "open")
        arguments.removeAt(1);
    args.process(arguments);

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    PortalFilePicker picker;
    Backend backend(&picker);
    if (!args.positionalArguments().isEmpty())
        backend.load(args.positionalArguments().first());

    qmlRegisterType<SpectraPlot>("Fluor", 1, 0, "SpectraPlot");
    qmlRegisterUncreatableType<Backend>("Fluor", 1, 0, "Backend", "Backend is provided as the backend property");

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    // For the README and for checking the window without a desktop.
    if (args.isSet(screenshotOption)) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        const QString path = args.value(screenshotOption);
        QTimer::singleShot(800, &app, [window, path] {
            const bool ok = window && window->grabWindow().save(path);
            if (!ok)
                fprintf(stderr, "Could not save %s\n", qPrintable(path));
            QCoreApplication::exit(ok ? 0 : 1);
        });
    }

    return app.exec();
}
