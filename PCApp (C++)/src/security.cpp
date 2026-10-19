#include "security.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <memory>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <stdexcept>
#ifdef Q_OS_WIN
#include <sddl.h>
#include <wincrypt.h>
#include <windows.h>
#endif

namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
QByteArray bioBytes(BIO *bio) {
    char *data = nullptr;
    const long size = BIO_get_mem_data(bio, &data);
    require(size > 0, "Certificate encoding failed");
    return QByteArray(data, static_cast<int>(size));
}
#ifdef Q_OS_WIN
void privateAcl(const QString &path, bool directory) {
    HANDLE token = nullptr;
    require(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token), "Cannot inspect account");
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    QByteArray buffer(static_cast<int>(size), 0);
    const bool ok = GetTokenInformation(token, TokenUser, buffer.data(), size, &size);
    CloseHandle(token);
    require(ok, "Cannot inspect user SID");
    LPWSTR sid = nullptr;
    require(ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(buffer.data())->User.Sid, &sid),
            "SID conversion failed");
    QString rule =
        QString("D:P(A;%1;FA;;;%2)").arg(directory ? "OICI" : "", QString::fromWCharArray(sid));
    LocalFree(sid);
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    require(ConvertStringSecurityDescriptorToSecurityDescriptorW(
                reinterpret_cast<LPCWSTR>(rule.utf16()), SDDL_REVISION_1, &descriptor, nullptr),
            "Private ACL creation failed");
    const bool secured = SetFileSecurityW(
        reinterpret_cast<LPCWSTR>(path.utf16()),
        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, descriptor);
    LocalFree(descriptor);
    require(secured, "Cannot protect application files");
}
QByteArray protectKey(const QByteArray &bytes, bool decrypt) {
    DATA_BLOB in{static_cast<DWORD>(bytes.size()),
                 reinterpret_cast<BYTE *>(const_cast<char *>(bytes.data()))};
    DATA_BLOB out{};
    const bool ok = decrypt ? CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr,
                                                 CRYPTPROTECT_UI_FORBIDDEN, &out)
                            : CryptProtectData(&in, L"Jenga Remote server identity", nullptr,
                                               nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out);
    require(ok, "Windows account key protection failed");
    QByteArray result(reinterpret_cast<char *>(out.pbData), static_cast<int>(out.cbData));
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return result;
}
#else
QByteArray protectKey(const QByteArray &bytes, bool) {
    return bytes;
}
#endif
} // namespace

QString Security::randomToken() {
    QByteArray bytes(32, 0);
    require(RAND_bytes(reinterpret_cast<unsigned char *>(bytes.data()), bytes.size()) == 1,
            "Secure randomness unavailable");
    return QString::fromLatin1(bytes.toHex());
}
QString Security::hash(const QByteArray &data) {
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}
bool Security::equal(const QString &left, const QString &right) {
    const QByteArray a = left.toUtf8(), b = right.toUtf8();
    return a.size() == b.size() &&
           CRYPTO_memcmp(a.constData(), b.constData(), static_cast<size_t>(a.size())) == 0;
}
void Security::protectDirectory(const QString &path) {
    require(QDir().mkpath(path), "Cannot create application directory");
#ifdef Q_OS_WIN
    privateAcl(path, true);
#else
    require(QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner),
            "Cannot protect application directory");
#endif
}
void Security::writePrivate(const QString &path, const QByteArray &bytes) {
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    require(file.open(QIODevice::WriteOnly), "Cannot create private state file");
    require(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner),
            "Cannot set private file permissions");
    require(file.write(bytes) == bytes.size(), "Private state write failed");
    require(file.commit(), "Atomic private state save failed");
#ifdef Q_OS_WIN
    privateAcl(path, false);
#endif
}
QByteArray Security::readFile(const QString &path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Cannot read application state");
    require(file.size() < 1024 * 1024, "Application state file too large");
    return file.readAll();
}
PairingTicket::PairingTicket(qint64 now) : expiresAt(now + 120000) {}
bool PairingTicket::consume(const QString &suppliedId, const QString &suppliedSecret, qint64 now) {
    if (consumed || cancelled || now >= expiresAt || !Security::equal(id, suppliedId) ||
        !Security::equal(secret, suppliedSecret))
        return false;
    consumed = true;
    return true;
}
bool PairingTicket::approvalAllowed(qint64 now) const {
    return !cancelled && now < expiresAt;
}
void PairingTicket::cancel() {
    cancelled = true;
}

