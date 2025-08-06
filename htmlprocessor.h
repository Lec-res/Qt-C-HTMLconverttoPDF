#ifndef HTMLPROCESSOR_H
#define HTMLPROCESSOR_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QStringList>
#include <QUrl>

class QNetworkReply;

class HtmlProcessor : public QObject
{
    Q_OBJECT

public:
    explicit HtmlProcessor(QObject *parent = nullptr);

signals:
    void processingFinished(const QString &pdfPath, const QStringList &failedUrls);
    void processingFailed(const QString &reason);
    void progressUpdated(int downloaded, int total);

public slots:
    void process(const QString &htmlContent, const QUrl &baseUrl, bool saveImages, bool generatePdf);

private slots:
    void onDownloadFinished(QNetworkReply *reply);

private:
    void resetState();
    void extractImageUrls(const QString &htmlContent, const QUrl &baseUrl);
    void createPdf();
    QString getFileExtensionFromContentType(const QNetworkReply *reply) const;

    QNetworkAccessManager* m_networkManager;

    // 状态变量
    QList<QUrl> m_imageUrls;
    QStringList m_savedImagePaths;
    QStringList m_failedUrls;
    int m_pendingDownloads;
    int m_totalImages;
    bool m_saveImages;
    bool m_generatePdf;
    QString m_imageSaveDir;
};

#endif // HTMLPROCESSOR_H
