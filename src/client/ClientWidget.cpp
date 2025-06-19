// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "ClientWidget.h"

#include "../configuration/configuration.h"
#include "ClientTelnet.h"
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
    QObject::connect(ui.playButton, &QAbstractButton::clicked, this, [this]() {
        getUi().parent->setCurrentIndex(1);
        getTelnet().connectToHost();
    });

    ui.input->installEventFilter(this);
    ui.input->setFocus();
    ui.display->setFocusPolicy(Qt::TabFocus);

    // Initialize m_configLifetime and register callback
    m_configLifetime.disconnectAll();
    Configuration &config = getConfig();
    config.integratedClient.registerChangeCallback(m_configLifetime, [this]() {
        this->handleClientSettingsUpdate();
    });
}

ClientWidget::~ClientWidget()
{
    m_configLifetime.disconnectAll(); // Ensure disconnection
    // Default destructor will handle unique_ptrs in m_pipeline
}

ClientWidget::Pipeline::~Pipeline()
{
    objs.clientTelnet.reset();
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

    initClientTelnet();
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
        NODISCARD ClientTelnet &getTelnet() { return getSelf().getTelnet(); }
        NODISCARD DisplayWidget &getDisplay() { return getSelf().getDisplay(); }
        NODISCARD PreviewWidget &getPreview() { return getSelf().getPreview(); }

    private:
        void virt_sendUserInput(const QString &msg) final { getTelnet().sendToMud(msg); }

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
        NODISCARD ClientTelnet &getTelnet() { return getSelf().getTelnet(); }

    private:
        void virt_showMessage(const QString &msg, int /*timeout*/) final
        {
            getSelf().slot_onShowMessage(msg);
        }
        void virt_windowSizeChanged(const int width, const int height) final
        {
            getTelnet().onWindowSizeChanged(width, height);
        }
        void virt_returnFocusToInput() final { getSelf().getInput().setFocus(); }
        void virt_showPreview(bool visible) final { getSelf().getPreview().setVisible(visible); }
    };
    auto &out = m_pipeline.outputs.displayWidgetOutputs;
    out = std::make_unique<LocalDisplayWidgetOutputs>(*this);
    getDisplay().init(deref(out));
}

void ClientWidget::initClientTelnet()
{
    class NODISCARD LocalClientTelnetOutputs final : public ClientTelnetOutputs
    {
    private:
        ClientWidget &m_self;

    public:
        explicit LocalClientTelnetOutputs(ClientWidget &self)
            : m_self{self}
        {}

    private:
        ClientWidget &getClient() { return m_self; }
        DisplayWidget &getDisplay() { return getClient().getDisplay(); }
        PreviewWidget &getPreview() { return getClient().getPreview(); }
        StackedInputWidget &getInput() { return getClient().getInput(); }

    private:
        void virt_connected() final
        {
            getClient().relayMessage("Connected using the integrated client");
            // Focus should be on the input
            getInput().setFocus();
        }
        void virt_disconnected() final
        {
            getDisplay().slot_displayText("\n\n\n");
            getClient().relayMessage("Disconnected using the integrated client");
        }
        void virt_socketError(const QString &errorStr) final
        {
            getDisplay().slot_displayText(QString("\nInternal error! %1\n").arg(errorStr));
        }
        void virt_echoModeChanged(const bool echo) final
        {
            getInput().setEchoMode(echo ? EchoModeEnum::Visible : EchoModeEnum::Hidden);
        }

        void virt_sendToUser(const QString &str) final
        {
            getDisplay().slot_displayText(str);
            getPreview().displayText(str);

            // Re-open the password dialog if we get a message in hidden echo mode
            if (getClient().getInput().getEchoMode() == EchoModeEnum::Hidden) {
                getClient().getInput().requestPassword();
            }
        }
    };
    auto &out = m_pipeline.outputs.clientTelnetOutputs;
    out = std::make_unique<LocalClientTelnetOutputs>(*this);
    m_pipeline.objs.clientTelnet = std::make_unique<ClientTelnet>(deref(out));
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

ClientTelnet &ClientWidget::getTelnet() // NOLINT (no, this shouldn't be const)
{
    return deref(m_pipeline.objs.clientTelnet);
}

void ClientWidget::slot_onVisibilityChanged(const bool /*visible*/)
{
    if (!isUsingClient()) {
        return;
    }

    // Delay connecting to verify that visibility is not just the dock popping back in
    QTimer::singleShot(500, [this]() {
        if (!isVisible()) {
            // Disconnect if the widget is closed or minimized
            getTelnet().disconnectFromHost();

        } else {
            // Connect if the client was previously activated and the widget is re-opened
            getTelnet().connectToHost();
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

void ClientWidget::handleClientSettingsUpdate()
{
    const auto& clientSettings = getConfig().integratedClient;

    // Update DisplayWidget
    // These methods are assumed to exist or would need to be added to DisplayWidget.
    // getDisplay().updateFontAndColorSettings(clientSettings.getFont(),
    //                                        clientSettings.getForegroundColor(),
    //                                        clientSettings.getBackgroundColor());
    // getDisplay().setScrollbackSize(clientSettings.getLinesOfScrollback());
    // getDisplay().updateTerminalSize(clientSettings.getColumns(), clientSettings.getRows());
    // For now, as a simpler approach if above methods are not yet implemented,
    // we can try to force a re-init or a less granular update if available.
    // If DisplayWidget itself listened, this would be cleaner.
    // As a placeholder for what can be done *without* modifying DisplayWidget's public API now:
    // Force re-evaluation of AnsiTextHelper defaults by recreating it or re-calling init.
    // This is not ideal as it's internal. A public refresh method is better.
    // For now, we'll rely on DisplayWidget's existing resizeEvent for some updates
    // and note that font/color/scrollback require DisplayWidget API changes.
    // TODO: Add public methods to DisplayWidget to allow dynamic update of Font, Colors, Scrollback.
    // Forcing a resize could trigger DisplayWidget::resizeEvent which re-calculates cols/rows
    // and calls windowSizeChanged, but it doesn't re-read all settings.
    // getDisplay().resize(getDisplay().size()); // This is a bit of a hack.


    // Update InputWidget
    // Similar assumptions/needs for InputWidget.
    // getInput().updateFontSetting(clientSettings.getFont()); // Assumed method
    // getInput().setHistorySize(clientSettings.getLinesOfInputHistory()); // Assumed (complex to implement)
    // Behavioral settings like clearInputOnEnter and tabCompletionDictionarySize are used internally
    // by InputWidget's methods and don't require explicit "refresh" calls on the widget itself.
    // TODO: Add public method to InputWidget to allow dynamic font update if needed.

    // Update PeekPreviewWidget
    // getPreview().setLines(clientSettings.getLinesOfPeekPreview()); // Assumed method
    // getPreview().updateFont(clientSettings.getFont()); // Assumed method
    // TODO: Add public methods to PreviewWidget for lines and font updates.

    // After potential updates to children that might affect their size or appearance:
    updateGeometry(); // If column/row changes affect ClientWidget's overall size hint via children
    update();         // Schedule a repaint for ClientWidget and children

    // The most robust way is for child widgets to listen to config changes themselves
    // or for ClientWidget to have a more formal contract with them via update methods.
    // The current implementation reflects a state where children are not fully dynamic post-construction
    // for all settings without specific new methods.
    // This function now primarily ensures that if ClientWidget itself used any of these settings
    // for its own geometry/painting (it mostly doesn't), it would refresh.
    // The main benefit of this callback in ClientWidget is for future enhancements
    // or if ClientWidget itself develops direct dependencies on these settings.
}
