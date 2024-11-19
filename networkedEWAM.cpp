#include "networkedEWAM.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#include <QTimer>
#include <iostream>
#include <cmath>

NetworkedEWAM::NetworkedEWAM(QObject *parent)
    : QObject(parent)
    , socket(new QTcpSocket(this))
    , reconnectTimer(new QTimer(this))
    , reconnectInterval(5000)  // 5 seconds default
    , reconnectAttempts(0)
    , autoReconnect(true)
{
    connect(socket, &QTcpSocket::connected, this, &NetworkedEWAM::onConnected);
    connect(socket, &QTcpSocket::disconnected, this, &NetworkedEWAM::onDisconnected);
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &NetworkedEWAM::onError);

    connect(reconnectTimer, &QTimer::timeout, this, &NetworkedEWAM::tryReconnect);

    // Initialize random seed
    qsrand(QDateTime::currentMSecsSinceEpoch());
}

void NetworkedEWAM::connectToHost(const QString& host, quint16 port) {
    currentHost = host;
    currentPort = port;
    reconnectAttempts = 0;

    std::cout << "Connecting to " << host.toStdString() << ":" << port << "..." << std::endl;
    socket->connectToHost(host, port);
}

void NetworkedEWAM::onConnected() {
    std::cout << "Connected to server" << std::endl;
    reconnectTimer->stop();
    reconnectAttempts = 0;
}

void NetworkedEWAM::onDisconnected() {
    std::cout << "Disconnected from server" << std::endl;
    if (autoReconnect && reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
        reconnectTimer->start(reconnectInterval);
    }
}

void NetworkedEWAM::onError(QAbstractSocket::SocketError error) {
    std::cerr << "Socket error: " << socket->errorString().toStdString() << std::endl;

    if (error == QAbstractSocket::ConnectionRefusedError) {
        std::cout << "Connection refused. ";
        if (autoReconnect && reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
            std::cout << "Will attempt to reconnect in "
                     << (reconnectInterval/1000) << " seconds..." << std::endl;
            reconnectTimer->start(reconnectInterval);
        } else if (reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
            std::cout << "Max reconnection attempts reached. Giving up." << std::endl;
        }
    }
}

void NetworkedEWAM::tryReconnect() {
    reconnectAttempts++;
    std::cout << "Reconnection attempt " << reconnectAttempts
              << " of " << MAX_RECONNECT_ATTEMPTS << "..." << std::endl;

    socket->connectToHost(currentHost, currentPort);
}

bool NetworkedEWAM::sendJson(const QJsonObject& json) {
    if (socket->state() != QAbstractSocket::ConnectedState) {
        if (autoReconnect && reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
            std::cout << "Not connected. Queuing message..." << std::endl;
            // Could add message queuing here if needed
            return false;
        }
        std::cerr << "Not connected to server and max reconnection attempts reached" << std::endl;
        return false;
    }

    QJsonDocument doc(json);
    QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";

    qint64 written = socket->write(data);
    if (written == -1) {
        std::cerr << "Failed to write data" << std::endl;
        return false;
    }

    // std::cout << "Sent: " << QString(data).trimmed().toStdString() << std::endl;
    return socket->flush();
}

NetworkedEWAM::~NetworkedEWAM() {
}

