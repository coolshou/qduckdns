#ifndef DUCKDNSCORE_H
#define DUCKDNSCORE_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslConfiguration>
#include <QTimer>
#include <QDnsLookup>

class DuckdnsCore : public QObject
{
    Q_OBJECT
public:
    explicit DuckdnsCore(QString domains,
                         QString token, QObject *parent = nullptr);
    void update();
    void nslookup(QString domainname);
    void handleDNSServers();
public slots:
    void onTimeout();
    void setupDailyTimer();
    qint64 msecToNextTarget(QTime target);
private slots:
    void onFinished(QNetworkReply *reply);
    void onErrorOccurred(QNetworkReply::NetworkError error);
signals:
private:
    QNetworkAccessManager *manager;
    QNetworkReply *m_reply = nullptr; // 需要保留 reply 指標
    QSslConfiguration sslConfig;
    QTimer *timer;
    QTimer *m_dailyTimer = nullptr;

    QString m_url;
    QString m_domains;
    QString m_token;
    QString m_logfile;
    QDnsLookup *m_dns;

    QString response_time;
    QByteArray response;
    QString response_ip;
};

#endif // DUCKDNSCORE_H
