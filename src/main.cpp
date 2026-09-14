#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>

#include "duckdnscore.h"
#include "version.h"
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages)
    {
        const QString baseName = "qduckdns_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName))
        {
            a.installTranslator(&translator);
            break;
        }
    }
    //
    QStringList args = QCoreApplication::arguments();

    if (args.size() > 1)
    {
        if (args.value(1) == "-h" || args.value(1) == "--help")
        {
            qDebug() << "v" << QDuckDNS_VERSION;
            qDebug() << "Usage:";
            qDebug() << "\tstatus: systemctl status qduckdns";
            qDebug() << "\tstart: sudo systemctl start qduckdns";
            qDebug() << "\tstop: sudo systemctl stop qduckdns";
            return -1;
        }
    }
    // 1. 獲取可寫入的設定檔根目錄
#ifdef Q_OS_LINUX
    // QStandardPaths::ConfigLocation => linux will be in <home>/.config/...
    QString baseConfigPath = "/etc";
#else
    QString baseConfigPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
#endif
    QString myConfig = QDir::cleanPath(baseConfigPath + "/qduckdns/qduckdns.cfg");
    if (!QFile::exists(myConfig))
    {
        // 1. 取得檔案的上層目錄路徑
        QFileInfo fileInfo(myConfig);
        QDir dir = fileInfo.dir();
        // 2. 如果上層資料夾不存在，先建立資料夾 (例如建立 /qduckdns)
        if (!dir.exists())
        {
            dir.mkpath("."); // mkpath 會連同所有不存在的父目錄一起建立
        }
    }
    QSettings cfg = QSettings(myConfig, QSettings::NativeFormat);
    cfg.beginGroup("main");
    QString domains = cfg.value("domains", "").toString();
    QString token = cfg.value("token", "").toString();
    cfg.endGroup();
    if (domains.isEmpty() || token.isEmpty())
    {
        qDebug() << "check config file:" << myConfig << ", Domains or token is empty";
        return -1;
    }
    DuckdnsCore ddns(domains, token);

    return QCoreApplication::exec();
}
