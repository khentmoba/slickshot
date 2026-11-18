// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "configwindow.h"
#include "config/configresolver.h"
#include "config/filenameeditor.h"
#include "config/generalconf.h"
#include "config/shortcutswidget.h"
#include "config/visualseditor.h"
#include "utils/colorutils.h"
#include "utils/confighandler.h"
#include "utils/globalvalues.h"
#include "utils/pathinfo.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFileSystemWatcher>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTabBar>
#include <QTextStream>
#include <QVBoxLayout>

#if defined(Q_OS_WIN)
#include <QTimer>
#include <QWindow>
#include <windows.h>
#endif

// Scroll area for tab content that cannot scroll by itself. It takes the size
// hints and size policy of its content, so the window is sized exactly as
// without it, until fitToScreen() enables scrolling because the window does
// not fit on the screen. Then it reports QScrollArea's own minimum size.
class ConfigWindow::TabScrollArea : public QScrollArea
{
public:
    explicit TabScrollArea(QWidget* content)
    {
        setWidget(content);
        setWidgetResizable(true);
        setFrameShape(QFrame::NoFrame);
        setFocusPolicy(Qt::NoFocus);
        setSizePolicy(content->sizePolicy());
        // Keep the tab pane background, like unwrapped content
        viewport()->setAutoFillBackground(false);
        content->setAutoFillBackground(false);
    }

    void enableScrolling()
    {
        m_scrolling = true;
        updateGeometry();
    }

    QSize sizeHint() const override { return widget()->sizeHint(); }
    QSize minimumSizeHint() const override
    {
        return m_scrolling ? QScrollArea::minimumSizeHint()
                           : widget()->minimumSizeHint();
    }

protected:
    bool event(QEvent* e) override
    {
        // QScrollArea does not propagate size changes of its content, so the
        // window would no longer grow with it
        if (e->type() == QEvent::LayoutRequest) {
            updateGeometry();
        }
        return QScrollArea::event(e);
    }

private:
    bool m_scrolling = false;
};

// ConfigWindow contains the menus where you can configure the application

ConfigWindow::ConfigWindow(QWidget* parent)
  : QWidget(parent)
{
    // We wrap QTabWidget in a QWidget because of a Qt bug
    auto* layout = new QVBoxLayout(this);
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->tabBar()->setUsesScrollButtons(false);

    layout->addWidget(m_tabWidget);

    setAttribute(Qt::WA_DeleteOnClose);
    setWindowIcon(QIcon(GlobalValues::iconPath()));
    setWindowTitle(tr("Configuration"));

    connect(ConfigHandler::getInstance(),
            &ConfigHandler::fileChanged,
            this,
            &ConfigWindow::updateChildren);

    // general
    m_generalConfig = new GeneralConf();
    m_generalConfigTab = new QWidget();
    auto* generalConfigLayout = new QVBoxLayout(m_generalConfigTab);
    m_generalConfigTab->setLayout(generalConfigLayout);
    generalConfigLayout->addWidget(m_generalConfig);
    m_tabWidget->addTab(m_generalConfigTab, tr("General"));

    // visuals
    m_visuals = new VisualsEditor();
    m_visualsTab = new QWidget();
    auto* visualsLayout = new QVBoxLayout(m_visualsTab);
    m_visualsTab->setLayout(visualsLayout);
    m_scrollAreas.append(new TabScrollArea(m_visuals));
    visualsLayout->addWidget(m_scrollAreas.last());
    m_tabWidget->addTab(m_visualsTab, tr("Interface"));

    // filename
    m_filenameEditor = new FileNameEditor();
    m_filenameEditorTab = new QWidget();
    auto* filenameEditorLayout = new QVBoxLayout(m_filenameEditorTab);
    m_filenameEditorTab->setLayout(filenameEditorLayout);
    m_scrollAreas.append(new TabScrollArea(m_filenameEditor));
    filenameEditorLayout->addWidget(m_scrollAreas.last());
    m_tabWidget->addTab(m_filenameEditorTab, tr("Filename Editor"));

    // shortcuts
    m_shortcuts = new ShortcutsWidget();
    m_shortcutsTab = new QWidget();
    auto* shortcutsLayout = new QVBoxLayout(m_shortcutsTab);
    m_shortcutsTab->setLayout(shortcutsLayout);
    shortcutsLayout->addWidget(m_shortcuts);
    m_tabWidget->addTab(m_shortcutsTab, tr("Shortcuts"));

    // Scrolling the settings page must not silently change a value when the
    // wheel happens to be over a combo box or spin box that is not focused.
    qApp->installEventFilter(this);

    applyTheme();

    // connect update sigslots
    connect(this,
            &ConfigWindow::updateChildren,
            m_filenameEditor,
            &FileNameEditor::updateComponents);
    connect(this,
            &ConfigWindow::updateChildren,
            m_visuals,
            &VisualsEditor::updateComponents);
    connect(this,
            &ConfigWindow::updateChildren,
            m_generalConfig,
            &GeneralConf::updateComponents);

    // Error indicator (this must come last)
    initErrorIndicator(m_visualsTab, m_visuals);
    initErrorIndicator(m_filenameEditorTab, m_filenameEditor);
    initErrorIndicator(m_generalConfigTab, m_generalConfig);
    initErrorIndicator(m_shortcutsTab, m_shortcuts);
}

