#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QProcess>
#include <QTimer>
#include <QProgressBar>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onAutoRefreshTick();
    void onScanController();
    void onConnectGController();
    void onDisconnect();
    void onClearLog();

    void handleProcessStdout();
    void handleProcessStderr();
    void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleCountdownTick();

private:
    enum class WorkflowState {
        Idle,
        CheckingStatus,
        ScanningForController,
        RunningRepairScript,
        Disconnecting
    };

    void setupUi();
    void applyTheme();
    void loadSettings();
    void saveSettings(const QString &mac, const QString &controllerType);

    bool isHidInputActive() const;
    bool queryDbusServicesResolved(const QString &mac) const;
    QString classifyControllerType(const QString &mac, const QString &infoOutput, QString *reasonOut = nullptr) const;

    void appendLog(const QString &text, const QString &color = "#e2e8f0");
    void setBusy(bool busy, const QString &statusMsg = QString());
    void updateBadge(QLabel *badge, const QString &stateText, bool isPositive, bool isNeutral = false);
    void parseBluetoothInfo(const QString &output);
    void processScanResults(const QString &output);

    void triggerSilentStatusUpdate();
    void showRepairPairingPrompt();

    // UI Elements - Status Card
    QLabel *m_deviceNameLabel = nullptr;
    QLabel *m_controllerTypeBadge = nullptr;
    QLabel *m_macDisplayLabel = nullptr;
    QLabel *m_pairedBadge = nullptr;
    QLabel *m_trustedBadge = nullptr;
    QLabel *m_connectedBadge = nullptr;
    QLabel *m_hidBadge = nullptr;
    QLabel *m_healthBanner = nullptr;

    // UI Elements - Actions
    QPushButton *m_scanBtn = nullptr;
    QPushButton *m_connectGBtn = nullptr;
    QPushButton *m_disconnectBtn = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_operationLabel = nullptr;

    // UI Elements - Log
    QPlainTextEdit *m_logViewer = nullptr;
    QPushButton *m_clearLogBtn = nullptr;

    // Process & Timers
    QProcess *m_process = nullptr;
    QTimer *m_countdownTimer = nullptr;
    QTimer *m_cmdTimeoutTimer = nullptr;
    QTimer *m_autoRefreshTimer = nullptr;

    WorkflowState m_state = WorkflowState::Idle;
    QString m_targetMac;
    QString m_controllerType = "Generic (G-Controller)";
    QString m_accumulatedStdout;
    int m_secondsRemaining = 0;
    bool m_silentCurrentCommand = false;
    bool m_promptShownForCurrentRepair = false;
    QString m_lastStatusSignature;

    static constexpr const char *DEFAULT_MAC = "89:56:AB:5D:AC:8E";
};
