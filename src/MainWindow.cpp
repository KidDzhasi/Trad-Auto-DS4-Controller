#include "MainWindow.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QMessageBox>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QSet>
#include <QStyle>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_process(new QProcess(this)),
      m_countdownTimer(new QTimer(this)),
      m_cmdTimeoutTimer(new QTimer(this)),
      m_autoRefreshTimer(new QTimer(this)) {

    setWindowTitle("Trad Auto-DS4 Controller");
    setWindowIcon(QIcon(":/assets/trad-auto-ds4-controller.svg"));
    setMinimumSize(660, 500);
    resize(700, 540);

    setupUi();
    applyTheme();
    loadSettings();

    connect(m_process, &QProcess::readyReadStandardOutput, this, &MainWindow::handleProcessStdout);
    connect(m_process, &QProcess::readyReadStandardError, this, &MainWindow::handleProcessStderr);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::handleProcessFinished);

    m_countdownTimer->setInterval(1000);
    connect(m_countdownTimer, &QTimer::timeout, this, &MainWindow::handleCountdownTick);

    m_cmdTimeoutTimer->setSingleShot(true);
    connect(m_cmdTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_process->state() != QProcess::NotRunning) {
            appendLog("Operation timed out. Stopping bluetoothctl process...", "#fbbf24");
            m_process->terminate();
            QTimer::singleShot(1500, this, [this]() {
                if (m_process->state() != QProcess::NotRunning) {
                    m_process->kill();
                }
            });
        }
    });

    // Background auto-refresh every 3 seconds
    m_autoRefreshTimer->setInterval(3000);
    connect(m_autoRefreshTimer, &QTimer::timeout, this, &MainWindow::onAutoRefreshTick);
    m_autoRefreshTimer->start();

    appendLog("Ready. Default G-Controller MAC: " + m_targetMac, "#38bdf8");
    QTimer::singleShot(150, this, &MainWindow::triggerSilentStatusUpdate);
}

MainWindow::~MainWindow() {
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
    }
}