void NetworkedEWAM::initializeSimulation(const QString& scenario) {
    if (scenario == "melbourne") {
        // Melbourne area simulation with multiple aircraft types
        createSimulatedEntity("FAST01", "F35", -37.814, 144.963, 25000);
        createSimulatedEntity("SLOW02", "P8", -37.714, 144.863, 30000);
        createSimulatedEntity("SURV03", "E7", -37.914, 144.863, 35000);

        // Add some emitters for radar coverage
        createSimulatedEmitter("RADAR01", "RADAR", "TA", -37.804, 144.953);
        createSimulatedEmitter("RADAR02", "RADAR", "MG", -37.714, 144.963);
    }
    else if (scenario == "convoy") {
        // Simulated convoy with escort
        double baseLatLon[2] = {-37.814, 144.963};
        for (int i = 0; i < 3; ++i) {
            QString id = QString("CONV%1").arg(i+1, 2, 10, QChar('0'));
            createSimulatedEntity(id, "C17",
                baseLatLon[0] + (qrand() % 100 - 50) * 0.0001,
                baseLatLon[1] + (qrand() % 100 - 50) * 0.0001,
                28000 + (qrand() % 4000));
        }
        // Add escort fighters
        createSimulatedEntity("ESC01", "F22", baseLatLon[0] + 0.02, baseLatLon[1] + 0.02, 35000);
        createSimulatedEntity("ESC02", "F22", baseLatLon[0] - 0.02, baseLatLon[1] - 0.02, 35000);
    }
    else if (scenario == "combat") {
        // Simulated combat scenario with multiple aircraft types
        createSimulatedEntity("RED01", "F35", -37.714, 144.863, 30000);
        createSimulatedEntity("RED02", "F35", -37.724, 144.873, 32000);
        createSimulatedEntity("BLUE01", "F22", -37.914, 144.963, 35000);
        createSimulatedEntity("BLUE02", "F22", -37.924, 144.973, 33000);
        createSimulatedEntity("AWC01", "E7", -37.814, 145.063, 38000);

        // Add jamming emitters
        createSimulatedEmitter("JAM01", "JAMMER", "EW", -37.814, 144.913);
        createSimulatedEmitter("JAM02", "JAMMER", "EW", -37.714, 144.863);
    }
    else if (scenario == "custom") {
        // Single aircraft for testing
        createSimulatedEntity("TEST01", "F35", -37.814, 144.963, 30000);
        createSimulatedEmitter("TEST_RADAR", "RADAR", "TA", -37.804, 144.953);
    }
}

void NetworkedEWAM::updateSimulation(int deltaMs) {
    static QDateTime lastLogTime = QDateTime::currentDateTime();
    const double deltaHours = deltaMs / (1000.0 * 60.0 * 60.0);

    // Log header every update
    std::cout << "\n" << QString(80, '-').toStdString() << std::endl;
    std::cout << "ID       TYPE   LAT        LON        ALT     SPD     HDG" << std::endl;
    std::cout << QString(80, '-').toStdString() << std::endl;
    lastLogTime = QDateTime::currentDateTime();

    // Update each entity
    for (auto it = entities.begin(); it != entities.end(); ++it) {
        SimulatedEntity& entity = it.value();

        // Store old values for change detection
        double oldLat = entity.lat;
        double oldLon = entity.lon;
        double oldAlt = entity.altitude;
        double oldSpd = entity.speed;
        double oldHdg = entity.heading;

        // Update position
        double distance = entity.speed * deltaHours;
        double distanceKm = distance * 1.852;
        updatePosition(entity, distanceKm);
        updateDynamics(entity, deltaMs);

        // Periodically set new target values
        if (qrand() % 100 < 5) {
            setNewTargets(entity);
        }

        // Format each field individually first
        QString latStr = QString("%1").arg(entity.lat, 9, 'f', 4);
        QString lonStr = QString("%1").arg(entity.lon, 9, 'f', 4);
        QString altStr = QString("%1").arg(entity.altitude, 7, 'f', 0);
        QString spdStr = QString("%1").arg(entity.speed, 7, 'f', 0);
        QString hdgStr = QString("%1").arg(entity.heading, 6, 'f', 1);

        // Add colors for changed values
        if (fabs(entity.lat - oldLat) > 0.0001) latStr = "\033[32m" + latStr + "\033[0m";
        if (fabs(entity.lon - oldLon) > 0.0001) lonStr = "\033[32m" + lonStr + "\033[0m";
        if (fabs(entity.altitude - oldAlt) > 10) altStr = "\033[33m" + altStr + "\033[0m";
        if (fabs(entity.speed - oldSpd) > 1) spdStr = "\033[36m" + spdStr + "\033[0m";
        if (fabs(entity.heading - oldHdg) > 1) hdgStr = "\033[35m" + hdgStr + "\033[0m";

        QString statusLine = entity.id + "\t " +
                           entity.type + "\t" +
                           latStr + " " +
                           lonStr + " " +
                           altStr + " " +
                           spdStr + " " +
                           hdgStr;

        std::cout << statusLine.toStdString() << std::endl;
        sendEntityUpdate(entity);
    }

    // Update emitters
    for (auto it = emitters.begin(); it != emitters.end(); ++it) {
        Emitter& emitter = it.value();

        // Slowly rotate emitters in a circular pattern
        double radius = 0.01;
        double angle = QDateTime::currentMSecsSinceEpoch() / 10000.0;

        emitter.lat = emitter.lat + radius * sin(angle);
        emitter.lon = emitter.lon + radius * cos(angle);

        sendEmitterUpdate(emitter);
    }
}

