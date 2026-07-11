#include "MainWindow.h"

#include "ADFWriter.h"
#include "ArduinoInterface.h"

#include <QButtonGroup>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenuBar>
#include <QMetaObject>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QTabWidget>
#include <QTextEdit>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <memory>
#include <vector>

using namespace ArduinoFloppyReader;

namespace {
QString qstr(const std::string &value)
{
    return QString::fromUtf8(value.c_str());
}

std::wstring wstr(const QString &value)
{
    return value.toStdWString();
}

QString resultMessage(ADFResult result, const std::string &detail)
{
    switch (result) {
    case ADFResult::adfrFileError:
        return "Unable to open the selected image file.";
    case ADFResult::adfrFileIOError:
        return "An error occurred while accessing the image file.";
    case ADFResult::adfrDiskWriteProtected:
        return "The disk is write protected.";
    case ADFResult::adfrFirmwareTooOld:
        return "This operation requires newer DrawBridge firmware.";
    case ADFResult::adfrMediaSizeMismatch:
        return "The image and disk density do not match.";
    case ADFResult::adfrExtendedADFNotSupported:
        return "Extended ADF files are not supported.";
    case ADFResult::adfrBadSCPFile:
        return "The SCP file is unsupported or incompatible.";
    case ADFResult::adfrIPFLibraryNotAvailable:
        return "The CAPSImg library is unavailable or could not read the IPF file.";
    case ADFResult::adfrDriveError:
        return detail.empty() ? "A drive communication error occurred."
                              : "A drive communication error occurred:\n\n" + qstr(detail);
    default:
        return "The operation could not be completed.";
    }
}

QString operationText(CallbackOperation operation)
{
    switch (operation) {
    case CallbackOperation::coStarting: return "Preparing interface...";
    case CallbackOperation::coReading: return "Reading cylinder...";
    case CallbackOperation::coWriting: return "Writing cylinder...";
    case CallbackOperation::coVerifying: return "Verifying cylinder...";
    case CallbackOperation::coRetryReading: return "Re-reading cylinder...";
    case CallbackOperation::coRetryWriting: return "Re-writing cylinder...";
    case CallbackOperation::coReVerifying: return "Re-verifying cylinder...";
    case CallbackOperation::coReadingFile: return "Reading file...";
    }
    return {};
}
}

OperationDialog::OperationDialog(const QString &title, QWidget *parent)
    : QDialog(parent), m_status(new QLabel), m_operation(new QLabel),
      m_cylinder(new QLabel("0")), m_side(new QLabel), m_good(new QLabel("0")),
      m_bad(new QLabel("0")), m_progress(new QProgressBar),
      m_abort(new QPushButton("&Abort"))
{
    setWindowTitle(title);
    setModal(true);
    setMinimumWidth(520);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    auto *top = new QHBoxLayout;
    auto *messages = new QVBoxLayout;
    messages->addWidget(m_status);
    messages->addWidget(m_operation);
    top->addLayout(messages, 1);
    top->addWidget(m_abort);

    auto *fields = new QHBoxLayout;
    fields->addWidget(new QLabel("Cylinder:"));
    fields->addWidget(m_cylinder);
    fields->addSpacing(12);
    fields->addWidget(new QLabel("Current Side:"));
    fields->addWidget(m_side);
    fields->addStretch();
    fields->addWidget(new QLabel("Good Sectors:"));
    fields->addWidget(m_good);
    fields->addSpacing(12);
    fields->addWidget(new QLabel("Partial Sectors:"));
    fields->addWidget(m_bad);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addLayout(fields);
    layout->addWidget(m_progress);
    connect(m_abort, &QPushButton::clicked, this, &OperationDialog::reject);
}

OperationDialog::~OperationDialog()
{
    m_aborted = true;
    if (m_thread.joinable()) m_thread.join();
}