void MainWindow::setupUi() {
    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(12);

    // 1. Clean Compact Header
    auto *headerFrame = new QFrame(this);
    headerFrame->setObjectName("headerFrame");
    auto *headerLayout = new QHBoxLayout(headerFrame);
    headerLayout->setContentsMargins(14, 10, 14, 10);
    headerLayout->setSpacing(12);

    auto *iconLabel = new QLabel(this);
    iconLabel->setPixmap(QIcon(":/assets/trad-auto-ds4-controller.svg").pixmap(42, 42));
    headerLayout->addWidget(iconLabel);

    auto *titleCol = new QVBoxLayout();
    titleCol->setSpacing(1);
    auto *titleLabel = new QLabel("TRAD AUTO-DS4 CONTROLLER", this);
    titleLabel->setObjectName("appTitle");
    auto *subtitleLabel = new QLabel("Generic & DualShock 4 Bluetooth Controller Manager • TRAD (KidDzhasi)", this);
    subtitleLabel->setObjectName("appSubtitle");
    titleCol->addWidget(titleLabel);
    titleCol->addWidget(subtitleLabel);
    headerLayout->addLayout(titleCol, 1);

    mainLayout->addWidget(headerFrame);

    // 2. Controller Status Card (Auto-Refreshing)
    auto *statusGroup = new QGroupBox("Controller Status (Auto-Refresh)", this);
    auto *statusLayout = new QVBoxLayout(statusGroup);
    statusLayout->setContentsMargins(14, 16, 14, 12);
    statusLayout->setSpacing(10);

    auto *infoRow = new QHBoxLayout();
    m_deviceNameLabel = new QLabel("Wireless Controller", this);
    m_deviceNameLabel->setObjectName("deviceNameLabel");

    m_controllerTypeBadge = new QLabel("Generic (G-Controller)", this);
    m_controllerTypeBadge->setObjectName("typeBadgeGeneric");

    m_macDisplayLabel = new QLabel("Default MAC: --", this);
    m_macDisplayLabel->setObjectName("macDisplayLabel");

    infoRow->addWidget(m_deviceNameLabel);
    infoRow->addSpacing(8);
    infoRow->addWidget(m_controllerTypeBadge);
    infoRow->addStretch();
    infoRow->addWidget(m_macDisplayLabel);
    statusLayout->addLayout(infoRow);

    auto *badgesGrid = new QGridLayout();
    badgesGrid->setSpacing(8);

    auto createBadgeCard = [this](const QString &title, QLabel **valueBadge) -> QFrame * {
        auto *card = new QFrame(this);
        card->setObjectName("badgeCard");
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(10, 8, 10, 8);
        layout->setSpacing(4);
        auto *lblTitle = new QLabel(title, card);
        lblTitle->setObjectName("badgeTitle");
        *valueBadge = new QLabel("--", card);
        (*valueBadge)->setAlignment(Qt::AlignCenter);
        (*valueBadge)->setMinimumHeight(26);
        layout->addWidget(lblTitle, 0, Qt::AlignCenter);
        layout->addWidget(*valueBadge);
        return card;
    };

    badgesGrid->addWidget(createBadgeCard("Paired", &m_pairedBadge), 0, 0);
    badgesGrid->addWidget(createBadgeCard("Trusted", &m_trustedBadge), 0, 1);
    badgesGrid->addWidget(createBadgeCard("Connected", &m_connectedBadge), 0, 2);
    badgesGrid->addWidget(createBadgeCard("Gamepad HID", &m_hidBadge), 0, 3);
    statusLayout->addLayout(badgesGrid);

    m_healthBanner = new QLabel("Checking controller status...", this);
    m_healthBanner->setObjectName("healthBanner");
    m_healthBanner->setWordWrap(true);
    statusLayout->addWidget(m_healthBanner);

    mainLayout->addWidget(statusGroup);

    // 3. Streamlined Action Bar (Scan Controller | Connect G-Controller | Disconnect)
    auto *actionsFrame = new QFrame(this);
    actionsFrame->setObjectName("actionsFrame");
    auto *actionsLayout = new QVBoxLayout(actionsFrame);
    actionsLayout->setContentsMargins(14, 12, 14, 12);
    actionsLayout->setSpacing(8);

    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);

    m_scanBtn = new QPushButton("Scan Controller", this);
    m_scanBtn->setObjectName("scanBtn");
    m_scanBtn->setToolTip("Scan Bluetooth, identify Generic vs Original DS4, and set Generic MAC as default");

    m_connectGBtn = new QPushButton("Connect G-Controller", this);
    m_connectGBtn->setObjectName("connectBtn");
    m_connectGBtn->setToolTip("Connect Generic Controller using repair-ds4 (remove stale entry, scan 10s, pair, trust & connect)");

    m_disconnectBtn = new QPushButton("Disconnect", this);
    m_disconnectBtn->setObjectName("dangerBtn");
    m_disconnectBtn->setToolTip("Disconnect current controller");

    btnRow->addWidget(m_scanBtn, 1);
    btnRow->addWidget(m_connectGBtn, 2);
    btnRow->addWidget(m_disconnectBtn, 1);
    actionsLayout->addLayout(btnRow);

    auto *progressRow = new QHBoxLayout();
    m_operationLabel = new QLabel("Auto-refresh active.", this);
    m_operationLabel->setObjectName("appSubtitle");
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setVisible(false);

    progressRow->addWidget(m_operationLabel, 1);
    progressRow->addWidget(m_progressBar, 1);
    actionsLayout->addLayout(progressRow);

    mainLayout->addWidget(actionsFrame);

    // 4. Compact Activity Log
    auto *logGroup = new QGroupBox("Activity Log", this);
    auto *logLayout = new QVBoxLayout(logGroup);
    logLayout->setContentsMargins(12, 14, 12, 10);
    logLayout->setSpacing(6);

    m_logViewer = new QPlainTextEdit(this);
    m_logViewer->setReadOnly(true);
    m_logViewer->setObjectName("logViewer");

    auto *logFooter = new QHBoxLayout();
    logFooter->addStretch();
    m_clearLogBtn = new QPushButton("Clear", this);
    m_clearLogBtn->setObjectName("secondaryBtn");
    logFooter->addWidget(m_clearLogBtn);

    logLayout->addWidget(m_logViewer, 1);
    logLayout->addLayout(logFooter);

    mainLayout->addWidget(logGroup, 1);

    // Signals
    connect(m_scanBtn, &QPushButton::clicked, this, &MainWindow::onScanController);
    connect(m_connectGBtn, &QPushButton::clicked, this, &MainWindow::onConnectGController);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &MainWindow::onDisconnect);
    connect(m_clearLogBtn, &QPushButton::clicked, this, &MainWindow::onClearLog);
}