void NetworkedEWAM::createSimulatedEntity(const QString& id, const QString& type,
                                         double lat, double lon, double altitude) {
    SimulatedEntity entity;
    entity.id = id;
    entity.type = type;
    entity.lat = lat;
    entity.lon = lon;
    entity.altitude = altitude;
    entity.speed = 400 + (qrand() % 200);
    entity.heading = qrand() % 360;
    entity.turnRate = 0;
    entity.climbRate = 0;
    entity.priority = "MED";
    entity.jam = false;
    entity.category = PE(QString(), type).getCategory(type);

    entity.targetAlt = altitude;
    entity.targetSpeed = entity.speed;
    entity.targetHeading = entity.heading;

    entities[id] = entity;

    // Print creation info with formatting
    std::cout << "\033[1m" << QString("Created %1 (%2)")
                .arg(id)
                .arg(type)
                .toStdString() << "\033[0m" << std::endl;
    std::cout << QString("  Position: %1, %2")
                .arg(lat, 0, 'f', 4)
                .arg(lon, 0, 'f', 4)
                .toStdString() << std::endl;
    std::cout << QString("  Initial: ALT:%1 SPD:%2 HDG:%3")
                .arg(altitude, 0, 'f', 0)
                .arg(entity.speed, 0, 'f', 0)
                .arg(entity.heading, 0, 'f', 0)
                .toStdString() << std::endl;
}

void NetworkedEWAM::setNewTargets(SimulatedEntity& entity) {
    double oldAlt = entity.targetAlt;
    double oldSpd = entity.targetSpeed;
    double oldHdg = entity.targetHeading;

    // Set new target altitude within ±5000 ft of current
    entity.targetAlt = entity.altitude + (qrand() % 10000 - 5000);
    entity.targetAlt = qBound(20000.0, entity.targetAlt, 40000.0);

    // Set new target speed within ±50 knots of current
    entity.targetSpeed = entity.speed + (qrand() % 100 - 50);
    entity.targetSpeed = qBound(300.0, entity.targetSpeed, 600.0);

    // Set new target heading within ±60° of current
    entity.targetHeading = entity.heading + (qrand() % 120 - 60);
    if (entity.targetHeading >= 360) entity.targetHeading -= 360;
    if (entity.targetHeading < 0) entity.targetHeading += 360;

    // Log significant changes
    if (fabs(entity.targetAlt - oldAlt) > 100 ||
        fabs(entity.targetSpeed - oldSpd) > 10 ||
        fabs(entity.targetHeading - oldHdg) > 5) {

        std::cout << QString("  %1 adjusting course:")
                    .arg(entity.id)
                    .toStdString();

        if (fabs(entity.targetAlt - oldAlt) > 100) {
            std::cout << QString(" ALT:%1%2")
                        .arg(entity.targetAlt > oldAlt ? "↑" : "↓")
                        .arg(entity.targetAlt, 0, 'f', 0)
                        .toStdString();
        }
        if (fabs(entity.targetSpeed - oldSpd) > 10) {
            std::cout << QString(" SPD:%1%2")
                        .arg(entity.targetSpeed > oldSpd ? "↑" : "↓")
                        .arg(entity.targetSpeed, 0, 'f', 0)
                        .toStdString();
        }
        if (fabs(entity.targetHeading - oldHdg) > 5) {
            std::cout << QString(" HDG:%1%2")
                        .arg(entity.targetHeading > oldHdg ? "→" : "←")
                        .arg(entity.targetHeading, 0, 'f', 0)
                        .toStdString();
        }
        std::cout << std::endl;
    }
}