void OperationDialog::invoke(const std::function<void()> &function, bool blocking)
{
    if (QThread::currentThread() == thread()) {
        function();
        return;
    }
    QMetaObject::invokeMethod(this, function,
        blocking ? Qt::BlockingQueuedConnection : Qt::QueuedConnection);
}

void OperationDialog::start(std::function<void(OperationDialog &)> operation)
{
    m_thread = std::thread([this, operation = std::move(operation)] {
        operation(*this);
        invoke([this] {
            m_finished = true;
            m_abort->setText("Close");
            accept();
        });
    });
}

void OperationDialog::setRange(int maximum)
{
    invoke([this, maximum] { m_progress->setRange(0, maximum); });
}

void OperationDialog::setProgress(int value)
{
    invoke([this, value] { m_progress->setValue(value); });
}

void OperationDialog::setStatus(const QString &status)
{
    invoke([this, status] { m_status->setText(status); });
}

void OperationDialog::setOperation(CallbackOperation operation)
{
    const QString text = operationText(operation);
    invoke([this, text] { m_operation->setText(text); });
}

void OperationDialog::setTrack(int track, DiskSurface surface, int good,
                               int bad, int total)
{
    invoke([this, track, surface, good, bad, total] {
        m_cylinder->setText(QString::number(track));
        m_side->setText(surface == DiskSurface::dsLower ? "Lower" : "Upper");
        if (good >= 0)
            m_good->setText(total >= 0 ? QString("%1/%2").arg(good).arg(total)
                                       : QString::number(good));
        if (bad >= 0) m_bad->setText(QString::number(bad));
    });
}

int OperationDialog::askRetry(const QString &title, const QString &message)
{
    int answer = QMessageBox::Abort;
    invoke([&] {
        QMessageBox box(QMessageBox::Warning, title, message,
                        QMessageBox::Abort | QMessageBox::Retry | QMessageBox::Ignore,
                        this);
        answer = box.exec();
    }, true);
    return answer;
}

void OperationDialog::showError(const QString &title, const QString &message)
{
    invoke([this, title, message] { QMessageBox::critical(this, title, message); }, true);
}

void OperationDialog::showInformation(const QString &title,
                                      const QString &message)
{
    invoke([this, title, message] { QMessageBox::information(this, title, message); }, true);
}

void OperationDialog::showCompletion(const QString &asset, const QString &message)
{
    invoke([this, asset, message] {
        QMessageBox box(this);
        box.setWindowTitle("DrawBridge");
        const QPixmap image(asset);
        if (!image.isNull()) box.setIconPixmap(image);
        box.setText(message);
        box.exec();
    }, true);
}