void MainWindow::applyTheme() {
    setStyleSheet(R"(
        QMainWindow, QWidget {
            background-color: #0f172a;
            color: #f1f5f9;
            font-family: "Cantarell", "Inter", "Segoe UI", sans-serif;
            font-size: 13px;
        }
        #headerFrame, #actionsFrame {
            background-color: #1e293b;
            border: 1px solid #334155;
            border-radius: 10px;
        }
        #appTitle {
            font-size: 16px;
            font-weight: 800;
            color: #38bdf8;
            letter-spacing: 1px;
        }
        #appSubtitle {
            font-size: 12px;
            color: #94a3b8;
        }
        QGroupBox {
            background-color: #1e293b;
            border: 1px solid #334155;
            border-radius: 10px;
            margin-top: 8px;
            font-weight: 700;
            color: #e2e8f0;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px;
            color: #38bdf8;
        }
        #badgeCard {
            background-color: #0f172a;
            border: 1px solid #334155;
            border-radius: 8px;
        }
        #badgeTitle {
            color: #94a3b8;
            font-size: 11px;
            font-weight: 600;
        }
        #deviceNameLabel {
            font-size: 14px;
            font-weight: 700;
            color: #f8fafc;
        }
        #typeBadgeGeneric {
            background-color: #312e81;
            color: #c7d2fe;
            border: 1px solid #6366f1;
            border-radius: 5px;
            padding: 2px 8px;
            font-size: 11px;
            font-weight: 700;
        }
        #typeBadgeOriginal {
            background-color: #0c4a6e;
            color: #bae6fd;
            border: 1px solid #0284c7;
            border-radius: 5px;
            padding: 2px 8px;
            font-size: 11px;
            font-weight: 700;
        }
        #macDisplayLabel {
            font-family: "JetBrains Mono", "Monospace", monospace;
            font-size: 12px;
            color: #38bdf8;
            font-weight: 700;
        }
        #healthBanner {
            background-color: #0f172a;
            border-left: 3px solid #38bdf8;
            border-radius: 4px;
            padding: 6px 10px;
            color: #cbd5e1;
            font-size: 12px;
        }
        QPushButton {
            border-radius: 7px;
            padding: 10px 14px;
            font-weight: 700;
            border: 1px solid transparent;
        }
        QPushButton:disabled {
            background-color: #334155;
            color: #64748b;
            border-color: #475569;
        }
        #connectBtn {
            background-color: #0284c7;
            color: #ffffff;
            font-size: 14px;
        }
        #connectBtn:hover:!disabled {
            background-color: #0369a1;
        }
        #scanBtn {
            background-color: #4f46e5;
            color: #ffffff;
        }
        #scanBtn:hover:!disabled {
            background-color: #4338ca;
        }
        #dangerBtn {
            background-color: #dc2626;
            color: #ffffff;
        }
        #dangerBtn:hover:!disabled {
            background-color: #b91c1c;
        }
        #secondaryBtn {
            background-color: #334155;
            color: #cbd5e1;
            border: 1px solid #475569;
            padding: 4px 10px;
            font-size: 11px;
        }
        #secondaryBtn:hover:!disabled {
            background-color: #475569;
        }
        #logViewer {
            background-color: #090d16;
            border: 1px solid #334155;
            border-radius: 7px;
            padding: 6px;
            font-family: "JetBrains Mono", "Fira Code", "Monospace", monospace;
            font-size: 11px;
            color: #e2e8f0;
        }
        QProgressBar {
            background-color: #0f172a;
            border: 1px solid #334155;
            border-radius: 3px;
        }
        QProgressBar::chunk {
            background-color: #38bdf8;
            border-radius: 2px;
        }
    )");
}

