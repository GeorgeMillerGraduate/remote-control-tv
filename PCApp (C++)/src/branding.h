#pragma once
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QWidget>

inline QImage brandIcon(int size) {
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 256.0, size / 256.0);
    QLinearGradient background(20, 0, 240, 256);
    background.setColorAt(0, QColor("#17354b"));
    background.setColorAt(1, QColor("#091523"));
    p.setPen(QPen(QColor("#31516a"), 3));
    p.setBrush(background);
    p.drawRoundedRect(QRectF(8, 8, 240, 240), 54, 54);
    p.setPen(QPen(QColor("#ecf6ff"), 12, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(40, 61, 146, 104), 13, 13);
    p.drawLine(QPointF(93, 167), QPointF(93, 190));
    p.drawLine(QPointF(64, 193), QPointF(121, 193));
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#091523"));
    p.drawRoundedRect(QRectF(133, 98, 82, 119), 19, 19);
    QLinearGradient phone(145, 100, 208, 219);
    phone.setColorAt(0, QColor("#68f4c3"));
    phone.setColorAt(1, QColor("#24b99b"));
    p.setPen(QPen(QColor("#59e8bb"), 9));
    p.setBrush(QColor("#102938"));
    p.drawRoundedRect(QRectF(143, 105, 62, 103), 13, 13);
    p.setPen(QPen(QColor("#59e8bb"), 5, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(166, 195), QPointF(182, 195));
    p.setPen(QPen(QColor("#59e8bb"), 8, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(71, 109), QPointF(88, 125));
    p.drawLine(QPointF(88, 125), QPointF(119, 91));
    p.setPen(Qt::NoPen);
    p.setBrush(phone);
    p.drawEllipse(QPointF(174, 151), 10, 10);
    return image;
}
class ConnectionArt final : public QWidget {
  public:
    explicit ConnectionArt(QWidget *parent = nullptr) : QWidget(parent) {
        setMinimumSize(240, 220);
        setMaximumWidth(330);
    }

  protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.fillRect(rect(), QColor("#142430"));
        p.setRenderHint(QPainter::Antialiasing);
        const double scale = std::min(width() / 320.0, height() / 245.0);
        p.translate((width() - 320 * scale) / 2, (height() - 245 * scale) / 2);
        p.scale(scale, scale);
        p.setPen(QPen(QColor("#203e4b"), 1));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QRectF(22, 4, 250, 250));
        p.drawEllipse(QRectF(58, 40, 178, 178));
        for (int i = 0; i < 9; ++i) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#365564"));
            p.drawEllipse(QPointF(30 + i * 29, 224), 1.5, 1.5);
        }
        p.setPen(QPen(QColor("#395b72"), 2));
        p.setBrush(QColor("#0c1723"));
        p.drawRoundedRect(QRectF(30, 51, 213, 134), 13, 13);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#172a3c"));
        p.drawRoundedRect(QRectF(40, 61, 193, 111), 6, 6);
        p.setBrush(QColor("#284456"));
        p.drawRoundedRect(QRectF(53, 74, 67, 8), 4, 4);
        p.drawRoundedRect(QRectF(53, 92, 106, 5), 2, 2);
        p.setBrush(QColor("#213c4d"));
        p.drawRoundedRect(QRectF(53, 113, 52, 43), 6, 6);
        p.drawRoundedRect(QRectF(114, 113, 102, 43), 6, 6);
        QPainterPath path;
        path.moveTo(124, 145);
        path.lineTo(142, 132);
        path.lineTo(160, 138);
        path.lineTo(179, 121);
        path.lineTo(196, 126);
        p.setPen(QPen(QColor("#52dfb1"), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(path);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#395b72"));
        p.drawRoundedRect(QRectF(108, 183, 53, 8), 3, 3);
        p.drawRoundedRect(QRectF(119, 187, 30, 14), 3, 3);
        p.drawRoundedRect(QRectF(93, 199, 84, 6), 3, 3);
        p.setPen(QPen(QColor("#58dfb4"), 2, Qt::DashLine));
        p.drawArc(QRectF(152, 22, 125, 125), 25 * 16, 100 * 16);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#0b111b"));
        p.drawRoundedRect(QRectF(204, 91, 91, 137), 17, 17);
        p.setPen(QPen(QColor("#50dcb0"), 3));
        p.setBrush(QColor("#11232d"));
        p.drawRoundedRect(QRectF(200, 86, 88, 133), 15, 15);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#1d3b45"));
        p.drawRoundedRect(QRectF(211, 108, 66, 83), 8, 8);
        p.setBrush(QColor("#50dcb0"));
        p.drawRoundedRect(QRectF(231, 98, 26, 3), 1.5, 1.5);
        p.drawRoundedRect(QRectF(233, 204, 22, 3), 1.5, 1.5);
        p.setPen(QPen(QColor("#50dcb0"), 2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(244, 151), 16, 16);
        p.drawEllipse(QPointF(244, 151), 7, 7);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#50dcb0"));
        p.drawEllipse(QPointF(73, 47), 4, 4);
    }
};
