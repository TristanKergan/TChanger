#include "ipc_client.hpp"
#include "../ipc_protocol.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

IpcClient::IpcClient(QObject* parent)
    : QObject(parent) {
    connect(&pollTimer_, &QTimer::timeout, this, &IpcClient::checkConnection);
    // Poll every 1.5 seconds for game runtime presence
    pollTimer_.start(1500);
}

IpcClient::~IpcClient() {
    pollTimer_.stop();
}

QString IpcClient::socketPath() const {
    return QString::fromStdString(hamzex::ipc::GetSocketPath());
}

QString IpcClient::defaultConfigFile() {
    const char* home = std::getenv("HOME");
    QString base = (home && *home) ? QString::fromUtf8(home) : QStringLiteral(".");
    return base + QStringLiteral("/.config/hamzex/config.json");
}

void IpcClient::setPollingEnabled(bool enabled) {
    if (enabled) {
        if (!pollTimer_.isActive())
            pollTimer_.start(1500);
    } else {
        pollTimer_.stop();
    }
}

bool IpcClient::transact(const QByteArray& request, QByteArray& response, int timeoutMs) {
    response.clear();
    const std::string sPath = hamzex::ipc::GetSocketPath();

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return false;

    struct timeval tv{};
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, sPath.c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        close(fd);
        return false;
    }

    QByteArray wireData = request;
    if (!wireData.endsWith('\n'))
        wireData.append('\n');

    if (!hamzex::ipc::WriteAll(fd, wireData.constData(), static_cast<std::size_t>(wireData.size()))) {
        close(fd);
        return false;
    }

    char buf[4096];
    while (response.size() < static_cast<qsizetype>(hamzex::ipc::kMaxPacketSize)) {
        struct pollfd pfd{};
        pfd.fd = fd;
        pfd.events = POLLIN;
        int pret = poll(&pfd, 1, timeoutMs);
        if (pret <= 0) break;

        ssize_t n = read(fd, buf, sizeof(buf));
        if (n <= 0) break;
        response.append(buf, n);
        if (response.contains('\n')) break;
    }

    close(fd);
    return !response.isEmpty();
}

bool IpcClient::ping(quint64* outVersion) {
    QByteArray req = "{\"cmd\":\"ping\"}\n";
    QByteArray resp;
    bool ok = transact(req, resp, 200);
    if (!ok) {
        if (connected_) {
            connected_ = false;
            emit connectionStatusChanged(false, lastVersion_);
        }
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(resp);
    if (!doc.isObject())
        return false;

    QJsonObject obj = doc.object();
    if (obj.value(QStringLiteral("status")).toString() != QLatin1String("ok"))
        return false;

    quint64 v = static_cast<quint64>(obj.value(QStringLiteral("config_version")).toInteger(0));
    lastVersion_ = v;
    if (outVersion) *outVersion = v;

    if (!connected_) {
        connected_ = true;
        emit connectionStatusChanged(true, lastVersion_);
    }
    return true;
}

void IpcClient::checkConnection() {
    ping();
}

bool IpcClient::getConfig(QByteArray& outJson, quint64* outVersion) {
    QByteArray req = "{\"cmd\":\"get_config\"}\n";
    QByteArray resp;
    if (!transact(req, resp, 500))
        return false;

    QJsonDocument doc = QJsonDocument::fromJson(resp);
    if (!doc.isObject())
        return false;

    QJsonObject obj = doc.object();
    if (obj.value(QStringLiteral("status")).toString() != QLatin1String("ok"))
        return false;

    quint64 v = static_cast<quint64>(obj.value(QStringLiteral("config_version")).toInteger(0));
    lastVersion_ = v;
    if (outVersion) *outVersion = v;

    QJsonValue cfgVal = obj.value(QStringLiteral("config"));
    if (cfgVal.isObject()) {
        QJsonDocument cfgDoc(cfgVal.toObject());
        outJson = cfgDoc.toJson(QJsonDocument::Indented);
    } else {
        outJson = resp;
    }

    emit configReceived(outJson, lastVersion_);
    return true;
}

bool IpcClient::applyConfig(const QByteArray& jsonPayload, quint64* outVersion, QString* outError) {
    // Validate JSON payload locally first
    QJsonParseError parseErr{};
    QJsonDocument userDoc = QJsonDocument::fromJson(jsonPayload, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !userDoc.isObject()) {
        if (outError) *outError = QStringLiteral("Invalid JSON: ") + parseErr.errorString();
        emit configApplied(false, *outError, lastVersion_);
        return false;
    }

    // Wrap in {"cmd":"apply_config","config":...}
    QJsonObject rootObj;
    rootObj[QStringLiteral("cmd")] = QStringLiteral("apply_config");
    rootObj[QStringLiteral("config")] = userDoc.object();

    QJsonDocument reqDoc(rootObj);
    QByteArray req = reqDoc.toJson(QJsonDocument::Compact);

    QByteArray resp;
    if (!transact(req, resp, 800)) {
        // Not connected to runtime -> Fallback: save to disk directly
        bool diskOk = saveToDisk(jsonPayload);
        if (outError) {
            *outError = diskOk
                ? QStringLiteral("Runtime not active. Config saved to disk successfully.")
                : QStringLiteral("Runtime not active and failed to write config file to disk.");
        }
        if (connected_) {
            connected_ = false;
            emit connectionStatusChanged(false, lastVersion_);
        }
        emit configApplied(diskOk, *outError, lastVersion_);
        return diskOk;
    }

    QJsonDocument doc = QJsonDocument::fromJson(resp);
    if (!doc.isObject()) {
        if (outError) *outError = QStringLiteral("Invalid response from runtime");
        emit configApplied(false, *outError, lastVersion_);
        return false;
    }

    QJsonObject obj = doc.object();
    if (obj.value(QStringLiteral("status")).toString() == QLatin1String("ok")) {
        quint64 v = static_cast<quint64>(obj.value(QStringLiteral("config_version")).toInteger(0));
        lastVersion_ = v;
        if (outVersion) *outVersion = v;
        if (!connected_) {
            connected_ = true;
            emit connectionStatusChanged(true, lastVersion_);
        }
        QString msg = QStringLiteral("Applied to runtime successfully! (Snapshot v%1)").arg(v);
        emit configApplied(true, msg, v);
        return true;
    }

    QString errMsg = obj.value(QStringLiteral("message")).toString(QStringLiteral("Unknown error"));
    if (outError) *outError = errMsg;
    emit configApplied(false, errMsg, lastVersion_);
    return false;
}

bool IpcClient::loadFromDisk(QByteArray& outJson) {
    QString path = defaultConfigFile();
    QFile file(path);
    if (!file.exists()) {
        // Try Hamzex capitalized
        const char* home = std::getenv("HOME");
        QString alt = QString::fromUtf8(home ? home : ".") + QStringLiteral("/.config/Hamzex/config.json");
        if (QFile::exists(alt))
            path = alt;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;

    outJson = f.readAll();
    return !outJson.isEmpty();
}

bool IpcClient::saveToDisk(const QByteArray& jsonPayload) {
    QString path = defaultConfigFile();
    QFileInfo fi(path);
    QDir dir = fi.dir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    if (QFile::exists(path)) {
        QString bakPath = path + QStringLiteral(".bak");
        QFile::remove(bakPath);
        QFile::copy(path, bakPath);
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    qint64 written = f.write(jsonPayload);
    f.close();
    return written == jsonPayload.size();
}