Identity::Identity(const QString &directory) : directory(directory) {
    Security::protectDirectory(directory);
    const QString path = directory + "/identity.json";
    if (!QFile::exists(path)) {
        using Context = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
        Context context(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr), EVP_PKEY_CTX_free);
        require(context && EVP_PKEY_keygen_init(context.get()) > 0 &&
                    EVP_PKEY_CTX_set_rsa_keygen_bits(context.get(), 3072) > 0,
                "RSA setup failed");
        EVP_PKEY *raw = nullptr;
        require(EVP_PKEY_keygen(context.get(), &raw) > 0, "Key generation failed");
        std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(raw, EVP_PKEY_free);
        std::unique_ptr<X509, decltype(&X509_free)> cert(X509_new(), X509_free);
        require(static_cast<bool>(cert), "Certificate allocation failed");
        require(X509_set_version(cert.get(), 2) == 1, "Certificate version failed");
        QByteArray serialBytes = QByteArray::fromHex(Security::randomToken().left(16).toLatin1());
        serialBytes[0] = static_cast<char>(serialBytes[0] & 0x7f);
        std::unique_ptr<BIGNUM, decltype(&BN_free)> serial(
            BN_bin2bn(reinterpret_cast<const unsigned char *>(serialBytes.data()),
                      serialBytes.size(), nullptr),
            BN_free);
        require(serial && BN_to_ASN1_INTEGER(serial.get(), X509_get_serialNumber(cert.get())),
                "Certificate serial failed");
        require(X509_gmtime_adj(X509_getm_notBefore(cert.get()), -300) &&
                    X509_gmtime_adj(X509_getm_notAfter(cert.get()), 315360000L),
                "Certificate dates failed");
        require(X509_set_pubkey(cert.get(), key.get()) == 1, "Certificate key failed");
        X509_NAME *name = X509_get_subject_name(cert.get());
        require(X509_NAME_add_entry_by_txt(
                    name, "CN", MBSTRING_ASC,
                    reinterpret_cast<const unsigned char *>("JengaRemoteCpp"), -1, -1, 0) == 1,
                "Certificate name failed");
        require(X509_set_issuer_name(cert.get(), name) == 1, "Certificate issuer failed");
        for (const auto &extension :
             {std::pair<int, const char *>{NID_basic_constraints, "critical,CA:FALSE"},
              {NID_key_usage, "critical,digitalSignature,keyEncipherment"},
              {NID_ext_key_usage, "serverAuth"}}) {
            X509_EXTENSION *value = X509V3_EXT_conf_nid(nullptr, nullptr, extension.first,
                                                        const_cast<char *>(extension.second));
            require(value, "Certificate extension failed");
            const bool added = X509_add_ext(cert.get(), value, -1) == 1;
            X509_EXTENSION_free(value);
            require(added, "Certificate extension insertion failed");
        }
        require(X509_sign(cert.get(), key.get(), EVP_sha256()) > 0, "Certificate signing failed");
        std::unique_ptr<BIO, decltype(&BIO_free)> privateBio(BIO_new(BIO_s_mem()), BIO_free),
            certBio(BIO_new(BIO_s_mem()), BIO_free);
        require(privateBio && certBio &&
                    PEM_write_bio_PrivateKey(privateBio.get(), key.get(), nullptr, nullptr, 0,
                                             nullptr, nullptr) &&
                    PEM_write_bio_X509(certBio.get(), cert.get()),
                "PEM encoding failed");
        QJsonObject state{
            {"certificate", QString::fromLatin1(bioBytes(certBio.get()))},
            {"privateKey",
             QString::fromLatin1(protectKey(bioBytes(privateBio.get()), false).toBase64())}};
        Security::writePrivate(path, QJsonDocument(state).toJson());
    }
    const auto state = QJsonDocument::fromJson(Security::readFile(path)).object();
    certificate = QSslCertificate(state.value("certificate").toString().toLatin1(), QSsl::Pem);
    QByteArray pem =
        protectKey(QByteArray::fromBase64(state.value("privateKey").toString().toLatin1()), true);
    privateKey = QSslKey(pem, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);
    OPENSSL_cleanse(pem.data(), static_cast<size_t>(pem.size()));
    require(!certificate.isNull() && !privateKey.isNull(), "Stored server identity is invalid");
}
QString Identity::fingerprint() const {
    return Security::hash(certificate.toDer());
}