void NetworkedEWAM::createSimulatedEmitter(const QString& id, const QString& type,
                                          const QString& category, double lat, double lon) {
    Emitter emitter(
        id, type, category,
        lat, lon,
        8.0 + (qrand() % 20) / 10.0,    // freqMin
        10.0 + (qrand() % 20) / 10.0,   // freqMax
        true,                           // active
        "MED", "MED",                   // priorities
        true, true, false, false,       // capability flags
        false                           // jam
    );
    emitters[id] = emitter;
    std::cout << "Created emitter: " << id.toStdString() << " (" << type.toStdString() << ")" << std::endl;
}

void NetworkedEWAM::updatePosition(SimulatedEntity& entity, double distanceKm) {
    const double R = 6371.0; // Earth radius in km

    double lat1 = entity.lat * M_PI / 180;
    double lon1 = entity.lon * M_PI / 180;
    double bearing = entity.heading * M_PI / 180;

    double angular_distance = distanceKm / R;

    double lat2 = asin(sin(lat1) * cos(angular_distance) +
                      cos(lat1) * sin(angular_distance) * cos(bearing));

    double lon2 = lon1 + atan2(sin(bearing) * sin(angular_distance) * cos(lat1),
                              cos(angular_distance) - sin(lat1) * sin(lat2));

    entity.lat = lat2 * 180 / M_PI;
    entity.lon = lon2 * 180 / M_PI;
}

void NetworkedEWAM::updateDynamics(SimulatedEntity& entity, int deltaMs) {
    const double deltaSeconds = deltaMs / 1000.0;

    // Update heading with turn rate
    if (fabs(entity.heading - entity.targetHeading) > 1.0) {
        double headingDiff = entity.targetHeading - entity.heading;
        // Normalize to -180 to 180
        if (headingDiff > 180) headingDiff -= 360;
        if (headingDiff < -180) headingDiff += 360;

        double turnDirection = (headingDiff > 0) ? 1.0 : -1.0;
        entity.turnRate = 3.0 * turnDirection; // 3 degrees per second

        entity.heading += entity.turnRate * deltaSeconds;
        if (entity.heading >= 360) entity.heading -= 360;
        if (entity.heading < 0) entity.heading += 360;
    }

    // Update altitude
    if (fabs(entity.altitude - entity.targetAlt) > 100) {
        double altDiff = entity.targetAlt - entity.altitude;
        entity.climbRate = (altDiff > 0) ? 2000 : -2000; // ft per minute
        entity.altitude += (entity.climbRate / 60.0) * deltaSeconds;
    }

    // Update speed
    if (fabs(entity.speed - entity.targetSpeed) > 10) {
        double speedDiff = entity.targetSpeed - entity.speed;
        double acceleration = (speedDiff > 0) ? 50 : -50; // knots per minute
        entity.speed += (acceleration / 60.0) * deltaSeconds;
    }
}

void NetworkedEWAM::sendEntityUpdate(const SimulatedEntity& entity) {
    QJsonObject json;
    json["id"] = entity.id;
    json["type"] = entity.type;
    json["lat"] = entity.lat;
    json["lon"] = entity.lon;
    json["altitude"] = entity.altitude;
    json["speed"] = entity.speed;
    json["heading"] = entity.heading;
    json["priority"] = entity.priority;
    json["jam"] = entity.jam;
    json["ghost"] = false;
    json["category"] = static_cast<int>(entity.category);
    json["state"] = "active";
    json["apd"] = entity.priority;  // Using priority as APD for simplicity

    sendJson(json);
}

