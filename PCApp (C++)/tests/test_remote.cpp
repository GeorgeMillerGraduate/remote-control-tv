#include "server.h"
#include <QApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QSslSocket>
#include <QTemporaryDir>
#include <QtTest>

class FakeInput final : public InputBackend {
  public:
    int movements = 0, releases = 0, keys = 0;
    void move(int, int) override {
        ++movements;
    }
    void point(int, double, double) override {
        ++movements;
    }
    void button(int, bool) override {}
    void scroll(int) override {}
    void key(int, bool) override {
        ++keys;
    }
    void text(const QString &) override {}
    void media(const QString &) override {}
    void releaseAll() noexcept override {
        ++releases;
    }
};
class RemoteTests : public QObject {
    Q_OBJECT
  private slots:
    void ticketSingleUse() {
        PairingTicket ticket(1000);
        QVERIFY(!ticket.consume(ticket.id, "bad", 1001));
        QVERIFY(ticket.consume(ticket.id, ticket.secret, 1001));
        QVERIFY(!ticket.consume(ticket.id, ticket.secret, 1002));
    }
    void expiredAndCancelled() {
        PairingTicket ticket(1000);
        QVERIFY(!ticket.consume(ticket.id, ticket.secret, 121000));
        PairingTicket pending(1000);
        QVERIFY(pending.consume(pending.id, pending.secret, 1001));
        pending.cancel();
        QVERIFY(!pending.approvalAllowed(1002));
    }
    void credentials() {
        const QString a = Security::randomToken(), b = Security::randomToken();
        QCOMPARE(a.size(), 64);
        QVERIFY(a != b);
        QVERIFY(Security::equal(a, a));
        QVERIFY(!Security::equal(a, b));
        QCOMPARE(Security::hash("abc"),
                 QString("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    }
    void persistentIdentity() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Identity first(directory.path()), second(directory.path());
        QCOMPARE(first.fingerprint(), second.fingerprint());
        QCOMPARE(first.fingerprint().size(), 64);
        QVERIFY(!first.certificate.isNull());
        QVERIFY(!first.privateKey.isNull());
#ifndef Q_OS_WIN
        auto permissions = QFile::permissions(directory.path() + "/identity.json");
        QVERIFY(!(permissions &
                  (QFile::ReadOther | QFile::WriteOther | QFile::ReadGroup | QFile::WriteGroup)));
#endif
    }
    void pairingControlReconnectAndRevoke() {
        QTemporaryDir directory;
        Identity identity(directory.path());
        FakeInput input;
        RemoteServer server(identity, input);
        server.start(0);
        const auto qr = server.createPairing("127.0.0.1");
        QCOMPARE(qr.value("protocol").toString(), QString("JengaRemote"));
        QCOMPARE(qr.value("serverPublicKeyFingerprint").toString(), identity.fingerprint());
        QSignalSpy requests(&server, &RemoteServer::pairingRequested);
        QWebSocket phone;
        connect(&phone, &QWebSocket::sslErrors, &phone,
                [&phone, &identity](const QList<QSslError> &) {
                    if (Security::hash(phone.sslConfiguration().peerCertificate().toDer()) ==
                        identity.fingerprint())
                        phone.ignoreSslErrors();
                });
        QList<QJsonObject> responses;
        connect(&phone, &QWebSocket::textMessageReceived, &phone, [&](const QString &text) {
            responses.append(QJsonDocument::fromJson(text.toUtf8()).object());
        });
        phone.open(QUrl(QString("wss://127.0.0.1:%1/").arg(server.port())));
        QTRY_COMPARE(phone.state(), QAbstractSocket::ConnectedState);
        qint64 sequence = 0;
        auto send = [&](const QString &type, QJsonObject fields = {}) {
            fields.insert("version", 1);
            fields.insert("seq", ++sequence);
            fields.insert("type", type);
            phone.sendTextMessage(
                QString::fromUtf8(QJsonDocument(fields).toJson(QJsonDocument::Compact)));
        };
        send("PAIR_REQUEST", {{"pairingId", qr.value("pairingId")},
                              {"pairingSecret", qr.value("pairingSecret")},
                              {"deviceName", "Test Android"}});
        QTRY_COMPARE(requests.size(), 1);
        QVERIFY(server.devices().isEmpty());
        QCOMPARE(input.movements, 0);
        const QString requestId = requests.at(0).at(0).toString();
        server.approve(requestId, true);
        auto approval = [&] {
            for (const auto &response : responses)
                if (response.value("type") == "PAIR_APPROVAL")
                    return response;
            return QJsonObject{};
        };
        QTRY_VERIFY(!approval().isEmpty());
        const auto credentials = approval();
        send("MOUSE_MOVE", {{"dx", 10}, {"dy", -2}});
        QTRY_COMPARE(input.movements, 1);
        // Replaying the same sequence must not execute another input.
        phone.sendTextMessage(QString::fromUtf8(QJsonDocument(QJsonObject{{"version", 1},
                                                                          {"seq", sequence},
                                                                          {"type", "MOUSE_MOVE"},
                                                                          {"dx", 10},
                                                                          {"dy", 1}})
                                                    .toJson()));
        QTest::qWait(100);
        QCOMPARE(input.movements, 1);
        phone.close();
        QTRY_COMPARE(phone.state(), QAbstractSocket::UnconnectedState);
        QTest::qWait(50);
        responses.clear();
        sequence = 0;
        phone.open(QUrl(QString("wss://127.0.0.1:%1/").arg(server.port())));
        QTRY_COMPARE(phone.state(), QAbstractSocket::ConnectedState);
        send("AUTH_REQUEST", {{"deviceId", credentials.value("deviceId")},
                              {"credential", credentials.value("credential")}});
        auto authed = [&] {
            for (const auto &response : responses)
                if (response.value("type") == "AUTH_SUCCESS")
                    return true;
            return false;
        };
        QTRY_VERIFY(authed());
        send("KEY_DOWN", {{"key", 10}});
        QTRY_COMPARE(input.keys, 1);
        // Viewing must stay disabled by default.
        send("SCREEN_REQUEST", {{"monitor", 0}, {"quality", 60}});
        QTest::qWait(100);
        bool refused = false;
        for (const auto &response : responses)
            if (response.value("message").toString() == "Permission disabled on PC")
                refused = true;
        QVERIFY(refused);
        server.revoke(credentials.value("deviceId").toString());
        QTRY_COMPARE(phone.state(), QAbstractSocket::UnconnectedState);
        QVERIFY(server.devices().isEmpty());
        QVERIFY(input.releases > 0);
        responses.clear();
        sequence = 0;
        phone.open(QUrl(QString("wss://127.0.0.1:%1/").arg(server.port())));
        QTRY_COMPARE(phone.state(), QAbstractSocket::ConnectedState);
        send("AUTH_REQUEST", {{"deviceId", credentials.value("deviceId")},
                              {"credential", credentials.value("credential")}});
        QTRY_COMPARE(phone.state(), QAbstractSocket::UnconnectedState);
        QVERIFY(!authed());
        server.stop();
    }
};
QTEST_MAIN(RemoteTests)
#include "test_remote.moc"