void OperationDialog::reject()
{
    if (!m_finished) {
        m_aborted = true;
        m_abort->setEnabled(false);
        m_abort->setText("Aborting...");
        return;
    }
    QDialog::reject();
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    buildInterface();
    restoreSettings();
    refreshPorts();

    auto *portTimer = new QTimer(this);
    portTimer->setInterval(2000);
    connect(portTimer, &QTimer::timeout, this, &MainWindow::refreshPorts);
    portTimer->start();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildInterface()
{
    setWindowTitle("DrawBridge aka Arduino Amiga Floppy Disk Reader and Writer");
    setMinimumSize(690, 420);
    resize(720, 440);

    auto *central = new QWidget;
    auto *root = new QVBoxLayout(central);
    auto *header = new QHBoxLayout;
    auto *connection = new QGroupBox("DrawBridge (Arduino) Connection");
    auto *grid = new QGridLayout(connection);
    m_port = new QComboBox;
    m_refresh = new QPushButton("Refresh");
    m_density = new QComboBox;
    m_density->addItems({"Automatic", "DD", "HD"});
    auto *diagnostics = new QPushButton("&Diagnostics...");
    auto *configure = new QPushButton("DrawBridge &Config...");
    auto *clean = new QPushButton("&Clean Drive...");
    configure->setMinimumWidth(150);
    clean->setMinimumWidth(120);
    diagnostics->setMinimumWidth(120);
    grid->addWidget(new QLabel("Port:"), 0, 0);
    grid->addWidget(m_port, 0, 1, 1, 3);
    grid->addWidget(m_refresh, 0, 4);
    grid->addWidget(new QLabel("Disk Type:"), 1, 0);
    grid->addWidget(m_density, 1, 1);
    grid->addWidget(configure, 1, 2);
    grid->addWidget(clean, 1, 3);
    grid->addWidget(diagnostics, 1, 4);
    header->addWidget(connection, 1);
    auto *logo = new QLabel;
    logo->setPixmap(QPixmap(":/drawbridge/floppy.bmp"));
    logo->setAlignment(Qt::AlignCenter);
    header->addWidget(logo);
    root->addLayout(header);

    m_tabs = new QTabWidget;
    m_tabs->addTab(buildReadPage(), "Read Disk");
    m_tabs->addTab(buildWritePage(), "Write Disk");
    root->addWidget(m_tabs, 1);

    auto *footer = new QLabel(
        "<a href=\"https://amiga.robsmithdev.co.uk\">DrawBridge by Robert Smith — "
        "amiga.robsmithdev.co.uk</a>");
    footer->setOpenExternalLinks(true);
    footer->setAlignment(Qt::AlignCenter);
    root->addWidget(footer);
    setCentralWidget(central);

    auto *about = menuBar()->addMenu("&Help")->addAction("&About DrawBridge");
    connect(about, &QAction::triggered, this, &MainWindow::showAbout);
    connect(m_refresh, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    connect(diagnostics, &QPushButton::clicked, this, &MainWindow::runDiagnostics);
    connect(configure, &QPushButton::clicked, this, &MainWindow::configureDevice);
    connect(clean, &QPushButton::clicked, this, &MainWindow::cleanDrive);
}

QWidget *MainWindow::buildReadPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *top = new QGridLayout;
    m_readFormat = new QComboBox;
    m_readFormat->addItems({"ADF (Amiga)", "IMA (IBM PC)", "ST (Atari ST)",
                            "SCP (Any/Flux)"});
    m_readFile = new QLineEdit;
    auto *browse = new QPushButton("...");
    auto *start = new QPushButton("Start...");
    top->addWidget(new QLabel("Save to:"), 0, 0);
    top->addWidget(m_readFormat, 0, 1);
    top->addWidget(new QLabel("Tracks:"), 0, 2);
    m_tracks = new QButtonGroup(this);
    auto *trackRow = new QHBoxLayout;
    for (const auto &[id, text] : std::vector<std::pair<int, QString>>{
             {80, "0–79"}, {82, "0–81"}, {84, "0–83"}}) {
        auto *radio = new QRadioButton(text);
        m_tracks->addButton(radio, id);
        trackRow->addWidget(radio);
        if (id == 80) radio->setChecked(true);
    }
    top->addLayout(trackRow, 0, 3, 1, 2);
    top->addWidget(new QLabel("File name:"), 1, 0);
    top->addWidget(m_readFile, 1, 1, 1, 3);
    top->addWidget(browse, 1, 4);
    layout->addLayout(top);
    m_scpNote = new QLabel(
        "DD SCP files have limited flux resolution. HD SCP files contain MFM "
        "levels without preservation-quality timing data. This is a hardware "
        "limitation, not an SCP format limitation.");
    m_scpNote->setWordWrap(true);
    m_flux = new QCheckBox("Experimental Accurate Flux Read (not recommended)");
    layout->addWidget(m_scpNote);
    layout->addWidget(m_flux);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(start);
    layout->addLayout(buttons);
    connect(browse, &QPushButton::clicked, this, &MainWindow::browseReadFile);
    connect(start, &QPushButton::clicked, this, &MainWindow::readDisk);
    connect(m_readFormat, &QComboBox::currentIndexChanged,
            this, &MainWindow::updateReadOptions);
    return page;
}

QWidget *MainWindow::buildWritePage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *file = new QHBoxLayout;
    m_writeFile = new QLineEdit;
    auto *browse = new QPushButton("...");
    file->addWidget(new QLabel("Disk Image File:"));
    file->addWidget(m_writeFile, 1);
    file->addWidget(browse);
    layout->addLayout(file);
    m_verify = new QCheckBox("Verify Write (ADF/IMA/ST only)");
    m_precomp = new QCheckBox(
        "Use Write Precompensation (ADF/IMA/ST, DD only)");
    m_erase = new QCheckBox(
        "Perform several track erases before write (helps with some old disks)");
    m_index = new QCheckBox(
        "Align tracks to index (rather than random like the Amiga) — slower");
    layout->addWidget(m_verify);
    layout->addWidget(m_precomp);
    layout->addWidget(m_erase);
    layout->addWidget(m_index);
    auto *buttons = new QHBoxLayout;
    auto *start = new QPushButton("Start...");
    buttons->addStretch();
    buttons->addWidget(start);
    layout->addLayout(buttons);
    connect(browse, &QPushButton::clicked, this, &MainWindow::browseWriteFile);
    connect(start, &QPushButton::clicked, this, &MainWindow::writeDisk);
    connect(m_writeFile, &QLineEdit::textChanged,
            this, &MainWindow::updateWriteOptions);
    return page;
}

