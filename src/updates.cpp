// Check for updates: asks GitHub for the latest release and offers the right download.
#include "mainwindow.h"

#include <QAbstractButton>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QUrl>
#include <QVersionNumber>

// The release file for this system, matched by the end of its name; empty when there is none.
QString MainWindow::platformDownloadSuffix()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("-windows-x64.zip");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("-macos.zip");
#else
    return QString();
#endif
}

QString MainWindow::installSteps()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("When the download finishes, close light_md, unzip the file and replace the old "
                          "light_md folder with the new one. Your documents are not affected.");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("When the download finishes, close light_md, unzip the file and drag light_md "
                          "to your Applications folder, replacing the old one.");
#else
    return QStringLiteral("Install the new version the same way you installed this one.");
#endif
}

void MainWindow::checkForUpdates()
{
    if (m_updateReply)
        return;
    QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/josel120/light_md/releases/latest")));
    // GitHub rejects API requests without a User-Agent.
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("light_md/%1").arg(QStringLiteral(LIGHT_MD_VERSION)));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setTransferTimeout(15000);
    m_status->setText(QStringLiteral("Checking for updates..."));
    m_updateReply = m_network.get(request);
    connect(m_updateReply, &QNetworkReply::finished, this, [this] { showUpdateResult(); });
}

void MainWindow::showUpdateResult()
{
    QNetworkReply *reply = m_updateReply;
    m_updateReply = nullptr;
    reply->deleteLater();
    updateStatus();
    const QString title = QStringLiteral("Check for updates");
    if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::warning(this, title,
                             QStringLiteral("Could not check for updates. Please check your internet "
                                            "connection and try again.\n\n(%1)")
                                 .arg(reply->errorString()));
        return;
    }
    const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
    QString tag = release.value(QStringLiteral("tag_name")).toString();
    if (tag.startsWith(QLatin1Char('v')))
        tag.remove(0, 1);
    const QVersionNumber latest = QVersionNumber::fromString(tag);
    const QVersionNumber current = QVersionNumber::fromString(QStringLiteral(LIGHT_MD_VERSION));
    if (latest.isNull()) {
        QMessageBox::warning(this, title, QStringLiteral("Could not read the latest version from GitHub."));
        return;
    }
    if (QVersionNumber::compare(latest, current) <= 0) {
        QMessageBox::information(this, title,
                                 QStringLiteral("You have the latest version of light_md (%1).")
                                     .arg(current.toString()));
        return;
    }

    const QUrl releasePage(release.value(QStringLiteral("html_url")).toString());
    QUrl download = releasePage;
    const QString suffix = platformDownloadSuffix();
    const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue &asset : assets) {
        const QJsonObject file = asset.toObject();
        if (!suffix.isEmpty() && file.value(QStringLiteral("name")).toString().endsWith(suffix)) {
            download = QUrl(file.value(QStringLiteral("browser_download_url")).toString());
            break;
        }
    }

    QMessageBox box(QMessageBox::Information, QStringLiteral("Update available"),
                    QStringLiteral("light_md %1 is available. You have version %2.")
                        .arg(latest.toString(), current.toString()),
                    QMessageBox::NoButton, this);
    box.setInformativeText(installSteps());
    QPushButton *btnDownload = box.addButton(QStringLiteral("Download"), QMessageBox::AcceptRole);
    QPushButton *btnNews = box.addButton(QStringLiteral("What's new"), QMessageBox::HelpRole);
    box.addButton(QStringLiteral("Not now"), QMessageBox::RejectRole);
    box.setDefaultButton(btnDownload);
    box.exec();
    if (box.clickedButton() == btnDownload)
        QDesktopServices::openUrl(download);
    else if (box.clickedButton() == btnNews)
        QDesktopServices::openUrl(releasePage);
}
