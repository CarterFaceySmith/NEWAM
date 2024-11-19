#ifndef NETWORKEDEWAM_H
#define NETWORKEDEWAM_H

#include <QObject>
#include <QTcpSocket>
#include <QTcpServer>
#include <QMap>
#include "../AbstractNetworkInterface/pe.h"
#include "../AbstractNetworkInterface/emitter.h"

struct SimulatedEntity {
    QString id;
    QString type;
    double lat;
    double lon;
    double altitude;
    double speed;       // knots
    double heading;     // degrees
    double turnRate;    // degrees per second
    double climbRate;   // feet per minute
    QString priority;
    bool jam;
    PE::PECategory category;

    // Target values for smooth transitions
    double targetAlt;
    double targetSpeed;
    double targetHeading;
};

class NetworkedEWAM : public QObject {
    Q_OBJECT

public:
    explicit NetworkedEWAM(QObject *parent = nullptr);
    ~NetworkedEWAM();

    // Client mode methods
    void initializeSimulation(const QString& scenario);
    void updateSimulation(int deltaMs);

    void connectToHost(const QString& host, quint16 port);
    void setReconnectInterval(int msecs) { reconnectInterval = msecs; }
    bool isConnected() const { return socket->state() == QAbstractSocket::ConnectedState; }

    // Server mode methods
    bool startServer(quint16 port);
    void stopServer();
    bool isServerMode() const { return server != nullptr; }

    // Echo mode (for testing)
    void sendTestMessage(const QString& message);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void tryReconnect();

private:
    void createSimulatedEntity(const QString& id, const QString& type,
                             double lat, double lon, double altitude);
    void createSimulatedEmitter(const QString& id, const QString& type,
                               const QString& category, double lat, double lon);
    void updatePosition(SimulatedEntity& entity, double distanceKm);
    void updateDynamics(SimulatedEntity& entity, int deltaMs);
    void setNewTargets(SimulatedEntity& entity);
    bool sendJson(const QJsonObject& json);
    void sendEntityUpdate(const SimulatedEntity& entity);
    void sendEmitterUpdate(const Emitter& emitter);
    void handleReceivedData(const QByteArray& data);

    QTcpSocket* socket;
    QString currentHost;
    quint16 currentPort;
    QTimer* reconnectTimer;
    int reconnectInterval;
    int reconnectAttempts;
    const int MAX_RECONNECT_ATTEMPTS = 5;
    bool autoReconnect;
    QTcpServer* server;          // For server mode
    QList<QTcpSocket*> clients;  // Connected clients in server mode
    QMap<QString, SimulatedEntity> entities;
    QMap<QString, Emitter> emitters;
    QByteArray buffer;           // For accumulating incoming data
};

#endif // NETWORKEDEWAM_H