void MainWindow::restoreSettings()
{
    QSettings settings;
    m_density->setCurrentIndex(settings.value("density", 0).toInt());
    m_tabs->setCurrentIndex(settings.value("mode", 0).toInt());
    m_readFormat->setCurrentIndex(settings.value("read/format", 0).toInt());
    if (auto *button = m_tracks->button(settings.value("read/tracks", 80).toInt()))
        button->setChecked(true);
    m_flux->setChecked(settings.value("read/accurateFlux", false).toBool());
    m_verify->setChecked(settings.value("write/verify", true).toBool());
    m_precomp->setChecked(settings.value("write/precomp", true).toBool());
    m_erase->setChecked(settings.value("write/erase", false).toBool());
    m_index->setChecked(settings.value("write/index", true).toBool());
    updateReadOptions();
    updateWriteOptions();
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue("port", selectedPort());
    settings.setValue("density", m_density->currentIndex());
    settings.setValue("mode", m_tabs->currentIndex());
    settings.setValue("read/format", m_readFormat->currentIndex());
    settings.setValue("read/tracks", m_tracks->checkedId());
    settings.setValue("read/accurateFlux", m_flux->isChecked());
    settings.setValue("write/verify", m_verify->isChecked());
    settings.setValue("write/precomp", m_precomp->isChecked());
    settings.setValue("write/erase", m_erase->isChecked());
    settings.setValue("write/index", m_index->isChecked());
}

void MainWindow::refreshPorts()
{
    const QString previous = selectedPort().isEmpty()
        ? QSettings().value("port").toString() : selectedPort();
    std::vector<std::wstring> ports;
    ArduinoInterface::enumeratePorts(ports);
    QStringList names;
    for (const auto &port : ports) names << QString::fromStdWString(port);
    names.removeDuplicates();
    names.sort(Qt::CaseInsensitive);
    QStringList existing;
    for (int i = 0; i < m_port->count(); ++i) existing << m_port->itemText(i);
    if (names == existing) return;
    m_port->blockSignals(true);
    m_port->clear();
    m_port->addItems(names);
    const int selected = names.indexOf(previous);
    if (selected >= 0) m_port->setCurrentIndex(selected);
    m_port->blockSignals(false);
}

QString MainWindow::selectedPort() const
{
    return m_port->currentText();
}

