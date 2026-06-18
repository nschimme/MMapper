// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "UpdateDialog.h"

#include "../configuration/configuration.h"
#include "../global/ConfigConsts-Computed.h"
#include "../global/RAII.h"
#include "../global/Version.h"

#include <algorithm>
#include <array>
#include <utility>

#include <QDesktopServices>
#include <QGridLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QPushButton>
#include <QRegularExpression>
#include <QSysInfo>

namespace { // anonymous

NODISCARD const char *getArchitectureRegexPattern()
{
    // See Qt documentation for expected keys
    static const std::array<std::pair<const char *, const char *>, 4> archPatterns = {
        {{"arm64", "(arm64|aarch64)"},
         {"x86_64", "(x86_64|amd64|x64)"},
         {"i386", "(i386|x86(?!_64))"},
         {"arm", "(arm(?!64)|armhf)"}}};

    static auto findPattern = [](const QString &arch) -> const char * {
        const auto it = std::find_if(archPatterns.begin(),
                                     archPatterns.end(),
                                     [&arch](const auto &pair) {
                                         return mmqt::toStdStringUtf8(arch) == pair.first;
                                     });
        return (it != archPatterns.end()) ? it->second : nullptr;
    };

    if (auto pattern = findPattern(QSysInfo::currentCpuArchitecture())) {
        return pattern;
    }
    if (auto fallback = findPattern(QSysInfo::buildCpuArchitecture())) {
        return fallback;
    }
    abort();
}

} // namespace

UpdateDialog::UpdateDialog(QWidget *const parent)
    : QDialog(parent)
{
    setWindowTitle("MMapper Updater");
    setWindowIcon(QIcon(":/icons/mmapper-lo.svg"));

    m_text = new QLabel(this);
    m_text->setWordWrap(true);
    m_text->setTextFormat(Qt::RichText);
    m_text->setOpenExternalLinks(true);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Upgrade"));

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        if (!m_downloadUrl.isEmpty()) {
            QDesktopServices::openUrl(QUrl(m_downloadUrl));
        }
        accept();
    });
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &UpdateDialog::reject);

    QGridLayout *mainLayout = new QGridLayout(this);
    mainLayout->addWidget(m_text);
    mainLayout->addWidget(m_buttonBox);

    connect(&m_manager, &QNetworkAccessManager::finished, this, &UpdateDialog::managerFinished);
}

void UpdateDialog::open()
{
    m_text->setText(tr("Checking for new version..."));
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    auto url = isMMapperBeta()
                   ? QStringLiteral("https://api.github.com/repos/mume/mmapper/git/ref/tags/beta")
                   : QStringLiteral("https://api.github.com/repos/mume/mmapper/releases/latest");
    QNetworkRequest request(url);
    // request.setRawHeader("User-Agent", "MMapper");
    m_manager.get(request);

    QDialog::open();
}

void UpdateDialog::setUpdateStatus(const QString &message,
                                   const bool enableUpgradeButton,
                                   const bool showAndUpdateDialog)
{
    m_text->setText(message);
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(enableUpgradeButton);
    if (showAndUpdateDialog) {
        open();
    }
}

QString UpdateDialog::findDownloadUrlForRelease(const QJsonObject &releaseObject) const
{
    static const auto archRegex = QRegularExpression(getArchitectureRegexPattern(),
                                                     QRegularExpression::CaseInsensitiveOption);
    const QJsonArray assets = releaseObject["assets"].toArray();
    for (const auto &assetValue : assets) {
        const QJsonObject asset = assetValue.toObject();
        const QString name = asset["name"].toString();

#ifdef Q_OS_WIN
        if (name.endsWith(".exe") && name.contains(archRegex)) {
            return asset["browser_download_url"].toString();
        }
#elif defined(Q_OS_MACOS)
        if (name.endsWith(".dmg") && name.contains(archRegex)) {
            return asset["browser_download_url"].toString();
        }
#elif defined(Q_OS_LINUX)
        if (name.endsWith(".deb") && name.contains(archRegex)) {
            return asset["browser_download_url"].toString();
        }
#endif
    }
    return {};
}

void UpdateDialog::managerFinished(QNetworkReply *const reply)
{
    const RAIICallback deleteLaterRAII{[&reply]() { reply->deleteLater(); }};

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << reply->errorString();
        setUpdateStatus(tr("Failed to check for updates: %1").arg(reply->errorString()),
                        false,
                        false);
        return;
    }

    const QString answer = reply->readAll();
    const QJsonDocument jsonResponse = QJsonDocument::fromJson(answer.toUtf8());

    if (jsonResponse.isNull()) {
        qWarning() << "Failed to parse update JSON";
        setUpdateStatus(tr("Failed to parse update information."), false, false);
        return;
    }

    const QJsonObject obj = jsonResponse.object();

    if (isMMapperBeta() && reply->request().url().toString().contains("/ref/tags/")) {
        // beta uses git ref api
        const QJsonObject object = obj["object"].toObject();
        const QString latestSha = object["sha"].toString();
        const QString currentVersion = QString::fromUtf8(getMMapperVersion());
        static const QRegularExpression shaRx(R"(-g([0-9a-f]{7,}))");
        auto result = shaRx.match(currentVersion);
        if (result.hasMatch()) {
            const QString currentSha = result.captured(1);
            if (!latestSha.startsWith(currentSha)) {
                setUpdateStatus(tr("A new beta version is available! "
                                   "<a href=\"https://github.com/mume/mmapper/actions\">"
                                   "Download from GitHub Actions</a>"),
                                false,
                                true);
            }
        }
        return;
    }

    const QString latestTag = obj["tag_name"].toString();
    const CompareVersion latest(latestTag);

    const QString currentVersion = QString::fromUtf8(getMMapperVersion());
    static CompareVersion current(currentVersion);

    if (latest > current) {
        m_downloadUrl = findDownloadUrlForRelease(obj);
        if (m_downloadUrl.isEmpty()) {
            m_downloadUrl = obj["html_url"].toString();
        }

        setUpdateStatus(tr("A new version (%1) is available!").arg(latestTag), true, true);
    } else {
        setUpdateStatus(tr("MMapper is up to date."), false, false);
    }
}
