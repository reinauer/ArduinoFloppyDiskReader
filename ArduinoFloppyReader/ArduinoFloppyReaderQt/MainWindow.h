#pragma once

#include <QDialog>
#include <QMainWindow>

#include <atomic>
#include <functional>
#include <thread>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTabWidget;
class QTextEdit;

namespace ArduinoFloppyReader {
enum class CallbackOperation;
enum class DiskSurface;
}

class OperationDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit OperationDialog(const QString &title, QWidget *parent = nullptr);
    ~OperationDialog() override;

    void start(std::function<void(OperationDialog &)> operation);
    bool aborted() const { return m_aborted.load(); }
    void setRange(int maximum);
    void setProgress(int value);
    void setStatus(const QString &status);
    void setOperation(ArduinoFloppyReader::CallbackOperation operation);
    void setTrack(int track, ArduinoFloppyReader::DiskSurface surface,
                  int good = -1, int bad = -1, int total = -1);
    int askRetry(const QString &title, const QString &message);
    void showError(const QString &title, const QString &message);
    void showInformation(const QString &title, const QString &message);
    void showCompletion(const QString &asset, const QString &message);

protected:
    void reject() override;

private:
    void invoke(const std::function<void()> &function, bool blocking = false);

    QLabel *m_status;
    QLabel *m_operation;
    QLabel *m_cylinder;
    QLabel *m_side;
    QLabel *m_good;
    QLabel *m_bad;
    QProgressBar *m_progress;
    QPushButton *m_abort;
    std::atomic_bool m_aborted{false};
    std::thread m_thread;
    bool m_finished = false;
};

class MainWindow final : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    enum class ImageType { Adf, Ibm, St, Scp, Ipf };

    void buildInterface();
    QWidget *buildReadPage();
    QWidget *buildWritePage();
    void restoreSettings();
    void saveSettings();
    void refreshPorts();
    QString selectedPort() const;
    bool requirePort();
    void updateReadOptions();
    void updateWriteOptions();
    ImageType writeImageType() const;
    void browseReadFile();
    void browseWriteFile();
    void readDisk();
    void writeDisk();
    void runDiagnostics();
    void configureDevice();
    void cleanDrive();
    void showAbout();
    QComboBox *m_port;
    QComboBox *m_density;
    QPushButton *m_refresh;
    QTabWidget *m_tabs;

    QComboBox *m_readFormat;
    QLineEdit *m_readFile;
    QButtonGroup *m_tracks;
    QCheckBox *m_flux;
    QLabel *m_scpNote;

    QLineEdit *m_writeFile;
    QCheckBox *m_verify;
    QCheckBox *m_precomp;
    QCheckBox *m_erase;
    QCheckBox *m_index;
};
