#include "duckdnscore.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDnsHostAddressRecord>
#include <QFile>
#include <QHostAddress>
#include <QTime>
#include <QTimer>
#include <QUrlQuery>

DuckdnsCore::DuckdnsCore(QString domains, QString token, QObject *parent)
    : QObject{parent}, m_domains(domains), m_token(token) {
  //<domains>.duckdns.org

  if (!QDir().mkpath("/tmp/qduckdns")) {
    qWarning() << "Failed to create /tmp/qduckdns";
    // return 1;
  }
  m_logfile = "/tmp/qduckdns/qduckdns.log";

  m_url = "https://www.duckdns.org/update";
  manager = new QNetworkAccessManager(this);
  QObject::connect(manager, &QNetworkAccessManager::finished, this,
                   &DuckdnsCore::onFinished);

  sslConfig = QSslConfiguration::defaultConfiguration();
  sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);

  m_retryTimer = new QTimer(this);
  m_retryTimer->setSingleShot(true);
  connect(m_retryTimer, &QTimer::timeout, this, &DuckdnsCore::update);

  // timer
  //  timer = new QTimer(this);
  //  connect(timer, &QTimer::timeout, this, &DuckdnsCore::onTimeout);
  //  timer->start(5*1000); // 10 sec
  // TODO: check public ip change every 10min?
  setupDailyTimer();
  update();
}

void DuckdnsCore::update() {
  if (m_reply) {
    if (m_reply->isRunning()) {
      qWarning() << "Previous request still running or timed out, aborting old "
                    "request.";
      m_reply->abort();
    }
    m_reply->deleteLater();
    m_reply = nullptr;
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
  request.setTransferTimeout(15000); // 15 秒超時 (Qt 5.15+ / Qt 6)

  m_reply = manager->get(request);
}
void DuckdnsCore::nslookup(QString domainname) {
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
void DuckdnsCore::handleDNSServers() {
  // Check the lookup succeeded.
  if (m_dns->error() != QDnsLookup::NoError) {
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
  for (const QDnsHostAddressRecord &record : records) {
    response_ip = record.value().toString();
    qDebug() << "found IP address:" << response_ip;
  }
  QFile logFile(m_logfile);
  if (logFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
    QTextStream out(&logFile);
    out << response_time << "\t" << response << "\t" << response_ip << "\r\n";
  }
  m_dns->deleteLater();
}

void DuckdnsCore::onTimeout() {
  // qDebug() << "onTimeout update";
  update();
}

void DuckdnsCore::setupDailyTimer() {
  QTime targetTime(1, 0, 0); // 01:00:00 TODO: multi time?

  m_dailyTimer = new QTimer(this);
  m_dailyTimer->setSingleShot(true);

  connect(m_dailyTimer, &QTimer::timeout, this, [this, targetTime]() {
    m_retryCount = 0; // 重置重試計數
    if (m_retryTimer && m_retryTimer->isActive()) {
      m_retryTimer->stop();
    }
    // 執行目標函式
    update();
    // 重新設定，等下一個 24 小時
    m_dailyTimer->start(msecToNextTarget(targetTime));
  });

  // 啟動，等到今天或明天的 01:00
  m_dailyTimer->start(msecToNextTarget(targetTime));

  qDebug() << "Next update at:"
           << QDateTime::currentDateTime().addMSecs(
                  msecToNextTarget(targetTime));
}

qint64 DuckdnsCore::msecToNextTarget(QTime target) {
  QDateTime now = QDateTime::currentDateTime();
  QDateTime next = QDateTime(now.date(), target);

  if (next <= now)
    next = next.addDays(1); // 今天已過，等明天

  return now.msecsTo(next);
}

void DuckdnsCore::onFinished(QNetworkReply *reply) {
  // QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
  if (!reply) {
    qDebug() << "reply not exist";
    return;
  }
  QDateTime d = QDateTime::currentDateTime();
  response_time = d.toString("yyyy/MM/dd_hh:mm:ss");
  if (reply->error() == QNetworkReply::NoError) {
    m_retryCount = 0; // 成功更新，重置重試計數
    if (m_retryTimer && m_retryTimer->isActive()) {
      m_retryTimer->stop();
    }
    response = reply->readAll(); // 先存
    nslookup(m_domains + ".duckdns.org");
  } else {
    QString s = QString("%1\t Update Fail (%2:%3)")
                    .arg(response_time)
                    .arg(static_cast<int>(reply->error()))
                    .arg(reply->errorString());
    qDebug() << s;

    QString retryMsg;
    if (reply->error() == QNetworkReply::TimeoutError) {
      if (m_retryCount < MAX_RETRIES) {
        m_retryCount++;
        retryMsg = QString("%1\t Update timed out. Scheduling retry %2/%3 in 3 minutes...")
                       .arg(response_time)
                       .arg(m_retryCount)
                       .arg(MAX_RETRIES);
        qWarning() << retryMsg;
        if (m_retryTimer) {
          m_retryTimer->start(3 * 60 * 1000); // 3 分鐘後再試
        }
      } else {
        retryMsg = QString("%1\t Update timed out. Reached maximum retries (%2). Waiting for next schedule.")
                       .arg(response_time)
                       .arg(MAX_RETRIES);
        qWarning() << retryMsg;
        m_retryCount = 0;
      }
    }

    QFile logFile(m_logfile);
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
      QTextStream out(&logFile);
      out << s << "\r\n";
      if (!retryMsg.isEmpty()) {
        out << retryMsg << "\r\n";
      }
    }
  }

  if (m_reply == reply) {
    m_reply = nullptr; // 清空，允許下次呼叫
  }
  reply->deleteLater();
}
