#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QFile>
#include <QSvgRenderer>

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    if (argc != 3) return 2;
    const QString input = QString::fromLocal8Bit(argv[1]);
    const QString output = QString::fromLocal8Bit(argv[2]);
    QFile file(input);
    if (!file.open(QIODevice::ReadOnly)) return 3;
    QByteArray bytes = file.readAll();
    QImage image(256, 256, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    if (bytes.contains("<svg")) {
        bytes.replace("currentColor", "#111111");
        QSvgRenderer svg(bytes);
        if (!svg.isValid()) return 4;
        QSizeF size = svg.defaultSize();
        size.scale(image.size(), Qt::KeepAspectRatio);
        svg.render(&painter, QRectF((256 - size.width()) / 2, (256 - size.height()) / 2,
                                  size.width(), size.height()));
    } else {
        QImageReader reader(input);
        const QImage original = reader.read();
        if (original.isNull()) return 5;
        const QSize size = original.size().scaled(image.size(), Qt::KeepAspectRatio);
        painter.drawImage(QRect(QPoint((256 - size.width()) / 2, (256 - size.height()) / 2), size), original);
    }
    painter.end();
    return image.save(output, "PNG") ? 0 : 6;
}
