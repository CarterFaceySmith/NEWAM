#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QTimer>
#include <csignal>
#include <iostream>
#include "networkedEWAM.h"

// Signal handler function prototype
void signalHandler(int signal);
static QCoreApplication* appPtr = nullptr;

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    appPtr = &app;  // Store application pointer for signal handler

    QCoreApplication::setApplicationName("TCP JSON Sender");
    QCoreApplication::setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Send simulated aerospace entity data to TCP socket");
    parser.addHelpOption();
    parser.addVersionOption();

    // Basic network options (using 'H' instead of 'h' for host)
    QCommandLineOption hostOption(QStringList() << "H" << "host",
                                  "Server host address", "host", "localhost");
    QCommandLineOption portOption(QStringList() << "p" << "port",
                                  "Server port", "port", "12345");

    // Simulation options (using 'V' instead of 'v' for verbose)
    QCommandLineOption scenarioOption(QStringList() << "s" << "scenario",
                                      "Simulation scenario (melbourne, convoy, combat, custom)", "scenario", "melbourne");
    QCommandLineOption intervalOption(QStringList() << "i" << "interval",
                                      "Update interval in milliseconds", "interval", "1000");
    QCommandLineOption verboseOption(QStringList() << "V" << "verbose",
                                     "Enable verbose output");

    // Mode options
    QCommandLineOption serverOption(QStringList() << "server",
                                    "Run in server mode instead of client mode");
    QCommandLineOption testOption(QStringList() << "test",
                                  "Run in test mode (send/receive simple messages)");
    QCommandLineOption messageOption(QStringList() << "m" << "message",
                                     "Test message to send in test mode", "message", "Hello World");

    QCommandLineOption noReconnectOption("no-reconnect",
                                         "Disable automatic reconnection attempts");
    QCommandLineOption reconnectIntervalOption(QStringList() << "r" << "reconnect-interval", "Reconnection attempt interval in seconds", "seconds", "5");

    parser.addOption(noReconnectOption);
    parser.addOption(reconnectIntervalOption);
    parser.addOption(hostOption);
    parser.addOption(portOption);
    parser.addOption(scenarioOption);
    parser.addOption(intervalOption);
    parser.addOption(verboseOption);
    parser.addOption(serverOption);
    parser.addOption(testOption);
    parser.addOption(messageOption);

    parser.process(app);

    // Get command line values
    QString host = parser.value(hostOption);
    quint16 port = parser.value(portOption).toUShort();
    QString scenario = parser.value(scenarioOption);
    int interval = parser.value(intervalOption).toInt();
    bool verbose = parser.isSet(verboseOption);
    bool serverMode = parser.isSet(serverOption);
    bool testMode = parser.isSet(testOption);
    QString testMessage = parser.value(messageOption);
    bool autoReconnect = !parser.isSet(noReconnectOption);
    int reconnectInterval = parser.value(reconnectIntervalOption).toInt() * 1000; // Convert to ms

    // Validate scenario if we're not in server or test mode
    if (!serverMode && !testMode) {
        QStringList validScenarios = {"melbourne", "convoy", "combat", "custom"};
        if (!validScenarios.contains(scenario)) {
            std::cerr << "Invalid scenario. Valid options are: "
                      << validScenarios.join(", ").toStdString() << std::endl;
            return 1;
        }
    }

    // Validate interval
    if (interval < 100) {
        std::cerr << "Warning: Update interval less than 100ms may cause performance issues" << std::endl;
    }

    // Print mode and connection info
    if (serverMode) {
        std::cout << "Starting server on port " << port << std::endl;
    } else if (testMode) {
        std::cout << "Starting in test mode" << std::endl;
        std::cout << "Server: " << host.toStdString() << ":" << port << std::endl;
        std::cout << "Test message: " << testMessage.toStdString() << std::endl;
        std::cout << "Interval: " << interval << "ms" << std::endl;
    } else {
        std::cout << "Starting " << scenario.toStdString() << " scenario..." << std::endl;
        std::cout << "Server: " << host.toStdString() << ":" << port << std::endl;
        std::cout << "Update interval: " << interval << "ms" << std::endl;
    }

    // Create sender instance
    NetworkedEWAM sender;

    if (!serverMode) {
        if (autoReconnect) {
            std::cout << "Auto-reconnect enabled (interval: "
                      << reconnectInterval/1000 << "s)" << std::endl;
        }
        sender.setReconnectInterval(reconnectInterval);
    }

    // Setup based on mode
    if (serverMode) {
        if (!sender.startServer(port)) {
            return 1;
        }
    } else if (testMode) {
        sender.connectToHost(host, port);

        // Setup test message timer
        QTimer messageTimer;
        messageTimer.setInterval(interval);
        QObject::connect(&messageTimer, &QTimer::timeout, [&sender, testMessage]() {
            sender.sendTestMessage(testMessage);
        });
        messageTimer.start();
    }  else {
        // Create a persistent timer (not a local variable)
        QTimer* updateTimer = new QTimer(&app);
        updateTimer->setInterval(interval);

        // Connect before starting simulation to ensure we don't miss updates
        QObject::connect(updateTimer, &QTimer::timeout, [&sender, interval]() {
            if (sender.isConnected()) {
                sender.updateSimulation(interval);
            }
        });

        // Start timer immediately
        updateTimer->start();

        // Connect after timer to prevent race condition
        sender.connectToHost(host, port);
        sender.initializeSimulation(scenario);

        // Add debug output to verify timer is running
        std::cout << "Simulation timer started with interval: " << interval << "ms" << std::endl;

        // Clean up timer on quit
        QObject::connect(&app, &QCoreApplication::aboutToQuit, [updateTimer]() {
            updateTimer->stop();
            delete updateTimer;
        });
    }

    // Setup clean shutdown handlers
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        if (serverMode) {
            std::cout << "\nStopping server..." << std::endl;
            sender.stopServer();
        } else {
            std::cout << "\nDisconnecting..." << std::endl;
        }
    });

    // For Ctrl+C handling
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Optional: Print help message for interactive commands if verbose mode is on
    if (verbose) {
        std::cout << "\nInteractive commands:" << std::endl;
        std::cout << "  Ctrl+C - Quit application" << std::endl;
        if (testMode) {
            std::cout << "  (Messages will be sent automatically every "
                      << interval << "ms)" << std::endl;
        }
    }

    int result = app.exec();

    // Clean up
    if (serverMode) {
        sender.stopServer();
    }

    std::cout << "Application ended." << std::endl;
    return result;
}

// Signal handler implementation
void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << std::endl;
    if (appPtr) {
        appPtr->quit();
    }
}