bool MainWindow::requirePort()
{
    if (!selectedPort().isEmpty()) return true;
    QMessageBox::warning(this, "DrawBridge", "No compatible serial port was found.");
    return false;
}

void MainWindow::updateReadOptions()
{
    const bool scp = m_readFormat->currentIndex() == 3;
    m_scpNote->setVisible(scp);
    m_flux->setVisible(scp);
    const QStringList extensions{"adf", "ima", "st", "scp"};
    const QString current = m_readFile->text();
    if (!current.isEmpty() && current.contains('.')) {
        QString replacement = current.left(current.lastIndexOf('.') + 1);
        replacement += extensions.at(m_readFormat->currentIndex());
        m_readFile->setText(replacement);
    }
}

MainWindow::ImageType MainWindow::writeImageType() const
{
    const QString suffix = m_writeFile->text().section('.', -1).toLower();
    if (suffix == "scp") return ImageType::Scp;
    if (suffix == "ipf") return ImageType::Ipf;
    if (suffix == "st") return ImageType::St;
    if (suffix == "ima" || suffix == "img") return ImageType::Ibm;
    return ImageType::Adf;
}

void MainWindow::updateWriteOptions()
{
    const ImageType type = writeImageType();
    const bool flux = type == ImageType::Scp || type == ImageType::Ipf;
    m_verify->setEnabled(!flux);
    m_precomp->setEnabled(!flux);
    m_index->setEnabled(type == ImageType::Adf);
}

void MainWindow::browseReadFile()
{
    const QStringList filters{
        "Amiga Disk Files (*.adf)", "IBM PC Disk Files (*.ima)",
        "Atari ST Disk Files (*.st)", "SuperCard Pro Files (*.scp)"};
    const QString file = QFileDialog::getSaveFileName(
        this, "Save Disk Image", m_readFile->text(),
        filters.at(m_readFormat->currentIndex()));
    if (!file.isEmpty()) m_readFile->setText(file);
}

void MainWindow::browseWriteFile()
{
    const QString file = QFileDialog::getOpenFileName(
        this, "Select a Disk Image File", m_writeFile->text(),
        "Supported Images (*.adf *.scp *.ipf *.ima *.img *.st);;"
        "Amiga Disk Files (*.adf *.ipf);;IBM PC Disk Files (*.ima *.img);;"
        "Atari ST Disk Files (*.st);;SuperCard Pro Files (*.scp);;All Files (*)");
    if (!file.isEmpty()) m_writeFile->setText(file);
}