ConfigWindow::~ConfigWindow()
{
    qApp->removeEventFilter(this);
}

void ConfigWindow::fitToScreen(const QRect& availableGeometry)
{
    // Must be called after show(), when the frame size is known
    const QSize frame = frameGeometry().size() - size();
    const QSize maxSize = availableGeometry.size() - frame;
    if (width() <= maxSize.width() && height() <= maxSize.height()) {
        return;
    }
    // The window is at least as large as the largest tab, which can exceed
    // small or High DPI screens. Let the tabs that cannot scroll by
    // themselves scroll, which lowers the layout's minimum size, and shrink
    // the window to fit.
    for (auto* scrollArea : m_scrollAreas) {
        scrollArea->enableScrolling();
    }
    // Apply the smaller minimum size now instead of on the next layout pass
    m_tabWidget->updateGeometry();
    layout()->activate();
    resize(size().boundedTo(maxSize));
}

#if defined(Q_OS_WIN)
bool ConfigWindow::event(QEvent* e)
{
    bool checkGeometry =
      e->type() == QEvent::Show || e->type() == QEvent::WindowStateChange;
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    checkGeometry |= e->type() == QEvent::DevicePixelRatioChange;
#endif
    const bool handled = QWidget::event(e);
    if (checkGeometry) {
        // Qt processes the native DPI resize after sending the change event.
        QTimer::singleShot(0, this, [this]() {
            if (!isVisible() || !windowHandle()) {
                return;
            }

            RECT client;
            if (!GetClientRect(reinterpret_cast<HWND>(windowHandle()->winId()),
                               &client)) {
                return;
            }
            const qreal dpr = windowHandle()->devicePixelRatio();
            const QSize clientSize(qRound((client.right - client.left) / dpr),
                                   qRound((client.bottom - client.top) / dpr));
            const bool normalWindow =
              !(windowState() & (Qt::WindowMinimized | Qt::WindowMaximized |
                                 Qt::WindowFullScreen));
            if (normalWindow && size() != clientSize) {
                resize(clientSize);
            }
            if (m_tabWidget->geometry() != layout()->contentsRect()) {
                layout()->invalidate();
                layout()->activate();
            }
        });
    }
    return handled;
}
#endif

