#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QSslCertificate>
#include <QSslKey>
#include <QString>

namespace Security {
QString randomToken();
QString hash(const QByteArray &data);
bool equal(const QString &left, const QString &right);
void protectDirectory(const QString &path);
void writePrivate(const QString &path, const QByteArray &bytes);
QByteArray readFile(const QString &path);
} // namespace Security

class PairingTicket {
  public:
    QString id = Security::randomToken();
    QString secret = Security::randomToken();
    qint64 expiresAt;
    explicit PairingTicket(qint64 now);
    bool consume(const QString &id, const QString &secret, qint64 now);
    bool approvalAllowed(qint64 now) const;
    void cancel();

  private:
    bool consumed = false;
    bool cancelled = false;
};

class Identity {
  public:
    explicit Identity(const QString &directory);
    QString directory;
    QSslCertificate certificate;
    QSslKey privateKey;
    QString fingerprint() const;
};