void MainWindow::readDisk()
{
    if (!requirePort()) return;
    if (m_readFile->text().isEmpty()) {
        QMessageBox::warning(this, "Read Disk", "Select an output file first.");
        return;
    }
    saveSettings();
    const std::wstring port = wstr(selectedPort());
    const std::wstring file = wstr(m_readFile->text());
    const int format = m_readFormat->currentIndex();
    const int tracks = m_tracks->checkedId();
    const int density = m_density->currentIndex();
    const bool accurateFlux = m_flux->isChecked();
    OperationDialog dialog("Reading Disk", this);
    dialog.start([=](OperationDialog &ui) {
        ADFWriter writer;
        ui.setStatus("Opening serial port and setting up device...");
        if (!writer.openDevice(port)) {
            ui.showError("Serial Port Error", qstr(writer.getLastError()));
            return;
        }
        bool hd = density == 2;
        const FirmwareVersion version = writer.getFirwareVersion();
        if (density == 0 && ((version.major > 1) || version.minor >= 9)) {
            ui.setStatus("Checking disk density...");
            if (writer.GuessDiskDensity(hd) != ADFResult::adfrComplete) {
                ui.showError("Disk Density", "Unable to determine the disk density.");
                writer.closeDevice();
                return;
            }
        }
        ui.setRange(tracks * 2);
        auto callback = [&ui](int track, DiskSurface side, int retries,
                              int good, int bad, int total,
                              CallbackOperation operation) {
            if (ui.aborted()) return WriteResponse::wrAbort;
            ui.setOperation(operation);
            ui.setTrack(track, side, good, bad, total);
            ui.setProgress(track * 2 + (side == DiskSurface::dsUpper ? 1 : 0));
            if (retries > 20) {
                const int answer = ui.askRetry("Disk Read Errors",
                    "The disk has checksum errors or missing data.");
                if (answer == QMessageBox::Abort) return WriteResponse::wrAbort;
                if (answer == QMessageBox::Retry) return WriteResponse::wrRetry;
                return WriteResponse::wrSkipBadChecksums;
            }
            return WriteResponse::wrContinue;
        };
        ADFResult result = ADFResult::adfrFileError;
        if (format == 0) result = writer.DiskToADF(file, hd, tracks, callback);
        else if (format == 3)
            result = writer.DiskToSCP(file, hd, tracks, 3, callback, accurateFlux);
        else result = writer.diskToIBMST(file, hd, callback);
        const std::string detail = writer.getLastError();
        writer.closeDevice();
        if (result == ADFResult::adfrComplete)
            ui.showCompletion(format == 3 ? ":/drawbridge/scp.bmp"
                                          : ":/drawbridge/fin.bmp",
                              "The disk image was read successfully.");
        else if (result == ADFResult::adfrCompletedWithErrors)
            ui.showCompletion(":/drawbridge/finpartial.bmp",
                              "The image was saved, but some sectors contain errors.");
        else if (result != ADFResult::adfrAborted)
            ui.showError("Read Failed", resultMessage(result, detail));
    });
    dialog.exec();
}

void MainWindow::writeDisk()
{
    if (!requirePort()) return;
    if (m_writeFile->text().isEmpty()) {
        QMessageBox::warning(this, "Write Disk", "Select an image file first.");
        return;
    }
    saveSettings();
    const std::wstring port = wstr(selectedPort());
    const std::wstring file = wstr(m_writeFile->text());
    const ImageType type = writeImageType();
    const int density = m_density->currentIndex();
    const bool verify = m_verify->isChecked();
    const bool precomp = m_precomp->isChecked();
    const bool erase = m_erase->isChecked();
    const bool index = m_index->isChecked();
    OperationDialog dialog("Writing Disk", this);
    dialog.start([=](OperationDialog &ui) {
        ADFWriter writer;
        ui.setStatus("Opening serial port and setting up device...");
        if (!writer.openDevice(port)) {
            ui.showError("Serial Port Error", qstr(writer.getLastError()));
            return;
        }
        bool hd = density == 2;
        const bool flux = type == ImageType::Scp || type == ImageType::Ipf;
        if (flux && density == 2) {
            ui.showError("Disk Density", "SCP and IPF writing supports DD media only.");
            writer.closeDevice();
            return;
        }
        const FirmwareVersion version = writer.getFirwareVersion();
        if (!flux && density == 0 && ((version.major > 1) || version.minor >= 9)) {
            ui.setStatus("Checking disk density...");
            if (writer.GuessDiskDensity(hd) != ADFResult::adfrComplete) {
                ui.showError("Disk Density", "Unable to determine the disk density.");
                writer.closeDevice();
                return;
            }
        }
        ui.setRange(160);
        auto callback = [&ui](int track, DiskSurface side, bool verifyError,
                              CallbackOperation operation) {
            if (ui.aborted()) return WriteResponse::wrAbort;
            ui.setOperation(operation);
            ui.setTrack(track, side);
            ui.setProgress(track * 2 + (side == DiskSurface::dsUpper ? 1 : 0));
            if (verifyError) {
                const int answer = ui.askRetry("Disk Write Error",
                                               "Verify error writing track.");
                if (answer == QMessageBox::Abort) return WriteResponse::wrAbort;
                if (answer == QMessageBox::Retry) return WriteResponse::wrRetry;
                return WriteResponse::wrSkipBadChecksums;
            }
            return WriteResponse::wrContinue;
        };
        ADFResult result;
        switch (type) {
        case ImageType::Ipf: result = writer.IPFToDisk(file, erase, callback); break;
        case ImageType::Scp: result = writer.SCPToDisk(file, erase, callback); break;
        case ImageType::Adf:
            result = writer.ADFToDisk(file, hd, verify, precomp, erase, index, callback);
            break;
        case ImageType::Ibm:
        case ImageType::St:
            result = writer.sectorFileToDisk(file, hd, verify, precomp, erase,
                                             type == ImageType::St, callback);
            break;
        }
        const std::string detail = writer.getLastError();
        writer.closeDevice();
        if (result == ADFResult::adfrComplete)
            ui.showCompletion(verify ? ":/drawbridge/fin2.bmp"
                                     : ":/drawbridge/dialog_o.bmp",
                              verify ? "The disk was written and verified successfully."
                                     : "The disk was written successfully without verification.");
        else if (result == ADFResult::adfrCompletedWithErrors)
            ui.showCompletion(":/drawbridge/finpartial2.bmp",
                              "Writing completed with verification errors.");
        else if (result != ADFResult::adfrAborted)
            ui.showError("Write Failed", resultMessage(result, detail));
    });
    dialog.exec();
}

