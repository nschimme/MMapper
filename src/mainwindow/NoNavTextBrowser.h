#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/UrlUtils.h"

#include <QTextBrowser>

class NoNavTextBrowser : public QTextBrowser
{
    Q_OBJECT
public:
    explicit NoNavTextBrowser(QWidget *parent = nullptr)
        : QTextBrowser(parent)
    {
        setOpenExternalLinks(false);
        connect(this, &QTextBrowser::anchorClicked, this, [](const QUrl &url) {
            mmqt::openUrl(url);
        });
    }

protected:
    void doSetSource(const QUrl &name,
                     QTextDocument::ResourceType type = QTextDocument::UnknownResource) override
    {}
};
