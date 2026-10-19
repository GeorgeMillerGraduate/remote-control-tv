#include "branding.h"
#include "qrcodegen.hpp"
#include "server.h"
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QSslSocket>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <algorithm>
#include <stdexcept>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

class MainWindow final : public QMainWindow {
  public:
    explicit MainWindow(std::unique_ptr<Identity> identity) : identity(std::move(identity)) {
        setWindowTitle("Jenga Remote");
        resize(1080, 760);
        setMinimumSize(940, 700);
        auto *body = new QWidget(this);
        auto *shell = new QHBoxLayout(body);
        shell->setContentsMargins(0, 0, 0, 0);
        shell->setSpacing(0);
        auto *rail = new QFrame;
        rail->setObjectName("rail");
        rail->setFixedWidth(210);
        auto *navigation = new QVBoxLayout(rail);
        navigation->setContentsMargins(22, 30, 22, 22);
        navigation->setSpacing(10);
        auto *logo = new QLabel;
        logo->setPixmap(QPixmap::fromImage(brandIcon(64)));
        navigation->addWidget(logo);
        auto *brand = new QLabel("Jenga Remote");
        brand->setObjectName("brand");
        navigation->addWidget(brand);
        auto *edition = new QLabel("DESKTOP  /  C++");
        edition->setObjectName("eyebrow");
        navigation->addWidget(edition);
        navigation->addSpacing(34);
        auto *pages = new QStackedWidget;
        auto *navGroup = new QButtonGroup(this);
        navGroup->setExclusive(true);
        for (const QString &name :
             {QString("Overview"), QString("Connection settings"), QString("Activity")}) {
            auto *button = new QPushButton(name);
            button->setObjectName("nav");
            button->setCheckable(true);
            button->setCursor(Qt::PointingHandCursor);
            navGroup->addButton(button);
            navigation->addWidget(button);
            const int index = navGroup->buttons().size() - 1;
            connect(button, &QPushButton::clicked, this,
                    [pages, index] { pages->setCurrentIndex(index); });
            if (index == 0)
                button->setChecked(true);
        }
        auto *devices = new QPushButton("Trusted phones");
        devices->setObjectName("nav");
        navigation->addWidget(devices);
        navigation->addStretch();
        auto *railNote =
            new QLabel("PRIVATE BY DESIGN\nTLS encryption\nYou approve every new phone");
        railNote->setObjectName("railNote");
        railNote->setWordWrap(true);
        railNote->setMinimumHeight(80);
        navigation->addWidget(railNote);
        shell->addWidget(rail);
        shell->addWidget(pages, 1);
        setCentralWidget(body);
        auto *overview = new QWidget;
        auto *layout = new QVBoxLayout(overview);
        layout->setContentsMargins(32, 28, 32, 26);
        layout->setSpacing(18);
        pages->addWidget(overview);
        auto *heading = new QHBoxLayout;
        auto *title = new QLabel("Overview");
        title->setObjectName("pageTitle");
        heading->addWidget(title);
        heading->addStretch();
        active = new QLabel("NO PHONE CONNECTED");
        active->setTextFormat(Qt::PlainText);
        active->setObjectName("statusBadge");
        heading->addWidget(active);
        layout->addLayout(heading);
        auto *intro = new QLabel("A simpler connection between your phone and your desktop.");
        intro->setObjectName("muted");
        layout->addWidget(intro);
        auto *hero = new QFrame;
        hero->setObjectName("hero");
        auto *heroLayout = new QHBoxLayout(hero);
        heroLayout->setContentsMargins(26, 25, 20, 25);
        auto *heroText = new QVBoxLayout;
        heroText->setSpacing(12);
        auto *heroTag = new QLabel("YOUR DESKTOP, WITHIN REACH");
        heroTag->setObjectName("accentLabel");
        heroText->addWidget(heroTag);
        auto *heroTitle = new QLabel("Your PC.\nIn your pocket.");
        heroTitle->setObjectName("heroTitle");
        heroText->addWidget(heroTitle);
        auto *heroCopy = new QLabel(
            "Use your phone as a touchpad, keyboard\nand remote desktop. Pair in seconds.");
        heroCopy->setObjectName("muted");
        heroCopy->setWordWrap(true);
        heroText->addWidget(heroCopy);
        qrButton = new QPushButton("Connect phone  /  Show QR");
        qrButton->setObjectName("primary");
        qrButton->setMinimumHeight(46);
        heroText->addWidget(qrButton);
        heroText->addStretch();
        heroLayout->addLayout(heroText, 1);
        heroLayout->addWidget(new ConnectionArt, 1);
        layout->addWidget(hero);
        auto *stats = new QHBoxLayout;
        auto card = [&](const QString &label, QLabel *value) {
            auto *frame = new QFrame;
            frame->setObjectName("card");
            auto *box = new QVBoxLayout(frame);
            box->setContentsMargins(18, 15, 18, 16);
            auto *caption = new QLabel(label);
            caption->setObjectName("eyebrow");
            box->addWidget(caption);
            value->setObjectName("statValue");
            value->setWordWrap(true);
            box->addWidget(value);
            stats->addWidget(frame, 1);
        };
        serverStatus = new QLabel("Starting…");
        endpoint = new QLabel("—");
        trustedCount = new QLabel("0 phones");
        card("SERVER", serverStatus);
        card("CONNECTION ADDRESS", endpoint);
        card("TRUSTED DEVICES", trustedCount);
        layout->addLayout(stats);
        auto *steps = new QFrame;
        steps->setObjectName("card");
        auto *stepLayout = new QVBoxLayout(steps);
        stepLayout->setContentsMargins(20, 18, 20, 18);
        auto *stepsTitle = new QLabel("Three steps. One secure connection.");
        stepsTitle->setObjectName("sectionTitle");
        stepLayout->addWidget(stepsTitle);
        auto *stepRow = new QHBoxLayout;
        for (const QString &step :
             {QString("01   Show the QR code"), QString("02   Scan on your phone"),
              QString("03   Approve on this PC")}) {
            auto *label = new QLabel(step);
            label->setObjectName("muted");
            stepRow->addWidget(label);
        }
        stepLayout->addLayout(stepRow);
        layout->addWidget(steps);
        auto *bottom = new QHBoxLayout;
        auto *access = new QPushButton("Connection settings");
        bottom->addWidget(access);
        bottom->addStretch();
        auto *emergency = new QPushButton("Emergency disconnect");
        emergency->setObjectName("danger");
        bottom->addWidget(emergency);
        layout->addLayout(bottom);
        layout->addStretch();
        connect(access, &QPushButton::clicked, this, [pages, navGroup] {
            pages->setCurrentIndex(1);
            navGroup->buttons().at(1)->setChecked(true);
        });
        auto *network = new QWidget;
        auto *networkLayout = new QVBoxLayout(network);
        networkLayout->setContentsMargins(32, 28, 32, 26);
        networkLayout->setSpacing(20);
        pages->addWidget(network);
        auto *networkTitle = new QLabel("Connection settings");
        networkTitle->setObjectName("pageTitle");
        networkLayout->addWidget(networkTitle);
        auto *networkInfo = new QLabel("Choose a reachable Wi-Fi or private VPN address. "
                                       "Permission changes disconnect the phone.");
        networkInfo->setObjectName("muted");
        networkInfo->setWordWrap(true);
        networkLayout->addWidget(networkInfo);
        auto *networkCard = new QGroupBox("Network");
        auto *networkCardLayout = new QVBoxLayout(networkCard);
        auto *form = new QFormLayout;
        address = new QComboBox;
        address->setEditable(true);
        for (const auto &interface : QNetworkInterface::allInterfaces()) {
            if (!(interface.flags() & QNetworkInterface::IsUp) ||
                (interface.flags() & QNetworkInterface::IsLoopBack))
                continue;
            for (const auto &entry : interface.addressEntries())
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    address->addItem(entry.ip().toString());
                    address->setItemData(address->count() - 1, interface.humanReadableName(),
                                         Qt::ToolTipRole);
                }
        }
        if (address->count() == 0)
            address->addItem("127.0.0.1");
        QSettings settings;
        if (settings.contains("address"))
            address->setCurrentText(settings.value("address").toString());
        portBox = new QSpinBox;
        portBox->setRange(1024, 65535);
        portBox->setValue(settings.value("port", 8765).toInt());
        form->addRow("PC LAN / private VPN address", address);
        form->addRow("Listening port", portBox);
        networkCardLayout->addLayout(form);
        auto *row = new QHBoxLayout;
        startButton = new QPushButton("Start server");
        auto *stopButton = new QPushButton("Stop server");
        row->addWidget(startButton);
        row->addWidget(stopButton);
        networkCardLayout->addLayout(row);
        networkLayout->addWidget(networkCard);
        auto *permissions = new QGroupBox("Phone permissions");
        auto *checks = new QVBoxLayout(permissions);
        checks->setSpacing(16);
        mouse = new QCheckBox("Mouse control");
        keyboard = new QCheckBox("Keyboard & text input");
        media = new QCheckBox("Media controls");
        screen = new QCheckBox("Screen viewing — off by default");
        for (auto *box : {mouse, keyboard, media, screen}) {
            box->setChecked(box != screen);
            checks->addWidget(box);
            connect(box, &QCheckBox::toggled, this, [this] {
                if (server)
                    server->setPermissions({mouse->isChecked(), keyboard->isChecked(),
                                            media->isChecked(), screen->isChecked()});
            });
        }
        networkLayout->addWidget(permissions);
        auto *hint =
            new QLabel("Local use: both devices on the same network.\nAway from home: connect both "
                       "devices through your existing WireGuard or Tailscale private network.");
        hint->setObjectName("muted");
        hint->setWordWrap(true);
        networkLayout->addWidget(hint);
        networkLayout->addStretch();
        auto *activityPage = new QWidget;
        auto *activityLayout = new QVBoxLayout(activityPage);
        activityLayout->setContentsMargins(32, 28, 32, 26);
        auto *activityTitle = new QLabel("Activity");
        activityTitle->setObjectName("pageTitle");
        activityLayout->addWidget(activityTitle);
        auto *privacyNote =
            new QLabel("Connection events only. Typed text and pairing secrets are never logged.");
        privacyNote->setObjectName("muted");
        privacyNote->setWordWrap(true);
        activityLayout->addWidget(privacyNote);
        logs = new QPlainTextEdit;
        logs->setReadOnly(true);
        logs->setMaximumBlockCount(250);
        activityLayout->addWidget(logs, 1);
        pages->addWidget(activityPage);
        connect(address, &QComboBox::currentTextChanged, this, [this] { refreshCards(); });
        connect(startButton, &QPushButton::clicked, this, [this] { startServer(); });
        connect(stopButton, &QPushButton::clicked, this, [this] { stopServer(); });
        connect(qrButton, &QPushButton::clicked, this, [this] { showQr(); });
        connect(emergency, &QPushButton::clicked, this, [this] {
            if (server) {
                server->cancelPairing();
                server->emergency();
                append("Emergency disconnect completed");
            }
        });
        connect(devices, &QPushButton::clicked, this, [this] { manage(); });
        append("Native C++ desktop server. Your existing Jenga Remote Android app is compatible.");
        if (!QApplication::arguments().contains("--capture-ui"))
            QTimer::singleShot(0, this, [this] { startServer(); });
        else
            refreshCards();
    }
    ~MainWindow() override {
        stopServer();
    }

  private:
    std::unique_ptr<Identity> identity;
    std::unique_ptr<NativeInput> input;
    std::unique_ptr<RemoteServer> server;
    QComboBox *address{};
    QSpinBox *portBox{};
    QPushButton *startButton{}, *qrButton{};
    QLabel *active{}, *serverStatus{}, *endpoint{}, *trustedCount{};
    QPlainTextEdit *logs{};
    QCheckBox *mouse{}, *keyboard{}, *media{}, *screen{};
    QPointer<QDialog> qrDialog;
    QPointer<QMessageBox> approval;
    void refreshCards() {
        if (serverStatus)
            serverStatus->setText(server && server->running() ? "Online" : "Stopped");
        if (endpoint)
            endpoint->setText(address->currentText() + ":" + QString::number(portBox->value()));
        if (trustedCount)
            trustedCount->setText(server ? QString::number(server->devices().size()) + " phones"
                                         : QString("—"));
    }
    void append(const QString &text) {
        logs->appendPlainText(QTime::currentTime().toString("HH:mm:ss") + "  " + text);
    }
    void failure(const std::exception &error) {
        QMessageBox::warning(this, "Jenga Remote", QString::fromUtf8(error.what()));
    }
    void startServer() {
        if (server && server->running())
            return;
        try {
            input = std::make_unique<NativeInput>();
            server = std::make_unique<RemoteServer>(*identity, *input);
            connect(server.get(), &RemoteServer::log, this, [this](const QString &message) {
                append(message);
                refreshCards();
            });
            connect(server.get(), &RemoteServer::activeChanged, this,
                    [this](bool connected, const QString &name) {
                        active->setText(connected ? "CONNECTED: " + name : "NO PHONE CONNECTED");
                        active->setStyleSheet(connected
                                                  ? "background:#173c34;color:#6befc3;padding:9px "
                                                    "12px;border-radius:7px;font-size:11px"
                                                  : "background:#1c2938;color:#9aadc0;padding:9px "
                                                    "12px;border-radius:7px;font-size:11px");
                    });
            connect(server.get(), &RemoteServer::pairingRequested, this,
                    [this](const QString &id, const QString &name, const QString &peer) {
                        auto *box =
                            new QMessageBox(QMessageBox::Question, "New device pairing request", "",
                                            QMessageBox::Yes | QMessageBox::No, this);
                        approval = box;
                        box->setAttribute(Qt::WA_DeleteOnClose);
                        box->setTextFormat(Qt::PlainText);
                        box->setText("Phone (self-reported): " + name + "\nAddress: " + peer +
                                     "\n\nApprove only the phone you are pairing.");
                        box->setDefaultButton(QMessageBox::No);
                        connect(box, &QMessageBox::finished, this, [this, id](int result) {
                            if (server)
                                server->approve(id, result == QMessageBox::Yes);
                        });
                        box->open();
                    });
            connect(server.get(), &RemoteServer::pairingEnded, this, [this] {
                if (approval)
                    approval->close();
            });
            connect(server.get(), &RemoteServer::paired, this, [this] {
                if (qrDialog)
                    qrDialog->accept();
            });
            server->setPermissions({mouse->isChecked(), keyboard->isChecked(), media->isChecked(),
                                    screen->isChecked()});
            server->start(static_cast<quint16>(portBox->value()));
            startButton->setEnabled(false);
            portBox->setEnabled(false);
            refreshCards();
            QSettings settings;
            settings.setValue("port", portBox->value());
        } catch (const std::exception &error) {
            server.reset();
            input.reset();
            refreshCards();
            failure(error);
        }
    }
    void stopServer() {
        if (qrDialog)
            qrDialog->close();
        if (approval)
            approval->close();
        if (server) {
            server->stop();
            server.reset();
        }
        input.reset();
        if (startButton)
            startButton->setEnabled(true);
        if (portBox)
            portBox->setEnabled(true);
        refreshCards();
    }
    void showQr() {
        if (!server || !server->running()) {
            QMessageBox::information(this, "Start server", "Click Start server first.");
            return;
        }
        if (qrDialog) {
            qrDialog->raise();
            return;
        }
        auto *dialog = new QDialog(this);
        qrDialog = dialog;
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle("Connect your phone");
        auto *layout = new QVBoxLayout(dialog);
        auto *image = new QLabel;
        image->setAlignment(Qt::AlignCenter);
        layout->addWidget(image);
        auto *instructions = new QLabel("Phone: Jenga Remote → Computers → Scan QR Code\nKeep this "
                                        "window open and approve the request on the PC.");
        instructions->setAlignment(Qt::AlignCenter);
        layout->addWidget(instructions);
        auto *countdown = new QLabel;
        countdown->setAlignment(Qt::AlignCenter);
        layout->addWidget(countdown);
        auto *refresh = new QPushButton("Refresh QR");
        auto *cancel = new QPushButton("Cancel pairing");
        layout->addWidget(refresh);
        layout->addWidget(cancel);
        auto expiry = std::make_shared<qint64>(0);
        auto generate = [this, image, expiry] {
            try {
                const QString host = address->currentText().trimmed();
                QSettings().setValue("address", host);
                const auto payload = server->createPairing(host);
                *expiry = payload.value("expiresAt").toInteger();
                const QByteArray json = QJsonDocument(payload).toJson(QJsonDocument::Compact);
                const auto qr =
                    qrcodegen::QrCode::encodeText(json.constData(), qrcodegen::QrCode::Ecc::MEDIUM);
                const int border = 4, scale = std::max(3, 480 / (qr.getSize() + border * 2));
                QImage bitmap((qr.getSize() + border * 2) * scale,
                              (qr.getSize() + border * 2) * scale, QImage::Format_RGB32);
                bitmap.fill(Qt::white);
                QPainter painter(&bitmap);
                painter.setPen(Qt::NoPen);
                painter.setBrush(Qt::black);
                for (int y = 0; y < qr.getSize(); ++y)
                    for (int x = 0; x < qr.getSize(); ++x)
                        if (qr.getModule(x, y))
                            painter.drawRect((x + border) * scale, (y + border) * scale, scale,
                                             scale);
                painter.end();
                image->setPixmap(QPixmap::fromImage(bitmap));
            } catch (const std::exception &error) {
                failure(error);
            }
        };
        connect(refresh, &QPushButton::clicked, dialog, generate);
        connect(cancel, &QPushButton::clicked, dialog, &QDialog::reject);
        auto *timer = new QTimer(dialog);
        timer->setInterval(1000);
        connect(timer, &QTimer::timeout, dialog, [countdown, expiry] {
            const qint64 remaining =
                std::max<qint64>(0, (*expiry - QDateTime::currentMSecsSinceEpoch()) / 1000);
            countdown->setText(remaining > 0 ? QString("Expires in %1 seconds").arg(remaining)
                                             : "Expired — click Refresh QR");
        });
        connect(dialog, &QDialog::finished, this, [this] {
            if (server)
                server->cancelPairing();
            qrDialog.clear();
        });
        generate();
        timer->start();
        dialog->show();
    }
    void manage() {
        if (!server) {
            QMessageBox::information(this, "Trusted phones", "Start the server first.");
            return;
        }
        auto *dialog = new QDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle("Trusted phones");
        auto *layout = new QVBoxLayout(dialog);
        auto *list = new QListWidget;
        layout->addWidget(list);
        auto reload = [this, list] {
            list->clear();
            const auto devices = server->devices();
            for (auto it = devices.begin(); it != devices.end(); ++it) {
                const auto entry = it.value().toObject();
                auto *item =
                    new QListWidgetItem(entry.value("name").toString() + "\n" + it.key(), list);
                item->setData(Qt::UserRole, it.key());
            }
        };
        reload();
        for (const QString &action :
             {QString("Disconnect"), QString("Revoke trust"), QString("Rename")}) {
            auto *button = new QPushButton(action);
            layout->addWidget(button);
            connect(button, &QPushButton::clicked, dialog, [this, list, reload, action] {
                if (!server || !list->currentItem())
                    return;
                const QString id = list->currentItem()->data(Qt::UserRole).toString();
                try {
                    if (action == "Disconnect")
                        server->disconnectDevice(id);
                    else if (action == "Revoke trust") {
                        if (QMessageBox::question(this, "Revoke trust",
                                                  "This phone will need to pair again.") ==
                            QMessageBox::Yes)
                            server->revoke(id);
                    } else {
                        bool ok = false;
                        QString name = QInputDialog::getText(this, "Phone name", "New name",
                                                             QLineEdit::Normal, {}, &ok);
                        if (ok)
                            server->rename(id, name);
                    }
                    reload();
                } catch (const std::exception &error) {
                    failure(error);
                }
            });
        }
        dialog->resize(480, 340);
        dialog->show();
    }
};
int main(int argc, char **argv) {
#ifdef Q_OS_WIN
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif
    QApplication app(argc, argv);
    app.setOrganizationName("JengaCode");
    app.setApplicationName("JengaRemoteCpp");
    app.setApplicationVersion("0.2.0");
    app.setWindowIcon(QIcon(QPixmap::fromImage(brandIcon(256))));
    QFont font("Segoe UI", 10);
    app.setFont(font);
    if (app.arguments().contains("--export-brand-assets")) {
        QDir().mkpath("assets");
        brandIcon(256).save("assets/app-icon.png");
        brandIcon(1024).save("assets/app-icon-large.png");
        return 0;
    }
    app.setStyleSheet(R"(
QWidget { background:#0e151f; color:#eaf0f8; font-family:'Segoe UI'; font-size:13px; }
QFrame#rail QLabel {background:transparent;}
QFrame#rail {background:#0a1018;border-right:1px solid #243040;}
QLabel#brand {font-size:20px;font-weight:700;} QLabel#eyebrow {font-size:10px;font-weight:700;color:#8094aa;letter-spacing:1px;}
QLabel#railNote {font-size:11px;line-height:1.6;color:#71879d;} QLabel#pageTitle {font-size:26px;font-weight:700;}
QLabel#muted {color:#9badc1;font-size:13px;} QLabel#accentLabel {color:#65e4be;font-size:10px;font-weight:700;}
QLabel#heroTitle {font-size:34px;font-weight:700;} QLabel#sectionTitle {font-size:15px;font-weight:600;}
QLabel#statValue {font-size:17px;font-weight:600;} QLabel#statusBadge {background:#1c2938;color:#9aadc0;border-radius:7px;padding:9px 12px;font-size:10px;font-weight:600;}
QFrame#hero {background:#142430;border:1px solid #2a424c;border-radius:14px;} QFrame#hero QLabel {background:transparent;}
QFrame#card {background:#151f2b;border:1px solid #253445;border-radius:10px;} QFrame#card QLabel {background:transparent;}
QPushButton {background:#233246;border:1px solid #33465c;border-radius:7px;padding:11px 15px;color:#dce7f4;font-weight:600;}
QPushButton:hover {background:#30455b;border-color:#54728d;} QPushButton:disabled {color:#637589;background:#182230;}
QPushButton#primary {background:#56dfb4;color:#0b2524;border:none;} QPushButton#primary:hover {background:#83f3ce;}
QPushButton#danger {background:#301d28;border-color:#693248;color:#f5a5ba;} QPushButton#danger:hover {background:#562534;}
QPushButton#nav {text-align:left;border:0;background:transparent;font-weight:500;padding:13px 12px;color:#889db4;} QPushButton#nav:checked {background:#192c39;color:#78e9c4;} QPushButton#nav:hover {background:#172332;color:#eaf0f8;}
QLineEdit,QComboBox,QSpinBox,QPlainTextEdit,QListWidget {background:#0b131d;border:1px solid #304357;border-radius:6px;padding:9px;color:#eaf0f8;selection-background-color:#237968;}
QGroupBox {background:#151f2b;border:1px solid #2a3b4d;border-radius:10px;margin-top:12px;padding:23px 18px 18px;font-weight:600;}
QGroupBox::title {subcontrol-origin:margin;left:18px;padding:0 6px;} QCheckBox {background:transparent;padding:5px;spacing:12px;font-weight:400;}
QCheckBox::indicator {width:20px;height:20px;border:1px solid #456078;border-radius:5px;background:#0d1722;} QCheckBox::indicator:checked {background:#4acfa7;border-color:#71e8c3;image:none;}
QToolTip {background:#223448;color:#edf4fa;border:1px solid #4c667e;padding:6px;}
)");
    if (app.arguments().contains("--capture-ui"))
        QStandardPaths::setTestModeEnabled(true);
    try {
        if (!QSslSocket::supportsSsl())
            throw std::runtime_error("TLS runtime is missing. Run the deployed package, including "
                                     "its TLS and OpenSSL files.");
        auto identity = std::make_unique<Identity>(
            QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
        MainWindow window(std::move(identity));
        window.show();
        if (app.arguments().contains("--capture-ui")) {
            const int index = app.arguments().indexOf("--capture-ui");
            if (index + 1 < app.arguments().size())
                QTimer::singleShot(1500, &window, [&window, &app, index] {
                    window.grab().save(app.arguments().at(index + 1));
                    app.quit();
                });
        }
        return app.exec();
    } catch (const std::exception &error) {
        QMessageBox::critical(nullptr, "Jenga Remote startup failed",
                              QString::fromUtf8(error.what()));
        return 1;
    }
}