void MainWindow::runDiagnostics()
{
    if (!requirePort()) return;
    QDialog dialog(this);
    dialog.setWindowTitle("Drive and Arduino Diagnostics");
    dialog.setWindowFlag(Qt::WindowCloseButtonHint, false);
    dialog.resize(700, 430);
    auto *output = new QTextEdit;
    output->setReadOnly(true);
    auto *close = new QPushButton("Close");
    close->setEnabled(false);
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(output);
    layout->addWidget(close, 0, Qt::AlignRight);
    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    const std::wstring port = wstr(selectedPort());
    std::thread worker([&] {
        ADFWriter writer;
        writer.runDiagnostics(port,
            [&](bool error, const std::string message) {
                const QString line = (error ? "DIAGNOSTICS FAILED: " : "") + qstr(message);
                QMetaObject::invokeMethod(output, [=] { output->append(line); });
            },
            [&](bool question, const std::string message) {
                bool answer = false;
                QMetaObject::invokeMethod(&dialog, [&] {
                    answer = question
                        ? QMessageBox::question(&dialog, "Diagnostics Question", qstr(message))
                              == QMessageBox::Yes
                        : QMessageBox::information(&dialog, "Diagnostics Prompt", qstr(message),
                              QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Ok;
                }, Qt::BlockingQueuedConnection);
                return answer;
            });
        writer.closeDevice();
        QMetaObject::invokeMethod(close, [=] { close->setEnabled(true); });
    });
    dialog.exec();
    if (worker.joinable()) worker.join();
}

void MainWindow::configureDevice()
{
    if (!requirePort()) return;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    ArduinoInterface interface;
    const DiagnosticResponse response = interface.openPort(wstr(selectedPort()));
    QApplication::restoreOverrideCursor();
    if (response != DiagnosticResponse::drOK) {
        QMessageBox::critical(this, "DrawBridge Config", interface.getLastErrorStr().c_str());
        return;
    }
    const FirmwareVersion version = interface.getFirwareVersion();
    if (version.major == 1 && version.minor < 9) {
        QMessageBox::warning(this, "DrawBridge Config",
                             "This feature requires firmware version 1.9 or newer.");
        return;
    }
    bool advanced, plus, dd, slow, aligned;
    bool error = interface.eeprom_IsAdvancedController(advanced) != DiagnosticResponse::drOK;
    error |= interface.eeprom_IsDrawbridgePlusMode(plus) != DiagnosticResponse::drOK;
    error |= interface.eeprom_IsDensityDetectDisabled(dd) != DiagnosticResponse::drOK;
    error |= interface.eeprom_IsSlowSeekMode(slow) != DiagnosticResponse::drOK;
    error |= interface.eeprom_IsIndexAlignMode(aligned) != DiagnosticResponse::drOK;
    if (error) {
        QMessageBox::critical(this, "DrawBridge Config", "Unable to read EEPROM settings.");
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle("DrawBridge EEPROM Config");
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel("The following settings are saved to your DrawBridge board:"));
    auto add = [&](const QString &text, bool checked) {
        auto *box = new QCheckBox(text);
        box->setChecked(checked);
        layout->addWidget(box);
        return box;
    };
    auto *advancedBox = add("Force DiskChange detection support", advanced);
    auto *plusBox = add("Set DrawBridge Plus mode", plus);
    auto *ddBox = add("Force all disks to be detected as double density", dd);
    auto *slowBox = add("Use slower disk seeking", slow);
    auto *alignedBox = add("Always index-align all track writes", aligned);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        error = interface.eeprom_SetAdvancedController(advancedBox->isChecked()) != DiagnosticResponse::drOK;
        error |= interface.eeprom_SetDrawbridgePlusMode(plusBox->isChecked()) != DiagnosticResponse::drOK;
        error |= interface.eeprom_SetDensityDetectDisabled(ddBox->isChecked()) != DiagnosticResponse::drOK;
        error |= interface.eeprom_SetSlowSeekMode(slowBox->isChecked()) != DiagnosticResponse::drOK;
        error |= interface.eeprom_SetIndexAlignMode(alignedBox->isChecked()) != DiagnosticResponse::drOK;
        QApplication::restoreOverrideCursor();
        if (error) QMessageBox::critical(this, "DrawBridge Config", "Unable to save EEPROM settings.");
    }
    interface.closePort();
}

