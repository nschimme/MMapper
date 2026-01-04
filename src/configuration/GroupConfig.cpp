#include "GroupConfig.h"
#include "configuration.h"

GroupConfig::GroupConfig(const QString &group, QObject *parent)
    : QObject(parent), m_group(group) {
    auto registration = getConfig().registerChangeCallback([this]() { this->handleChange(); });
    this->setParent(registration.get());
}

void GroupConfig::set(std::function<void(QSettings &)> callback) {
    m_callback = callback;
    QSettings settings;
    settings.beginGroup(m_group);
    m_callback(settings);
}

QSettings *GroupConfig::get() {
    auto *settings = new QSettings();
    settings->beginGroup(m_group);
    return settings;
}

void GroupConfig::handleChange() {
    if (m_callback) {
        QSettings settings;
        settings.beginGroup(m_group);
        m_callback(settings);
    }
}