void MainWindow::loadSettings() {
    const QString home = QDir::homePath();
    const QString primaryIni = home + "/.config/trad-auto-ds4-controller/config.ini";
    const QString compatIni = home + "/.config/trad-repair-ds4/config.ini";

    QString loadedMac = DEFAULT_MAC;
    QString loadedType = "Generic (G-Controller)";

    if (QFile::exists(primaryIni)) {
        QSettings s(primaryIni, QSettings::IniFormat);
        loadedMac = s.value("Controller/MAC", DEFAULT_MAC).toString().trimmed().toUpper();
        loadedType = s.value("Controller/Type", "Generic (G-Controller)").toString();
    } else if (QFile::exists(compatIni)) {
        QSettings s(compatIni, QSettings::IniFormat);
        loadedMac = s.value("Controller/MAC", DEFAULT_MAC).toString().trimmed().toUpper();
    }

    static const QRegularExpression macRe("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    if (!macRe.match(loadedMac).hasMatch()) {
        loadedMac = DEFAULT_MAC;
    }

    saveSettings(loadedMac, loadedType);
}

void MainWindow::saveSettings(const QString &mac, const QString &controllerType) {
    m_targetMac = mac.trimmed().toUpper();
    m_controllerType = controllerType;

    const QString home = QDir::homePath();
    const QStringList configPaths = {
        home + "/.config/trad-auto-ds4-controller/config.ini",
        home + "/.config/trad-repair-ds4/config.ini"
    };

    for (const QString &path : configPaths) {
        QSettings settings(path, QSettings::IniFormat);
        settings.setValue("Controller/MAC", m_targetMac);
        settings.setValue("Controller/Type", m_controllerType);
        settings.sync();
    }

    m_macDisplayLabel->setText("Default MAC: " + m_targetMac);
    m_controllerTypeBadge->setText(m_controllerType);
    if (m_controllerType.contains("Original", Qt::CaseInsensitive)) {
        m_controllerTypeBadge->setObjectName("typeBadgeOriginal");
    } else {
        m_controllerTypeBadge->setObjectName("typeBadgeGeneric");
    }
    m_controllerTypeBadge->style()->unpolish(m_controllerTypeBadge);
    m_controllerTypeBadge->style()->polish(m_controllerTypeBadge);
}

QString MainWindow::classifyControllerType(const QString &mac, const QString &infoOutput, QString *reasonOut) const {
    const QString cleanMac = mac.trimmed().toUpper();
    const QString oui = cleanMac.left(8);

    // Known genuine Sony Interactive Entertainment IEEE OUIs
    static const QSet<QString> sonyOuis = {
        "00:04:1F", "00:13:A9", "00:19:C5", "00:1D:BA", "00:1F:E2",
        "00:24:BE", "00:D9:D1", "28:0D:FC", "30:0E:D5", "40:B8:9A",
        "5C:96:56", "60:5B:B4", "64:D4:BD", "70:9E:29", "78:C8:81",
        "84:17:66", "90:3A:E6", "98:B6:E9", "A0:AB:51", "A8:E3:EE",
        "AC:89:95", "B8:78:26", "BC:60:A7", "C8:63:F1", "D4:4B:5E",
        "F8:46:1C", "FC:0F:E6"
    };

    bool ok = false;
    const int firstByte = cleanMac.left(2).toInt(&ok, 16);
    const bool hasNonStandardIeeeBits = ok && ((firstByte & 0x03) != 0);
    const bool hasUnbondedBredr =
        infoOutput.contains("Paired: yes", Qt::CaseInsensitive) &&
        infoOutput.contains("Bonded: no", Qt::CaseInsensitive);

    if (hasNonStandardIeeeBits || !sonyOuis.contains(oui)) {
        if (reasonOut) {
            *reasonOut = QString("Non-Sony OUI (%1)%2")
                             .arg(oui, hasUnbondedBredr ? " + Unbonded BR/EDR profile" : "");
        }
        return "Generic (G-Controller)";
    }

    if (hasUnbondedBredr) {
        if (reasonOut) {
            *reasonOut = "Unbonded BR/EDR clone pairing behavior";
        }
        return "Generic (G-Controller)";
    }

    if (reasonOut) {
        *reasonOut = QString("Genuine Sony OUI (%1)").arg(oui);
    }
    return "Original (Sony DS4)";
}

bool MainWindow::isHidInputActive() const {
    QFile f("/proc/bus/input/devices");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    const QString content = QString::fromUtf8(f.readAll());
    return content.contains("Wireless Controller", Qt::CaseInsensitive) ||
           content.contains("054C", Qt::CaseInsensitive);
}