void NetworkedEWAM::sendEmitterUpdate(const Emitter& emitter) {
    QJsonObject json;
    json["id"] = emitter.id;
    json["type"] = emitter.type;
    json["category"] = emitter.category;
    json["lat"] = emitter.lat;
    json["lon"] = emitter.lon;
    json["freqMin"] = emitter.freqMin;
    json["freqMax"] = emitter.freqMax;
    json["active"] = emitter.active;
    json["eaPriority"] = emitter.eaPriority;
    json["esPriority"] = emitter.esPriority;
    json["jamResponsible"] = emitter.jamResponsible;
    json["reactiveEligible"] = emitter.reactiveEligible;
    json["preemptiveEligible"] = emitter.preemptiveEligible;
    json["consentRequired"] = emitter.consentRequired;
    json["jam"] = emitter.jam;
    json["altitude"] = 0.0;  // Ground-based emitter
    json["heading"] = 0.0;
    json["speed"] = 0.0;
    json["jamIneffective"] = emitter.jamIneffective;
    json["jamEffective"] = emitter.jamEffective;

    sendJson(json);
}


// Client mode functions for testing without EWAM

bool NetworkedEWAM::startServer(quint16 port) {
    if (!server) {
        server = new QTcpServer(this);
        connect(server, &QTcpServer::newConnection, this, &NetworkedEWAM::onNewConnection);
    }

    if (!server->listen(QHostAddress::Any, port)) {
        std::cerr << "Failed to start server on port " << port << ": "
                  << server->errorString().toStdString() << std::endl;
        return false;
    }

    std::cout << "Server listening on port " << port << std::endl;
    return true;
}

void NetworkedEWAM::stopServer() {
    if (server) {
        server->close();
        for (QTcpSocket* client : clients) {
            client->disconnectFromHost();
        }
        qDeleteAll(clients);
        clients.clear();
    }
}

void NetworkedEWAM::onNewConnection() {
    QTcpSocket* clientSocket = server->nextPendingConnection();
    if (!clientSocket) return;

    connect(clientSocket, &QTcpSocket::readyRead, this, &NetworkedEWAM::onReadyRead);
    connect(clientSocket, &QTcpSocket::disconnected, this, &NetworkedEWAM::onClientDisconnected);

    clients.append(clientSocket);
    std::cout << "New client connected. Total clients: " << clients.size() << std::endl;
}

void NetworkedEWAM::onReadyRead() {
    QTcpSocket* clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;

    while (clientSocket->bytesAvailable() > 0) {
        QByteArray data = clientSocket->readLine();
        if (!data.isEmpty()) {
            handleReceivedData(data);

            // Echo back to all clients in server mode
            for (QTcpSocket* client : clients) {
                client->write(data);
                client->flush();
            }
        }
    }
}

void NetworkedEWAM::onClientDisconnected() {
    QTcpSocket* clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;

    clients.removeOne(clientSocket);
    clientSocket->deleteLater();
    std::cout << "Client disconnected. Remaining clients: " << clients.size() << std::endl;
}

void NetworkedEWAM::handleReceivedData(const QByteArray& data) {
    std::cout << "Received: " << QString(data).trimmed().toStdString() << std::endl;

    try {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject json = doc.object();
            // Process received JSON data
            if (json.contains("id")) {
                std::cout << "Received entity/emitter update for ID: "
                         << json["id"].toString().toStdString() << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error processing received data: " << e.what() << std::endl;
    }
}

void NetworkedEWAM::sendTestMessage(const QString& message) {
    QJsonObject json;
    json["type"] = "test";
    json["message"] = message;
    json["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    sendJson(json);
}
