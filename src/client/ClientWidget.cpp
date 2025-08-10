// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "ClientWidget.h"

#include "../configuration/configuration.h"
#include "../proxy/proxy.h"
#include "PreviewWidget.h"
#include "displaywidget.h"
#include "stackedinputwidget.h"
#include "ui_ClientWidget.h"

#include <memory>

#include <QFileDialog>
#include <QScrollBar>
#include <QString>
#include <QTimer>

ClientWidget::ClientWidget(QWidget *const parent)
    : QWidget(parent)
{
    setWindowTitle("MMapper Client");

    initPipeline();

    auto &ui = getUi();

    // Port
    ui.port->setText(QString("%1").arg(getConfig().connection.localPort));

    ui.playButton->setFocus();
    QObject::connect(ui.playButton, &QAbstractButton::clicked, this, &ClientWidget::playButtonClicked);
    QObject::connect(ui.playButton, &QAbstractButton::clicked, this, [this]() {
        getUi().parent->setCurrentIndex(1);
    });

    ui.input->installEventFilter(this);
    ui.display->setFocusPolicy(Qt::TabFocus);
}

ClientWidget::~ClientWidget() = default;

ClientWidget::Pipeline::~Pipeline()
{
    objs.ui.reset();
}

QSize ClientWidget::minimumSizeHint() const
{
    return m_pipeline.objs.ui->display->sizeHint();
}

void ClientWidget::initPipeline()
{
    m_pipeline.objs.ui = std::make_unique<Ui::ClientWidget>();
    getUi().setupUi(this); // creates stacked input and display

    initStackedInputWidget();
    initDisplayWidget();
}

void ClientWidget::initStackedInputWidget()
{
    class NODISCARD LocalStackedInputWidgetOutputs final : public StackedInputWidgetOutputs
    {
    private:
        ClientWidget &m_self;

    public:
        explicit LocalStackedInputWidgetOutputs(ClientWidget &self)
            : m_self{self}
        {}

    private:
        NODISCARD ClientWidget &getSelf() { return m_self; }
        NODISCARD Proxy &getProxy() { return deref(getSelf().m_proxy); }
        NODISCARD DisplayWidget &getDisplay() { return getSelf().getDisplay(); }
        NODISCARD PreviewWidget &getPreview() { return getSelf().getPreview(); }

    private:
        void virt_sendUserInput(const QString &msg) final { getProxy().sendFromClient(msg); }

        void virt_displayMessage(const QString &msg) final
        {
            getDisplay().slot_displayText(msg);
            getPreview().displayText(msg);
        }

        void virt_showMessage(const QString &msg, MAYBE_UNUSED int timeout) final
        {
            // REVISIT: Why is timeout ignored?
            getSelf().slot_onShowMessage(msg);
        }
        void virt_requestPassword() final { getSelf().getInput().requestPassword(); }
    };
    auto &out = m_pipeline.outputs.stackedInputWidgetOutputs;
    out = std::make_unique<LocalStackedInputWidgetOutputs>(*this);
    getInput().init(deref(out));
}

void ClientWidget::initDisplayWidget()
{
    class NODISCARD LocalDisplayWidgetOutputs final : public DisplayWidgetOutputs
    {
    private:
        ClientWidget &m_self;

    public:
        explicit LocalDisplayWidgetOutputs(ClientWidget &self)
            : m_self{self}
        {}

    private:
        NODISCARD ClientWidget &getSelf() { return m_self; }
        NODISCARD Proxy &getProxy() { return deref(getSelf().m_proxy); }

    private:
        void virt_showMessage(const QString &msg, int /*timeout*/) final
        {
            getSelf().slot_onShowMessage(msg);
        }
        void virt_windowSizeChanged(const int width, const int height) final
        {
            getProxy().windowSizeChanged(width, height);
        }
        void virt_returnFocusToInput() final { getSelf().getInput().setFocus(); }
        void virt_showPreview(bool visible) final { getSelf().getPreview().setVisible(visible); }
    };
    auto &out = m_pipeline.outputs.displayWidgetOutputs;
    out = std::make_unique<LocalDisplayWidgetOutputs>(*this);
    getDisplay().init(deref(out));
}

DisplayWidget &ClientWidget::getDisplay()
{
    return deref(getUi().display);
}

PreviewWidget &ClientWidget::getPreview()
{
    return deref(getUi().preview);
}

StackedInputWidget &ClientWidget::getInput()
{
    return deref(getUi().input);
}

void ClientWidget::setProxy(QPointer<Proxy> proxy)
{
    m_proxy = proxy;
}

void ClientWidget::displayText(const QString &text)
{
    getDisplay().slot_displayText(text);
    getPreview().displayText(text);
}

void ClientWidget::setEchoMode(EchoModeEnum echoMode)
{
    getInput().setEchoMode(echoMode);
}

void ClientWidget::setFocusOnInput()
{
    getInput().setFocus();
}

void ClientWidget::relayMessage(const QString &msg)
{
    emit sig_relayMessage(msg);
}

void ClientWidget::slot_onVisibilityChanged(const bool /*visible*/)
{
    if (!isUsingClient()) {
        return;
    }

    // Delay connecting to verify that visibility is not just the dock popping back in
    QTimer::singleShot(500, [this]() {
        if (!m_proxy) {
            return;
        }
        if (!isVisible()) {
            // Disconnect if the widget is closed or minimized
            m_proxy->disconnectFromMud();

        } else {
            // Connect if the client was previously activated and the widget is re-opened
            m_proxy->connectToMud();
        }
    });
}

bool ClientWidget::isUsingClient() const
{
    return getUi().parent->currentIndex() != 0;
}

void ClientWidget::slot_onShowMessage(const QString &message)
{
    relayMessage(message);
}

void ClientWidget::slot_saveLog()
{
    struct NODISCARD Result
    {
        QStringList filenames;
        bool isHtml = false;
    };
    const auto getFileNames = [this]() -> Result {
        auto save = std::make_unique<QFileDialog>(this, "Choose log file name ...");
        save->setFileMode(QFileDialog::AnyFile);
        save->setDirectory(QDir::current());
        save->setNameFilters(QStringList() << "Text log (*.log *.txt)"
                                           << "HTML log (*.htm *.html)");
        save->setDefaultSuffix("txt");
        save->setAcceptMode(QFileDialog::AcceptSave);

        if (save->exec() == QDialog::Accepted) {
            const QString nameFilter = save->selectedNameFilter().toLower();
            const bool isHtml = nameFilter.endsWith(".htm") || nameFilter.endsWith(".html");
            return Result{save->selectedFiles(), isHtml};
        }

        return Result{};
    };

    const auto result = getFileNames();
    const auto &fileNames = result.filenames;

    if (fileNames.isEmpty()) {
        relayMessage(tr("No filename provided"));
        return;
    }

    QFile document(fileNames[0]);
    if (!document.open(QFile::WriteOnly | QFile::Text)) {
        relayMessage(QString("Error occurred while opening %1").arg(document.fileName()));
        return;
    }

    const auto getDocStringUtf8 = [](const QTextDocument *const pDoc,
                                     const bool isHtml) -> QByteArray {
        auto &doc = deref(pDoc);
        const QString string = isHtml ? doc.toHtml() : doc.toPlainText();
        return string.toUtf8();
    };
    document.write(getDocStringUtf8(getDisplay().document(), result.isHtml));
    document.close();
}

bool ClientWidget::focusNextPrevChild(MAYBE_UNUSED bool next)
{
    if (getInput().hasFocus()) {
        getDisplay().setFocus();
    } else {
        getInput().setFocus();
    }
    return true;
}