bool MainWindow::queryDbusServicesResolved(const QString &mac) const {
    QString devPath = mac;
    devPath.replace(':', '_');
    QProcess busctl;
    busctl.start("busctl", {"get-property", "org.bluez",
                            "/org/bluez/hci0/dev_" + devPath,
                            "org.bluez.Device1", "ServicesResolved"});
    if (!busctl.waitForFinished(1000)) {
        return false;
    }
    const QString out = QString::fromUtf8(busctl.readAllStandardOutput()).trimmed();
    return out.contains("true", Qt::CaseInsensitive);
}

void MainWindow::appendLog(const QString &text, const QString &color) {
    const QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    const QString escaped = text.toHtmlEscaped();
    m_logViewer->appendHtml(
        QString("<span style='color:#64748b;'>[%1]</span> <span style='color:%2;'>%3</span>")
            .arg(timestamp, color, escaped));
    if (QScrollBar *sb = m_logViewer->verticalScrollBar()) {
        sb->setValue(sb->maximum());
    }
}

void MainWindow::setBusy(bool busy, const QString &statusMsg) {
    m_scanBtn->setEnabled(!busy);
    m_connectGBtn->setEnabled(!busy);
    m_disconnectBtn->setEnabled(!busy);

    m_progressBar->setVisible(busy);
    if (busy) {
        m_progressBar->setRange(0, 0);
    } else {
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(100);
    }

    if (!statusMsg.isEmpty()) {
        m_operationLabel->setText(statusMsg);
    } else if (!busy) {
        m_operationLabel->setText("Auto-refresh active.");
    }
}

void MainWindow::updateBadge(QLabel *badge, const QString &stateText, bool isPositive, bool isNeutral) {
    badge->setText(stateText);
    if (isNeutral) {
        badge->setStyleSheet(
            "background-color: #334155; color: #cbd5e1; border-radius: 5px; padding: 4px 8px; font-weight: 700; font-size: 12px;");
    } else if (isPositive) {
        badge->setStyleSheet(
            "background-color: #065f46; color: #34d399; border: 1px solid #10b981; border-radius: 5px; padding: 4px 8px; font-weight: 700; font-size: 12px;");
    } else {
        badge->setStyleSheet(
            "background-color: #7f1d1d; color: #fca5a5; border: 1px solid #ef4444; border-radius: 5px; padding: 4px 8px; font-weight: 700; font-size: 12px;");
    }
}