bool ConfigWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::Wheel) {
        auto* widget = qobject_cast<QWidget*>(obj);
        const bool isValueWidget =
          qobject_cast<QComboBox*>(obj) != nullptr ||
          qobject_cast<QAbstractSpinBox*>(obj) != nullptr;
        if (widget != nullptr && isValueWidget && !widget->hasFocus() &&
            isAncestorOf(widget)) {
            // Scrolling the page must not change an unfocused value widget.
            event->ignore();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void ConfigWindow::applyTheme()
{
    const QColor background = palette().window().color();
    const bool isDark = ColorUtils::colorIsDark(background);

    const QString paneBorder =
      isDark ? "rgba(255, 255, 255, 0.12)" : "rgba(0, 0, 0, 0.12)";
    const QString selectedBg =
      isDark ? "rgba(56, 189, 248, 0.15)" : "rgba(2, 132, 199, 0.10)";
    const QString selectedColor = isDark ? "#38bdf8" : "#0284c7";
    const QString hoverBg =
      isDark ? "rgba(255, 255, 255, 0.06)" : "rgba(0, 0, 0, 0.05)";
#if defined(Q_OS_MACOS)
    // Fix Qt6 macOS bug where tab pane content renders behind the tab bar
    const QString macTopBorder = "border-top: 2px solid palette(mid); ";
#else
    const QString macTopBorder = "";
#endif

    m_tabWidget->setStyleSheet(
      QStringLiteral(
        "QTabWidget::pane { border: 1px solid %1; %5border-radius: 8px; }"
        "QTabBar::tab { padding: 8px 16px; margin: 2px; border-radius: 6px; "
        "border-bottom: 2px solid transparent; font-weight: 500; }"
        "QTabBar::tab:selected { background: %2; color: %3; border-bottom: 2px "
        "solid %3; }"
        "QTabBar::tab:!selected:hover { background: %4; }")
        .arg(paneBorder, selectedBg, selectedColor, hoverBg, macTopBorder));

    const QString modifier =
      isDark ? PathInfo::whiteIconPath() : PathInfo::blackIconPath();
    m_tabWidget->setTabIcon(0, QIcon(modifier + "config.svg"));
    m_tabWidget->setTabIcon(1, QIcon(modifier + "graphics.svg"));
    m_tabWidget->setTabIcon(2, QIcon(modifier + "name_edition.svg"));
    m_tabWidget->setTabIcon(3, QIcon(modifier + "shortcut.svg"));
}

void ConfigWindow::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange ||
        event->type() == QEvent::ThemeChange) {
        applyTheme();
    }
}

void ConfigWindow::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) {
        close();
    }
}

void ConfigWindow::initErrorIndicator(QWidget* tab, QWidget* widget)
{
    auto* label = new QLabel(tab);
    auto* btnResolve = new QPushButton(tr("Resolve"), tab);
    auto* btnLayout = new QHBoxLayout();

    // Set up label
    label->setText(tr(
      "<b>Configuration file has errors. Resolve them before continuing.</b>"));
    label->setStyleSheet(QStringLiteral(":disabled { color: %1; }")
                           .arg(qApp->palette().color(QPalette::Text).name()));
    label->setVisible(ConfigHandler().hasError());

    // Set up "Show errors" button
    btnResolve->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    btnLayout->addWidget(btnResolve);
    btnResolve->setVisible(ConfigHandler().hasError());

    widget->setEnabled(!ConfigHandler().hasError());

    // Add label and button to the parent widget's layout
    auto* layout = static_cast<QBoxLayout*>(tab->layout());
    if (layout != nullptr) {
        layout->insertWidget(0, label);
        layout->insertLayout(1, btnLayout);
    } else {
        widget->layout()->addWidget(label);
        widget->layout()->addWidget(btnResolve);
    }

    // Sigslots
    connect(
      ConfigHandler::getInstance(), &ConfigHandler::error, widget, [=, this]() {
          widget->setEnabled(false);
          label->show();
          btnResolve->show();
      });
    connect(ConfigHandler::getInstance(),
            &ConfigHandler::errorResolved,
            widget,
            [=]() {
                widget->setEnabled(true);
                label->hide();
                btnResolve->hide();
            });
    connect(btnResolve, &QPushButton::clicked, this, [this]() {
        ConfigResolver().exec();
    });
}
