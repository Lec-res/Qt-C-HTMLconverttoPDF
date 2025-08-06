#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class HtmlProcessor;
class QTextEdit;
class QLineEdit;
class QPushButton;
class QCheckBox;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void startProcessing();
    void onProcessingFinished(const QString &pdfPath, const QStringList &failedUrls);
    void onProcessingFailed(const QString &reason);
    void onProgressUpdated(int downloaded, int total);

private:
    void createUI();

    HtmlProcessor *m_processor;

    QTextEdit *m_htmlInput;
    QLineEdit *m_baseUrlInput;
    QPushButton *m_startButton;
    QCheckBox *m_saveImagesCheck;
    QCheckBox *m_generatePdfCheck;
};
#endif // MAINWINDOW_H