void MainWindow::parseBluetoothInfo(const QString &output) {
    m_macDisplayLabel->setText("Default MAC: " + m_targetMac);

    if (output.contains("not available", Qt::CaseInsensitive) || output.trimmed().isEmpty()) {
        m_deviceNameLabel->setText("Wireless Controller");
        updateBadge(m_pairedBadge, "no", false);
        updateBadge(m_trustedBadge, "no", false);
        updateBadge(m_connectedBadge, "no", false);
        updateBadge(m_hidBadge, "Inactive", false);
        m_healthBanner->setText(
            "Controller not paired. Click 'Connect G-Controller' (or 'Scan Controller' to detect a different controller).");
        return;
    }

    auto extractField = [&output](const QString &key) -> QString {
        QRegularExpression re(QString("^\\s*%1:\\s*(.+)$").arg(QRegularExpression::escape(key)),
                              QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch m = re.match(output);
        return m.hasMatch() ? m.captured(1).trimmed() : QString();
    };

    const QString name = extractField("Name");
    const QString alias = extractField("Alias");
    const QString paired = extractField("Paired").toLower();
    const QString trusted = extractField("Trusted").toLower();
    const QString connected = extractField("Connected").toLower();

    const bool isPaired = (paired == "yes");
    const bool isTrusted = (trusted == "yes");
    const bool isConnected = (connected == "yes");
    const bool hidActive = isHidInputActive();
    const bool servicesResolved = isConnected ? queryDbusServicesResolved(m_targetMac) : false;

    m_deviceNameLabel->setText(!alias.isEmpty() ? alias : (!name.isEmpty() ? name : "Wireless Controller"));

    updateBadge(m_pairedBadge, isPaired ? "yes" : "no", isPaired);
    updateBadge(m_trustedBadge, isTrusted ? "yes" : "no", isTrusted);
    updateBadge(m_connectedBadge, isConnected ? "yes" : "no", isConnected);
    updateBadge(m_hidBadge, (isConnected && hidActive) ? "Active" : "Inactive", isConnected && hidActive);

    const QString sig = QString("%1|%2|%3|%4").arg(paired, trusted, connected, hidActive ? "active" : "inactive");
    if (sig != m_lastStatusSignature) {
        m_lastStatusSignature = sig;
        appendLog(QString("Status [%1]: Paired=%2, Trusted=%3, Connected=%4, HID=%5")
                      .arg(m_targetMac, isPaired ? "yes" : "no", isTrusted ? "yes" : "no",
                           isConnected ? "yes" : "no", hidActive ? "ACTIVE" : "INACTIVE"),
                  (isConnected && hidActive) ? "#34d399" : "#fbbf24");
    }

    if (isConnected && hidActive && servicesResolved) {
        m_healthBanner->setText("Connected & Ready — Gamepad HID input is active.");
    } else if (isConnected && !hidActive) {
        m_healthBanner->setText("White-LED Stall Detected — Click 'Connect G-Controller' to repair and restore input.");
    } else {
        m_healthBanner->setText("Disconnected — Click 'Connect G-Controller' and hold SHARE + Home to connect.");
    }
}

void MainWindow::triggerSilentStatusUpdate() {
    if (m_state != WorkflowState::Idle || m_process->state() != QProcess::NotRunning) {
        return;
    }
    m_state = WorkflowState::CheckingStatus;
    m_silentCurrentCommand = true;
    m_accumulatedStdout.clear();
    m_cmdTimeoutTimer->start(4000);
    m_process->start("bluetoothctl", {"info", m_targetMac});
}

void MainWindow::onAutoRefreshTick() {
    triggerSilentStatusUpdate();
}

void MainWindow::onScanController() {
    m_autoRefreshTimer->stop();
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(500);
    }

    m_state = WorkflowState::ScanningForController;
    m_silentCurrentCommand = true;
    m_accumulatedStdout.clear();
    m_secondsRemaining = 8;

    setBusy(true, "Scanning Bluetooth for controllers (8s)... Hold SHARE + Home if pairing a new controller.");
    m_progressBar->setRange(0, 8);
    m_progressBar->setValue(0);
    m_countdownTimer->start();

    appendLog("Scanning Bluetooth for controllers (8s)...", "#38bdf8");

    // Scans for 8 seconds, then dumps discovered devices and detailed info for each
    const QString scanScript = R"(
        bluetoothctl --timeout 8 scan on 2>&1 || true
        echo "===DEVICES==="
        bluetoothctl devices || true
        for mac in $(bluetoothctl devices | awk '{print $2}'); do
            echo "===INFO:$mac==="
            bluetoothctl info "$mac" || true
        done
    )";

    m_cmdTimeoutTimer->start(18000);
    m_process->start("bash", {"-c", scanScript});
}