void MainWindow::cleanDrive()
{
    if (!requirePort()) return;
    const auto answer = QMessageBox::question(this, "Floppy Drive Cleaner",
        "Insert a cleaning disk, add cleaning solution, and select Yes to start.\n\n"
        "Do not use an ordinary floppy disk for this operation.");
    if (answer != QMessageBox::Yes) return;
    const std::wstring port = wstr(selectedPort());
    OperationDialog dialog("Floppy Drive Cleaner", this);
    dialog.start([=](OperationDialog &ui) {
        ADFWriter writer;
        if (!writer.openDevice(port)) {
            ui.showError("Drive Cleaner", qstr(writer.getLastError()));
            return;
        }
        const ADFResult result = writer.runClean([&](uint16_t position, uint16_t maximum) {
            if (position == 0) ui.setRange(maximum);
            ui.setProgress(position);
            return !ui.aborted();
        });
        writer.closeDevice();
        if (result == ADFResult::adfrComplete)
            ui.showInformation("Drive Cleaner", "Cleaning cycle completed.");
        else if (result == ADFResult::adfrCompletedWithErrors)
            ui.showError("Drive Cleaner", "No cleaning disk was detected.");
        else if (result != ADFResult::adfrAborted)
            ui.showError("Drive Cleaner", "The cleaning cycle failed.");
    });
    dialog.exec();
}

void MainWindow::showAbout()
{
    QMessageBox box(this);
    box.setWindowTitle("About DrawBridge");
    box.setIconPixmap(QPixmap(":/drawbridge/floppy.bmp"));
    box.setTextFormat(Qt::RichText);
    box.setText("<b>DrawBridge</b><br>Arduino Amiga Floppy Disk Reader and Writer<br>"
                "Version 2.8.8 Qt<br><br>Copyright © 2017–2024 Robert Smith<br>"
                "<a href='https://amiga.robsmithdev.co.uk'>Project website</a> · "
                "<a href='https://discord.gg/HctVgSFEXu'>Discord</a>");
    box.exec();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
}
