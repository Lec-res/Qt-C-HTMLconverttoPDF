#include "htmlprocessor.h"
#include <QRegularExpression>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>
#include <QDir>
#include <QPrinter>
#include <QPageSize>
#include <QPageLayout>
#include <QPainter> // 包含 QPainter
#include <QImage>   // 包含 QImage
#include <QDebug>

HtmlProcessor::HtmlProcessor(QObject *parent)
    : QObject(parent),
    m_networkManager(new QNetworkAccessManager(this)),
    m_imageSaveDir("images")
{
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &HtmlProcessor::onDownloadFinished);
}

void HtmlProcessor::resetState()
{
    m_imageUrls.clear();
    m_savedImagePaths.clear();
    m_failedUrls.clear();
    m_pendingDownloads = 0;
    m_totalImages = 0;
    m_saveImages = false;
    m_generatePdf = false;
}

void HtmlProcessor::process(const QString &htmlContent, const QUrl &baseUrl, bool saveImages, bool generatePdf)
{
    resetState();
    m_saveImages = saveImages;
    m_generatePdf = generatePdf;

    extractImageUrls(htmlContent, baseUrl);

    if (m_imageUrls.isEmpty()) {
        emit processingFailed("未在HTML中找到有效的图片链接。");
        return;
    }

    m_totalImages = m_imageUrls.size();
    m_pendingDownloads = m_totalImages;
    m_savedImagePaths.resize(m_totalImages);

    if (m_saveImages || m_generatePdf) {
        QDir().mkpath(m_imageSaveDir);
    }

    emit progressUpdated(0, m_totalImages);
    qDebug() << "找到" << m_totalImages << "张图片，开始并行下载...";

    for (int i = 0; i < m_totalImages; ++i) {
        QNetworkRequest request(m_imageUrls.at(i));
        request.setAttribute(QNetworkRequest::User, i);
        m_networkManager->get(request);
    }
}

void HtmlProcessor::extractImageUrls(const QString &htmlContent, const QUrl &baseUrl)
{
    QRegularExpression regex("<img[^>]+src\\s*=\\s*['\"]([^'\"]+)['\"]", QRegularExpression::CaseInsensitiveOption);
    auto i = regex.globalMatch(htmlContent);

    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        QString relativePath = match.captured(1);
        QUrl fullUrl = baseUrl.resolved(QUrl(relativePath));
        if (fullUrl.isValid() && !m_imageUrls.contains(fullUrl)) {
            m_imageUrls << fullUrl;
        }
    }
}

QString HtmlProcessor::getFileExtensionFromContentType(const QNetworkReply* reply) const
{
    QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString().toLower();
    if (contentType.startsWith("image/png")) return "png";
    if (contentType.startsWith("image/gif")) return "gif";
    if (contentType.startsWith("image/bmp")) return "bmp";
    if (contentType.startsWith("image/webp")) return "webp";
    return "jpg";
}

void HtmlProcessor::onDownloadFinished(QNetworkReply *reply)
{
    m_pendingDownloads--;

    bool ok;
    int originalIndex = reply->request().attribute(QNetworkRequest::User).toInt(&ok);
    if (!ok) {
        qDebug() << "错误：无法从网络回复中获取原始索引。";
        reply->deleteLater();
        emit progressUpdated(m_totalImages - m_pendingDownloads, m_totalImages);
        return;
    }

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray imageData = reply->readAll();
        if (m_saveImages || m_generatePdf) {
            QString extension = getFileExtensionFromContentType(reply);
            QString fileName = QString("%1/image_%2.%3").arg(m_imageSaveDir).arg(originalIndex).arg(extension);
            QFile file(fileName);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(imageData);
                file.close();
                m_savedImagePaths[originalIndex] = fileName;
            } else {
                m_failedUrls.append(reply->url().toString());
                m_savedImagePaths[originalIndex] = "";
            }
        }
    } else {
        m_failedUrls.append(reply->url().toString());
        m_savedImagePaths[originalIndex] = "";
    }

    reply->deleteLater();
    emit progressUpdated(m_totalImages - m_pendingDownloads, m_totalImages);

    if (m_pendingDownloads == 0) {
        qDebug() << "所有下载任务已完成。";
        if (m_generatePdf) {
            createPdf();
        } else {
            emit processingFinished("", m_failedUrls);
        }
    }
}

void HtmlProcessor::createPdf()
{
    bool anySuccess = false;
    for(const QString& path : m_savedImagePaths) {
        if (!path.isEmpty()) {
            anySuccess = true;
            break;
        }
    }

    if (!anySuccess) {
        emit processingFailed("所有图片都下载失败，无法生成PDF。");
        return;
    }

    QPrinter printer;
    printer.setResolution(150);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageOrientation(QPageLayout::Landscape);
    printer.setOutputFileName("output.pdf");

    QPainter painter;
    if (!painter.begin(&printer)) {
        emit processingFailed("无法启动打印机进行PDF生成。");
        return;
    }

    bool firstPage = true;
    for (const QString &path : m_savedImagePaths) {
        if (path.isEmpty()) {
            continue; // 跳过下载失败的图片
        }

        if (!firstPage) {
            printer.newPage(); // 为第二张及以后的图片创建新页面
        }

        QImage image(path);
        if (image.isNull()) {
            continue; // 跳过无法加载的图片
        }

        // 获取页面的可用绘图区域 (以设备像素为单位)
        QRectF pageRect = printer.pageRect(QPrinter::DevicePixel);

        // 在保持宽高比的同时，将图片缩放到最适合页面的尺寸
        QImage scaledImage = image.scaled(pageRect.size().toSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

        // 计算居中绘制的坐标
        int x = (pageRect.width() - scaledImage.width()) / 2;
        int y = (pageRect.height() - scaledImage.height()) / 2;

        // 绘制图片
        painter.drawImage(x, y, scaledImage);

        firstPage = false;
    }

    painter.end(); // 结束绘制

    qDebug() << "PDF文件已生成：output.pdf";
    emit processingFinished("output.pdf", m_failedUrls);
}
