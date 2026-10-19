#pragma once
#include "input.h"
#include "security.h"
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUdpSocket>
#include <QWebSocket>
#include <QWebSocketServer>
#include <memory>

struct Permissions {
    bool mouse = true, keyboard = true, media = true, screen = false;
};
class RemoteServer final : public QObject {
    Q_OBJECT
  public:
    RemoteServer(Identity &identity, InputBackend &input, QObject *parent = nullptr);
    ~RemoteServer() override;
    void start(quint16 port);
    void stop();
    bool running() const;
    quint16 port() const;
    QJsonObject createPairing(const QString &host);
    void cancelPairing();
    void approve(const QString &requestId, bool accepted);
    void emergency();
    void setPermissions(Permissions value);
    QJsonObject devices() const;
    void revoke(const QString &id);
    void rename(const QString &id, const QString &name);
    void disconnectDevice(const QString &id);
  signals:
    void log(const QString &text);
    void activeChanged(bool active, const QString &name);
    void pairingRequested(const QString &requestId, const QString &name, const QString &peer);
    void pairingEnded();
    void paired();

  private:
    struct Session {
        QString id;
        qint64 sequence = 0, last = 0, window = 0;
        int count = 0;
        bool pending = false;
    };
    Identity &identity;
    InputBackend &input;
    QWebSocketServer listener;
    QUdpSocket discovery;
    QTimer watchdog;
    QHash<QWebSocket *, Session> sessions;
    QPointer<QWebSocket> controller;
    QPointer<QWebSocket> pendingSocket;
    QString pendingId, pendingName;
    std::shared_ptr<PairingTicket> ticket;
    QJsonObject trusted;
    Permissions permissions;
    bool captureBusy = false;
    qint64 discoveryNext = 0;
    void incoming();
    void message(QWebSocket *socket, const QString &text);
    void drop(QWebSocket *socket);
    void authenticate(QWebSocket *socket, const QString &id);
    void saveTrust();
    void clearPending();
    void frame(QWebSocket *socket, int monitor, int quality);
    static void send(QWebSocket *socket, const QString &type, QJsonObject fields = {});
};
