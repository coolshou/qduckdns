#include "duckdnscore.h"
#include <QTimer>
#include <QDateTime>
#include <QTime>
#include <QDir>
#include <QFile>
#include <QUrlQuery>
#include <QHostAddress>
#include <QDnsHostAddressRecord>
#include <QDebug>

DuckdnsCore::DuckdnsCore(QString domains,
                         QString token, QObject *parent)
    : QObject{parent}, m_domains(domains), m_token(token)
{
    //<domains>.duckdns.org

    if (!QDir().mkpath("/tmp/qduckdns"))
    {
        qWarning() << "Failed to create /tmp/qduckdns";
        // return 1;
    }
    m_logfile = "/tmp/qduckdns/qduckdns.log";

    m_url = "https://www.duckdns.org/update";
    manager = new QNetworkAccessManager();
    QObject::connect(manager, &QNetworkAccessManager::finished, this, &DuckdnsCore::onFinished);
    // QObject::connect(m_reply, &QNetworkReply::errorOccurred, this, &DuckdnsCore::onErrorOccurred);

    sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);

    // timer
    //  timer = new QTimer(this);
    //  connect(timer, &QTimer::timeout, this, &DuckdnsCore::onTimeout);
    //  timer->start(5*1000); // 10 sec
    // TODO: check public ip change every 10min?
    setupDailyTimer();
    update();
}

void DuckdnsCore::update()
{
    if (m_reply && m_reply->isRunning())
    {
        qWarning() << "Previous request still running, skip.";
        return;
    }
    // update
    response_ip = "";
    QUrlQuery query;
    query.addQueryItem("domains", m_domains);
    query.addQueryItem("token", m_token);
    query.addQueryItem("ip", "");
    QUrl url(m_url);
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setSslConfiguration(sslConfig);

    m_reply = manager->get(request);
}
void DuckdnsCore::nslookup(QString domainname)
{
    // Create a DNS lookup.
    m_dns = new QDnsLookup(this);
    connect(m_dns, &QDnsLookup::finished, this, &DuckdnsCore::handleDNSServers);

    // m_dns->setType(QDnsLookup::SRV);
    m_dns->setType(QDnsLookup::A);
    // qDebug() << "nslookup: " << domainname;
    m_dns->setName(domainname);
    // m_dns->setNameserver("8.8.8.8");
    m_dns->lookup();
}
void DuckdnsCore::handleDNSServers()
{
    // Check the lookup succeeded.
    if (m_dns->error() != QDnsLookup::NoError)
    {
        QString t = "DNS lookup failed: " + m_dns->errorString();
        qWarning() << t;
        m_dns->deleteLater();
        return;
    }

    // Handle the results. // QDnsLookup::SRV
    // const auto records = m_dns->serviceRecords();
    // for (const QDnsServiceRecord &record : records)
    // {
    //     qDebug() << "QDnsServiceRecord: " << record.name();
    // }
    // 讀取 A 紀錄（IPv4 位址）
    const auto records = m_dns->hostAddressRecords();
    for (const QDnsHostAddressRecord &record : records)
    {
        response_ip = record.value().toString();
        qDebug() << "found IP address:" << response_ip;
    }
    QFile logFile(m_logfile);
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        QTextStream out(&logFile);
        out << response_time << "\t" << response << "\t" << response_ip << "\r\n";
    }
    m_dns->deleteLater();
}

void DuckdnsCore::onErrorOccurred(QNetworkReply::NetworkError error)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;

    qWarning() << "Error:" << error << " " << reply->errorString();
}

void DuckdnsCore::onTimeout()
{
    // qDebug() << "onTimeout update";
    update();
}

void DuckdnsCore::setupDailyTimer()
{
    QTime targetTime(1, 0, 0); // 01:00:00 TODO: multi time?

    m_dailyTimer = new QTimer(this);
    m_dailyTimer->setSingleShot(true);

    connect(m_dailyTimer, &QTimer::timeout, this, [this, targetTime]()
            {
        // 執行目標函式
        update();
        // 重新設定，等下一個 24 小時
        m_dailyTimer->start(msecToNextTarget(targetTime)); });

    // 啟動，等到今天或明天的 01:00
    m_dailyTimer->start(msecToNextTarget(targetTime));

    qDebug() << "Next update at:" << QDateTime::currentDateTime().addMSecs(msecToNextTarget(targetTime));
}

qint64 DuckdnsCore::msecToNextTarget(QTime target)
{
    QDateTime now = QDateTime::currentDateTime();
    QDateTime next = QDateTime(now.date(), target);

    if (next <= now)
        next = next.addDays(1); // 今天已過，等明天

    return now.msecsTo(next);
}

void DuckdnsCore::onFinished(QNetworkReply *reply)
{
    // QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
    {
        qDebug() << "reply not exist";
        return;
    }
    QDateTime d = QDateTime::currentDateTime();
    response_time = d.toString("yyyy/MM/dd_hh:mm:ss");
    if (reply->error() == QNetworkReply::NoError)
    {
        response = reply->readAll(); // 先存
        nslookup(m_domains + ".duckdns.org");
    }
    else
    {
        QFile logFile(m_logfile);
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Append))
        {
            QTextStream out(&logFile);
            out << response_time << "\t Update Fail (" << reply->error() << ":" << reply->errorString() << ")\r\n";
        }
    }

    reply->deleteLater();
    m_reply = nullptr; // 清空，允許下次呼叫
}
