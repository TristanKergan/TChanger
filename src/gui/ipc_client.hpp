#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QTimer>
#include <cstdint>

class IpcClient : public QObject {
    Q_OBJECT

public:
    explicit IpcClient(QObject* parent = nullptr);
    ~IpcClient() override;

    bool isConnected() const noexcept { return connected_; }
    quint64 lastVersion() const noexcept { return lastVersion_; }
    QString socketPath() const;
    static QString defaultConfigFile();

    // Fast synchronous IPC operations (< 1ms on local Unix domain socket)
    bool ping(quint64* outVersion = nullptr);
    bool getConfig(QByteArray& outJson, quint64* outVersion = nullptr);
    bool applyConfig(const QByteArray& jsonPayload, quint64* outVersion = nullptr, QString* outError = nullptr);

    // Direct disk operations (offline mode or manual disk save)
    static bool loadFromDisk(QByteArray& outJson);
    static bool saveToDisk(const QByteArray& jsonPayload);

public slots:
    void checkConnection();
    void setPollingEnabled(bool enabled);

signals:
    void connectionStatusChanged(bool connected, quint64 version);
    void configReceived(const QByteArray& json, quint64 version);
    void configApplied(bool success, const QString& message, quint64 version);

private:
    bool transact(const QByteArray& request, QByteArray& response, int timeoutMs = 250);

    bool connected_ = false;
    quint64 lastVersion_ = 0;
    QTimer pollTimer_;
};
