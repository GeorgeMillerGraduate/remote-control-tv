#include "server.h"
#include <QBuffer>
#include <QCursor>
#include <QDateTime>
#include <QFile>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QNetworkDatagram>
#include <QPainter>
#include <QScreen>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QSysInfo>
#include <QUuid>
#include <QtConcurrent>
#include <cmath>
#include <stdexcept>

namespace {
qint64 now() {
    return QDateTime::currentMSecsSinceEpoch();
}
QString field(const QJsonObject &object, const QString &name, int maximum) {
    const auto value = object.value(name);
    if (!value.isString() || value.toString().size() > maximum)
        throw std::runtime_error("Invalid text field");
    return value.toString();
}
qint64 number(const QJsonObject &object, const QString &name, qint64 minimum, qint64 maximum) {
    const auto value = object.value(name);
    const double data = value.toDouble();
    if (!value.isDouble() || !std::isfinite(data) || std::floor(data) != data || data < minimum ||
        data > maximum)
        throw std::runtime_error("Invalid numeric field");
    return static_cast<qint64>(data);
}
bool flag(const QJsonObject &object, const QString &name) {
    if (!object.value(name).isBool())
        throw std::runtime_error("Boolean required");
    return object.value(name).toBool();
}
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
} // namespace
RemoteServer::RemoteServer(Identity &identity, InputBackend &input, QObject *parent)
    : QObject(parent), identity(identity), input(input),
      listener("JengaRemote", QWebSocketServer::SecureMode, this) {
    QSslConfiguration tls = QSslConfiguration::defaultConfiguration();
    tls.setLocalCertificate(identity.certificate);
    tls.setPrivateKey(identity.privateKey);
    tls.setPeerVerifyMode(QSslSocket::VerifyNone); // Client authenticates inside pinned TLS, not by
                                                   // a client certificate.
    tls.setProtocol(QSsl::TlsV1_2OrLater);
    listener.setSslConfiguration(tls);
    listener.setMaxPendingConnections(8);
    listener.setHandshakeTimeout(10000);
    const QString trustPath = identity.directory + "/trusted.json";
    if (QFile::exists(trustPath)) {
        QJsonParseError error{};
        auto data = QJsonDocument::fromJson(Security::readFile(trustPath), &error);
        require(error.error == QJsonParseError::NoError && data.isObject(),
                "Trust registry is invalid");
        trusted = data.object();
        require(trusted.size() <= 32, "Trust registry limit exceeded");
    }
    connect(&listener, &QWebSocketServer::newConnection, this, &RemoteServer::incoming);
    connect(&discovery, &QUdpSocket::readyRead, this, [this] {
        while (discovery.hasPendingDatagrams()) {
            auto packet = discovery.receiveDatagram(128);
            if (packet.data() != "JENGA_DISCOVER_1" || now() < discoveryNext)
                continue;
            // Hints contain no credentials. Spoofing cannot change Android's pinned identity.
            discoveryNext = now() + 100;
            QJsonObject response{{"protocol", "JengaRemote"},
                                 {"version", 1},
                                 {"port", port()},
                                 {"serverPublicKeyFingerprint", this->identity.fingerprint()}};
            discovery.writeDatagram(QJsonDocument(response).toJson(QJsonDocument::Compact),
                                    packet.senderAddress(), packet.senderPort());
        }
    });
    watchdog.setInterval(1000);
    connect(&watchdog, &QTimer::timeout, this, [this] {
        for (auto *socket : sessions.keys()) {
            const auto session = sessions.value(socket);
            if (now() - session.last > (session.id.isEmpty() ? 30000 : 45000))
                socket->close(QWebSocketProtocol::CloseCodePolicyViolated, "Session timeout");
        }
        if (ticket && !ticket->approvalAllowed(now()))
            cancelPairing();
    });
}
RemoteServer::~RemoteServer() {
    stop();
}
void RemoteServer::start(quint16 port) {
    require(!running(), "Server is already running");
    require(listener.listen(QHostAddress::Any, port),
            "Cannot listen on this port; stop the Java server or choose another port");
    if (!discovery.bind(QHostAddress::AnyIPv4, port))
        emit log("LAN discovery unavailable; QR pairing still works");
    watchdog.start();
    emit log(QString("Encrypted server listening on TCP %1").arg(port));
}
void RemoteServer::stop() {
    watchdog.stop();
    cancelPairing();
    emergency();
    discovery.close();
    listener.close();
}
bool RemoteServer::running() const {
    return listener.isListening();
}
quint16 RemoteServer::port() const {
    return listener.serverPort();
}
QJsonObject RemoteServer::createPairing(const QString &host) {
    require(running(), "Start the server first");
    require(!host.trimmed().isEmpty(), "Choose a reachable LAN or VPN address");
    cancelPairing();
    ticket = std::make_shared<PairingTicket>(now());
    return {{"protocol", "JengaRemote"},
            {"version", 1},
            {"deviceName", QSysInfo::machineHostName()},
            {"host", host.trimmed()},
            {"port", port()},
            {"pairingId", ticket->id},
            {"pairingSecret", ticket->secret},
            {"expiresAt", ticket->expiresAt},
            {"serverPublicKeyFingerprint", this->identity.fingerprint()}};
}
void RemoteServer::cancelPairing() {
    if (ticket)
        ticket->cancel();
    ticket.reset();
    if (pendingSocket) {
        auto *socket = pendingSocket.data();
        clearPending();
        socket->close(QWebSocketProtocol::CloseCodePolicyViolated, "Pairing cancelled or expired");
    }
}
void RemoteServer::incoming() {
    while (listener.hasPendingConnections()) {
        auto *socket = listener.nextPendingConnection();
        if (sessions.size() >= 8) {
            socket->close(QWebSocketProtocol::CloseCodePolicyViolated, "Server busy");
            socket->deleteLater();
            continue;
        }
        socket->setParent(this);
        socket->setMaxAllowedIncomingFrameSize(16384);
        socket->setMaxAllowedIncomingMessageSize(16384);
        Session session;
        session.last = session.window = now();
        sessions.insert(socket, session);
        connect(socket, &QWebSocket::textMessageReceived, this,
                [this, socket](const QString &text) { message(socket, text); });
        connect(socket, &QWebSocket::binaryMessageReceived, this, [socket](const QByteArray &) {
            socket->close(QWebSocketProtocol::CloseCodeDatatypeNotSupported, "JSON required");
        });
        connect(socket, &QWebSocket::disconnected, this, [this, socket] {
            drop(socket);
            socket->deleteLater();
        });
        send(socket, "HELLO");
    }
}
void RemoteServer::send(QWebSocket *socket, const QString &type, QJsonObject fields) {
    if (!socket || socket->state() != QAbstractSocket::ConnectedState)
        return;
    fields.insert("version", 1);
    fields.insert("type", type);
    socket->sendTextMessage(
        QString::fromUtf8(QJsonDocument(fields).toJson(QJsonDocument::Compact)));
}
void RemoteServer::message(QWebSocket *socket, const QString &text) {
    if (!sessions.contains(socket) || socket->state() != QAbstractSocket::ConnectedState)
        return;
    try {
        require(text.toUtf8().size() <= 16384, "Message too large");
        auto &session = sessions[socket];
        if (now() - session.window >= 1000) {
            session.window = now();
            session.count = 0;
        }
        require(++session.count <= 240, "Rate limit exceeded");
        QJsonParseError error{};
        const auto document = QJsonDocument::fromJson(text.toUtf8(), &error);
        require(error.error == QJsonParseError::NoError && document.isObject(), "Invalid JSON");
        const auto object = document.object();
        require(number(object, "version", 1, 1) == 1, "Unsupported version");
        const qint64 sequence = number(object, "seq", 1, 9007199254740991LL);
        require(sequence > session.sequence, "Invalid sequence");
        session.sequence = sequence;
        session.last = now();
        const QString type = field(object, "type", 40);
        if (session.id.isEmpty()) {
            require(!session.pending, "Approval is pending");
            if (type == "PAIR_REQUEST") {
                require(ticket && !pendingSocket &&
                            ticket->consume(field(object, "pairingId", 100),
                                            field(object, "pairingSecret", 100), now()),
                        "Invalid or expired QR code");
                pendingName = field(object, "deviceName", 80);
                pendingId = QUuid::createUuid().toString(QUuid::WithoutBraces);
                session.pending = true;
                pendingSocket = socket;
                emit pairingRequested(pendingId, pendingName, socket->peerAddress().toString());
                return;
            }
            require(type == "AUTH_REQUEST", "Authenticate before sending commands");
            const QString id = field(object, "deviceId", 100),
                          credential = field(object, "credential", 100);
            require(trusted.contains(id) &&
                        Security::equal(trusted.value(id).toObject().value("hash").toString(),
                                        Security::hash(credential.toUtf8())),
                    "Authentication failed");
            authenticate(socket, id);
            return;
        }
        if (type == "PING")
            send(socket, "PONG", {{"time", now()}});
        else if (type == "DISCONNECT")
            socket->close();
        else if (type == "MOUSE_MOVE") {
            require(permissions.mouse, "Permission disabled on PC");
            input.move(number(object, "dx", -500, 500), number(object, "dy", -500, 500));
        } else if (type == "MOUSE_BUTTON") {
            require(permissions.mouse, "Permission disabled on PC");
            input.button(number(object, "button", 1, 3), flag(object, "down"));
        } else if (type == "MOUSE_SCROLL") {
            require(permissions.mouse, "Permission disabled on PC");
            input.scroll(number(object, "amount", -30, 30));
        } else if (type == "KEY_DOWN" || type == "KEY_UP") {
            require(permissions.keyboard, "Permission disabled on PC");
            input.key(number(object, "key", 8, 524), type == "KEY_DOWN");
        } else if (type == "TEXT_INPUT") {
            require(permissions.keyboard, "Permission disabled on PC");
            input.text(field(object, "text", 4000));
        } else if (type == "MEDIA_CONTROL") {
            require(permissions.media, "Permission disabled on PC");
            input.media(field(object, "action", 30));
        } else if (type == "SCREEN_REQUEST") {
            require(permissions.screen, "Permission disabled on PC");
            frame(socket, number(object, "monitor", 0, QGuiApplication::screens().size() - 1),
                  number(object, "quality", 20, 85));
        } else if (type == "DESKTOP_POINT") {
            require(permissions.mouse && permissions.screen, "Permission disabled on PC");
            require(object.value("x").isDouble() && object.value("y").isDouble(), "Invalid point");
            const double x = object.value("x").toDouble(), y = object.value("y").toDouble();
            require(std::isfinite(x) && std::isfinite(y) && x >= 0 && x <= 1 && y >= 0 && y <= 1,
                    "Invalid point");
            input.point(number(object, "monitor", 0, QGuiApplication::screens().size() - 1), x, y);
        } else
            throw std::runtime_error("Unsupported command");
    } catch (const std::exception &error) {
        if (controller == socket)
            input.releaseAll();
        send(socket, "ERROR", {{"message", QString::fromUtf8(error.what())}});
        if (sessions.value(socket).id.isEmpty())
            socket->close(QWebSocketProtocol::CloseCodePolicyViolated, "Authentication failed");
    }
}
void RemoteServer::clearPending() {
    pendingSocket.clear();
    pendingId.clear();
    pendingName.clear();
    emit pairingEnded();
}
void RemoteServer::approve(const QString &requestId, bool accepted) {
    if (!pendingSocket || pendingId != requestId)
        return;
    QWebSocket *socket = pendingSocket;
    const QString name = pendingName;
    const bool allowed = accepted && ticket && ticket->approvalAllowed(now()) &&
                         sessions.contains(socket) &&
                         socket->state() == QAbstractSocket::ConnectedState;
    clearPending();
    if (!allowed) {
        send(socket, "PAIR_REJECTED", {{"message", "Pairing rejected or expired"}});
        socket->close();
        return;
    }
    if (controller || trusted.size() >= 32) {
        send(socket, "PAIR_REJECTED",
             {{"message", "Another controller is active or the trust list is full"}});
        socket->close();
        return;
    }
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces),
                  credential = Security::randomToken();
    trusted.insert(id, QJsonObject{{"hash", Security::hash(credential.toUtf8())}, {"name", name}});
    try {
        saveTrust();
        authenticate(socket, id);
        send(socket, "PAIR_APPROVAL", {{"deviceId", id}, {"credential", credential}});
        emit log("New phone approved and trusted");
        emit paired();
    } catch (const std::exception &) {
        trusted.remove(id);
        try {
            saveTrust();
        } catch (...) {
        }
        send(socket, "ERROR", {{"message", "Could not save trusted device"}});
        socket->close();
    }
}
void RemoteServer::authenticate(QWebSocket *socket, const QString &id) {
    require(!controller || controller == socket, "Another phone is controlling this PC");
    require(sessions.contains(socket), "Connection ended");
    auto device = trusted.value(id).toObject();
    device.insert("lastConnected", now());
    trusted.insert(id, device);
    saveTrust();
    sessions[socket].id = id;
    sessions[socket].pending = false;
    controller = socket;
    send(socket, "AUTH_SUCCESS", {{"monitors", QGuiApplication::screens().size()}});
    emit activeChanged(true, device.value("name").toString());
    emit log("Remote control connected");
}
void RemoteServer::drop(QWebSocket *socket) {
    if (controller == socket) {
        input.releaseAll();
        controller.clear();
        emit activeChanged(false, {});
        emit log("Remote control disconnected");
    }
    if (pendingSocket == socket)
        clearPending();
    sessions.remove(socket);
}
void RemoteServer::saveTrust() {
    Security::writePrivate(identity.directory + "/trusted.json", QJsonDocument(trusted).toJson());
}
void RemoteServer::emergency() {
    input.releaseAll();
    for (auto *socket : sessions.keys()) {
        drop(socket);
        socket->close(QWebSocketProtocol::CloseCodeNormal, "Disconnected by PC");
    }
}
void RemoteServer::setPermissions(Permissions value) {
    permissions = value;
    emergency();
}
QJsonObject RemoteServer::devices() const {
    return trusted;
}
void RemoteServer::revoke(const QString &id) {
    const auto previous = trusted;
    trusted.remove(id);
    try {
        saveTrust();
    } catch (...) {
        trusted = previous;
        throw;
    }
    disconnectDevice(id);
    emit log("Phone trust revoked");
}
void RemoteServer::rename(const QString &id, const QString &name) {
    require(trusted.contains(id) && !name.isEmpty() && name.size() <= 80, "Invalid device name");
    auto previous = trusted;
    auto entry = trusted.value(id).toObject();
    entry.insert("name", name);
    trusted.insert(id, entry);
    try {
        saveTrust();
    } catch (...) {
        trusted = previous;
        throw;
    }
}
void RemoteServer::disconnectDevice(const QString &id) {
    for (auto *socket : sessions.keys())
        if (sessions.value(socket).id == id) {
            drop(socket);
            socket->close(QWebSocketProtocol::CloseCodeNormal, "Disconnected by PC");
        }
}
void RemoteServer::frame(QWebSocket *socket, int monitor, int quality) {
    if (captureBusy || socket->bytesToWrite() > 0)
        return;
    const auto screens = QGuiApplication::screens();
    QImage image = screens[monitor]->grabWindow(0).toImage();
    require(!image.isNull(), "Screen capture is unavailable");
    QPainter painter(&image);
    const QPoint logical = QCursor::pos() - screens[monitor]->geometry().topLeft();
    const qreal ratio = image.devicePixelRatio();
    painter.setPen(QPen(Qt::black, 2));
    painter.setBrush(Qt::white);
    painter.drawEllipse(logical, 4, 4);
    painter.end();
    Q_UNUSED(ratio);
    captureBusy = true;
    auto *watcher = new QFutureWatcher<QByteArray>(this);
    QPointer<QWebSocket> target = socket;
    connect(watcher, &QFutureWatcher<QByteArray>::finished, this, [this, watcher, target] {
        const QByteArray bytes = watcher->result();
        watcher->deleteLater();
        captureBusy = false;
        if (bytes.isEmpty()) {
            if (target)
                send(target, "ERROR", {{"message", "JPEG encoding unavailable"}});
            return;
        }
        if (target && sessions.contains(target) && controller == target && permissions.screen &&
            target->state() == QAbstractSocket::ConnectedState && bytes.size() < 2000000 &&
            target->bytesToWrite() == 0)
            target->sendBinaryMessage(bytes);
    });
    watcher->setFuture(QtConcurrent::run([image, quality] {
        QImage scaled =
            image.width() > 1280 ? image.scaledToWidth(1280, Qt::SmoothTransformation) : image;
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        if (!scaled.save(&buffer, "JPEG", quality))
            return QByteArray{};
        return bytes;
    }));
}