void MainWindow::processScanResults(const QString &output) {
    struct Candidate {
        QString mac;
        QString name;
        QString info;
        bool seenLiveInScan = false;
    };

    QList<Candidate> candidates;
    const QString scanPart = output.section("===DEVICES===", 0, 0);

    // Parse each ===INFO:<MAC>=== block
    const QStringList blocks = output.split("===INFO:");
    for (int i = 1; i < blocks.size(); ++i) {
        const QString &blk = blocks[i];
        const QString mac = blk.section("===", 0, 0).trimmed().toUpper();
        const QString info = blk.section("===", 1);

        const bool isGamepad =
            info.contains("Wireless Controller", Qt::CaseInsensitive) ||
            info.contains("DualShock", Qt::CaseInsensitive) ||
            info.contains("Gamepad", Qt::CaseInsensitive) ||
            info.contains("input-gaming", Qt::CaseInsensitive) ||
            info.contains("0x00002508", Qt::CaseInsensitive) ||
            info.contains("v054C", Qt::CaseInsensitive);

        if (!isGamepad) {
            continue;
        }

        QRegularExpression nameRe("^\\s*Name:\\s*(.+)$", QRegularExpression::MultilineOption);
        QRegularExpressionMatch m = nameRe.match(info);
        const QString name = m.hasMatch() ? m.captured(1).trimmed() : "Wireless Controller";

        Candidate c;
        c.mac = mac;
        c.name = name;
        c.info = info;
        c.seenLiveInScan = scanPart.contains(mac, Qt::CaseInsensitive);
        candidates.append(c);
    }

    // Also check if a controller appeared in scanPart by name even if info wasn't populated
    if (candidates.isEmpty()) {
        QRegularExpression scanRe("Device\\s+(([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2})\\s+(Wireless Controller|DualShock.*|PS4.*|.*Controller.*)",
                                  QRegularExpression::CaseInsensitiveOption);
        auto it = scanRe.globalMatch(scanPart);
        while (it.hasNext()) {
            auto match = it.next();
            Candidate c;
            c.mac = match.captured(1).toUpper();
            c.name = match.captured(3).trimmed();
            c.seenLiveInScan = true;
            candidates.append(c);
        }
    }

    if (candidates.isEmpty()) {
        appendLog("No controller detected during scan. Keep default MAC: " + m_targetMac, "#fbbf24");
        return;
    }

    // Prioritize a controller that actively broadcast during the live scan
    Candidate chosen = candidates.first();
    for (const Candidate &c : candidates) {
        if (c.seenLiveInScan) {
            chosen = c;
            break;
        }
    }

    QString reason;
    const QString detectedType = classifyControllerType(chosen.mac, chosen.info, &reason);

    appendLog(QString("Detected Controller: %1 [%2]").arg(chosen.name, chosen.mac), "#38bdf8");
    appendLog(QString("Classification: %1 — %2").arg(detectedType, reason), "#c084fc");

    if (detectedType.contains("Generic", Qt::CaseInsensitive)) {
        saveSettings(chosen.mac, "Generic (G-Controller)");
        appendLog(QString("Generic controller identified! Set %1 as default MAC until next scan.").arg(chosen.mac), "#34d399");
    } else {
        saveSettings(m_targetMac, "Original (Sony DS4)");
        appendLog(QString("Original Sony DS4 identified (%1). Default G-Controller MAC remains %2.").arg(chosen.mac, m_targetMac), "#38bdf8");
    }
}

void MainWindow::onConnectGController() {
    m_autoRefreshTimer->stop();
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(500);
    }

    m_state = WorkflowState::RunningRepairScript;
    m_silentCurrentCommand = false;
    m_promptShownForCurrentRepair = false;
    m_accumulatedStdout.clear();

    const QString repairScriptPath = QDir::homePath() + "/.local/bin/repair-ds4";
    setBusy(true, "Connecting G-Controller (" + m_targetMac + ")...");
    appendLog("=== Connect G-Controller (" + m_targetMac + ") ===", "#38bdf8");

    m_process->start(repairScriptPath, {m_targetMac});
}

void MainWindow::showRepairPairingPrompt() {
    if (m_state != WorkflowState::RunningRepairScript || m_process->state() == QProcess::NotRunning) {
        return;
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Connect G-Controller — Pairing Mode");
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText(
        QString("<b>Stale entry for <code>%1</code> cleared.</b><br><br>"
                "1. Hold <b><span style='color:#38bdf8;'>SHARE + Home (PS)</span></b> on your G-Controller until the light bar <b>double-flashes</b>.<br>"
                "2. Click <b>OK (Start 10s Scan & Connect)</b> below while it is double-flashing.")
            .arg(m_targetMac));
    QPushButton *okBtn = msgBox.addButton("OK (Start 10s Scan & Connect)", QMessageBox::AcceptRole);
    msgBox.addButton(QMessageBox::Cancel);
    msgBox.setDefaultButton(okBtn);
    msgBox.exec();

    if (msgBox.clickedButton() != okBtn) {
        appendLog("Connect G-Controller cancelled.", "#fbbf24");
        m_process->kill();
        m_process->waitForFinished(500);
        m_state = WorkflowState::Idle;
        setBusy(false, "Cancelled.");
        m_autoRefreshTimer->start();
        triggerSilentStatusUpdate();
        return;
    }

    m_secondsRemaining = 10;
    m_progressBar->setRange(0, 14);
    m_progressBar->setValue(0);
    m_operationLabel->setText(QString("Scanning for G-Controller %1 (10s)...").arg(m_targetMac));
    m_countdownTimer->start();
    m_cmdTimeoutTimer->start(40000);

    m_process->write("\n");
}

void MainWindow::onDisconnect() {
    m_autoRefreshTimer->stop();
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(500);
    }

    setBusy(true, "Disconnecting " + m_targetMac + "...");
    appendLog("$ bluetoothctl disconnect " + m_targetMac, "#38bdf8");
    m_state = WorkflowState::Disconnecting;
    m_silentCurrentCommand = false;
    m_accumulatedStdout.clear();
    m_cmdTimeoutTimer->start(8000);
    m_process->start("bluetoothctl", {"disconnect", m_targetMac});
}

