#include "mainwindow.h"
#include "htmlprocessor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QStatusBar>
#include <QMessageBox>
#include <QThread>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    createUI();

    m_processor = new HtmlProcessor();
    // 将处理器移到子线程中，避免长时间操作冻结UI
    QThread* thread = new QThread(this);
    m_processor->moveToThread(thread);

    // 信号槽连接
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::startProcessing);
    connect(this, &MainWindow::destroyed, thread, &QThread::quit);
    connect(thread, &QThread::finished, m_processor, &QObject::deleteLater);

    // 连接处理器的信号到主窗口的槽
    connect(m_processor, &HtmlProcessor::processingFinished, this, &MainWindow::onProcessingFinished);
    connect(m_processor, &HtmlProcessor::processingFailed, this, &MainWindow::onProcessingFailed);
    connect(m_processor, &HtmlProcessor::progressUpdated, this, &MainWindow::onProgressUpdated);

    thread->start();

    setWindowTitle("HTML 图片下载器");
}

MainWindow::~MainWindow()
{
}

void MainWindow::createUI()
{
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    m_baseUrlInput = new QLineEdit("https://www.example.com");
    m_htmlInput = new QTextEdit;
    m_htmlInput->setPlaceholderText("在这里粘贴HTML代码...");

    m_saveImagesCheck = new QCheckBox("保存图片到本地");
    m_saveImagesCheck->setChecked(true);
    m_generatePdfCheck = new QCheckBox("生成PDF文件");
    m_generatePdfCheck->setChecked(true);
    m_startButton = new QPushButton("开始处理");

    QHBoxLayout *optionsLayout = new QHBoxLayout();
    optionsLayout->addWidget(m_saveImagesCheck);
    optionsLayout->addWidget(m_generatePdfCheck);
    optionsLayout->addStretch();

    mainLayout->addWidget(new QLabel("页面基础URL:"));
    mainLayout->addWidget(m_baseUrlInput);
    mainLayout->addWidget(new QLabel("HTML内容:"));
    mainLayout->addWidget(m_htmlInput, 1);
    mainLayout->addLayout(optionsLayout);
    mainLayout->addWidget(m_startButton);

    setCentralWidget(centralWidget);
    statusBar()->showMessage("准备就绪");
}


void MainWindow::startProcessing()
{
    QString html = m_htmlInput->toPlainText();
    QUrl baseUrl(m_baseUrlInput->text());

    if (html.isEmpty()) {
        QMessageBox::warning(this, "警告", "HTML内容不能为空。");
        return;
    }
    if (!baseUrl.isValid() || baseUrl.host().isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入一个有效的基础URL。");
        return;
    }

    m_startButton->setEnabled(false);
    statusBar()->showMessage("正在处理中...");

    // 调用processor的槽函数，Qt会自动处理跨线程的调用
    QMetaObject::invokeMethod(m_processor, "process", Qt::QueuedConnection,
                              Q_ARG(QString, html),
                              Q_ARG(QUrl, baseUrl),
                              Q_ARG(bool, m_saveImagesCheck->isChecked()),
                              Q_ARG(bool, m_generatePdfCheck->isChecked()));
}

void MainWindow::onProcessingFinished(const QString &pdfPath, const QStringList &failedUrls)
{
    m_startButton->setEnabled(true);

    QString message = "处理完成！\n";
    if (!pdfPath.isEmpty()) {
        message += QString("PDF已保存至：%1\n").arg(pdfPath);
    }
    if(m_saveImagesCheck->isChecked()){
        message += "图片已保存至 'images' 文件夹。\n";
    }

    if (failedUrls.isEmpty()) {
        message += "\n所有图片都已成功下载。";
    } else {
        message += QString("\n有 %1 张图片下载失败。").arg(failedUrls.size());
    }

    statusBar()->showMessage("处理完成！");
    QMessageBox::information(this, "成功", message);
}

void MainWindow::onProcessingFailed(const QString &reason)
{
    m_startButton->setEnabled(true);
    statusBar()->showMessage("处理失败");
    QMessageBox::critical(this, "错误", reason);
}

void MainWindow::onProgressUpdated(int downloaded, int total)
{
    if (total > 0) {
        statusBar()->showMessage(QString("正在下载... %1 / %2").arg(downloaded).arg(total));
    }
}
