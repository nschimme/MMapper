// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "parserpage.h"

#include "../configuration/configuration.h"
#include "../global/Charset.h"
#include "../global/SignalBlocker.h"
#include "../parser/AbstractParser-Utils.h"
#include "AnsiColorDialog.h"
#include "ansicombo.h"

#include <QColor>
#include <QComboBox>
#include <QString>
#include <QValidator>
#include <QtWidgets>

class NODISCARD CommandPrefixValidator final : public QValidator
{
public:
    explicit CommandPrefixValidator(QObject *parent);
    ~CommandPrefixValidator() final;

    void fixup(QString &input) const override
    {
        mmqt::toLatin1InPlace(input); // transliterates non-latin1 codepoints
    }

    NODISCARD State validate(QString &input, int & /* pos */) const override
    {
        if (input.isEmpty()) {
            return State::Intermediate;
        }

        const bool valid = input.length() == 1 && isValidPrefix(mmqt::toLatin1(input[0]));
        return valid ? State::Acceptable : State::Invalid;
    }
};

CommandPrefixValidator::CommandPrefixValidator(QObject *const parent)
    : QValidator(parent)
{}

CommandPrefixValidator::~CommandPrefixValidator() = default;

ParserPage::ParserPage(QWidget *const parent)
    : QWidget(parent)
{
    setupUi(this);

    connect(roomNameColorPushButton,
            &QAbstractButton::clicked,
            this,
            &ParserPage::slot_roomNameColorClicked);
    connect(roomDescColorPushButton,
            &QAbstractButton::clicked,
            this,
            &ParserPage::slot_roomDescColorClicked);

    connect(charPrefixLineEdit, &QLineEdit::editingFinished, this, [this]() {
        if (!charPrefixLineEdit->text().isEmpty()) {
            setConfig().parser.prefixChar.set(mmqt::toLatin1(charPrefixLineEdit->text().front()));
        }
    });

    connect(encodeEmoji, &QCheckBox::clicked, this, [](bool checked) {
        setConfig().parser.encodeEmoji.set(checked);
    });
    connect(decodeEmoji, &QCheckBox::clicked, this, [](bool checked) {
        setConfig().parser.decodeEmoji.set(checked);
    });

    auto &parser = setConfig().parser;
    parser.roomNameColor.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    parser.roomDescColor.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    parser.prefixChar.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    parser.encodeEmoji.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    parser.decodeEmoji.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
}

void ParserPage::slot_loadConfig()
{
    const auto &settings = getConfig().parser;

    SignalBlocker b1(*charPrefixLineEdit);
    SignalBlocker b2(*encodeEmoji);
    SignalBlocker b3(*decodeEmoji);

    AnsiCombo::makeWidgetColoured(roomNameColorLabel, settings.roomNameColor.get());
    AnsiCombo::makeWidgetColoured(roomDescColorLabel, settings.roomDescColor.get());

    charPrefixLineEdit->setText(QString(static_cast<char>(settings.prefixChar.get())));
    if (charPrefixLineEdit->validator() == nullptr) {
        charPrefixLineEdit->setValidator(new CommandPrefixValidator(this));
    }

    encodeEmoji->setChecked(settings.encodeEmoji.get());
    decodeEmoji->setChecked(settings.decodeEmoji.get());
}

void ParserPage::slot_roomNameColorClicked()
{
    AnsiColorDialog::getColor(getConfig().parser.roomNameColor.get(), this, [this](QString ansiString) {
        AnsiCombo::makeWidgetColoured(roomNameColorLabel, ansiString);
        setConfig().parser.roomNameColor.set(ansiString);
    });
}

void ParserPage::slot_roomDescColorClicked()
{
    AnsiColorDialog::getColor(getConfig().parser.roomDescColor.get(), this, [this](QString ansiString) {
        AnsiCombo::makeWidgetColoured(roomDescColorLabel, ansiString);
        setConfig().parser.roomDescColor.set(ansiString);
    });
}