void MainWindow::handleCountdownTick() {
    if (m_state != WorkflowState::RunningRepairScript && m_state != WorkflowState::ScanningForController) {
        m_countdownTimer->stop();
        return;
    }

    int val = m_progressBar->value() + 1;
    if (val <= m_progressBar->maximum()) {
        m_progressBar->setValue(val);
    }

    if (m_secondsRemaining > 0) {
        m_secondsRemaining--;
    }

    if (m_state == WorkflowState::ScanningForController) {
        m_operationLabel->setText(QString("Scanning Bluetooth for controllers (%1s remaining)...").arg(m_secondsRemaining));
    } else if (m_secondsRemaining > 0) {
        m_operationLabel->setText(QString("Scanning for G-Controller %1 (%2s remaining)...").arg(m_targetMac).arg(m_secondsRemaining));
    } else {
        m_operationLabel->setText(QString("Pairing, trusting, and connecting %1...").arg(m_targetMac));
    }
}

void MainWindow::handleProcessStdout() {
    const QString out = QString::fromUtf8(m_process->readAllStandardOutput());
    m_accumulatedStdout += out;

    if (m_silentCurrentCommand) {
        return;
    }

    for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
        const QString clean = line.trimmed();
        if (!clean.isEmpty()) {
            appendLog(clean, "#e2e8f0");
        }
    }

    if (m_state == WorkflowState::RunningRepairScript && !m_promptShownForCurrentRepair &&
        m_accumulatedStdout.contains("Put the controller into pairing mode", Qt::CaseInsensitive)) {
        m_promptShownForCurrentRepair = true;
        QTimer::singleShot(50, this, &MainWindow::showRepairPairingPrompt);
    }
}

void MainWindow::handleProcessStderr() {
    const QString err = QString::fromUtf8(m_process->readAllStandardError());
    if (m_silentCurrentCommand) {
        return;
    }
    for (const QString &line : err.split('\n', Qt::SkipEmptyParts)) {
        const QString clean = line.trimmed();
        if (!clean.isEmpty()) {
            appendLog("[stderr] " + clean, "#f87171");
        }
    }
}

void MainWindow::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitStatus);
    m_cmdTimeoutTimer->stop();
    m_countdownTimer->stop();

    const WorkflowState finishedState = m_state;
    m_state = WorkflowState::Idle;
    m_silentCurrentCommand = false;

    if (!m_autoRefreshTimer->isActive()) {
        m_autoRefreshTimer->start();
    }

    switch (finishedState) {
    case WorkflowState::CheckingStatus:
        parseBluetoothInfo(m_accumulatedStdout);
        break;

    case WorkflowState::ScanningForController:
        processScanResults(m_accumulatedStdout);
        setBusy(false, "Scan complete.");
        QTimer::singleShot(250, this, &MainWindow::triggerSilentStatusUpdate);
        break;

    case WorkflowState::RunningRepairScript:
        if (exitCode == 0) {
            appendLog("G-Controller connected and ready (" + m_targetMac + ")!", "#34d399");
            setBusy(false, "G-Controller connected!");
        } else {
            appendLog("Connect G-Controller failed — make sure the controller light bar was double-flashing during the scan.", "#f87171");
            setBusy(false, "Connection failed — retry while controller is double-flashing.");
        }
        QTimer::singleShot(300, this, &MainWindow::triggerSilentStatusUpdate);
        break;

    case WorkflowState::Disconnecting:
        appendLog("Disconnected " + m_targetMac + ".", "#38bdf8");
        setBusy(false, "Disconnected.");
        QTimer::singleShot(300, this, &MainWindow::triggerSilentStatusUpdate);
        break;

    default:
        setBusy(false);
        break;
    }
}

void MainWindow::onClearLog() {
    m_logViewer->clear();
}
