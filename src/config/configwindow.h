// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#pragma once

#include <QTabWidget>

class FileNameEditor;
class ShortcutsWidget;
class GeneralConf;
class QFileSystemWatcher;
class VisualsEditor;
class QWidget;

class ConfigWindow : public QWidget
{
    Q_OBJECT
public:
    explicit ConfigWindow(QWidget* parent = nullptr);
    ~ConfigWindow() override;

    void fitToScreen(const QRect& availableGeometry);

signals:
    void updateChildren();

protected:
#if defined(Q_OS_WIN)
    bool event(QEvent*) override;
#endif
    void keyPressEvent(QKeyEvent*) override;
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    class TabScrollArea;

    void applyTheme();
    QTabWidget* m_tabWidget;
    QList<TabScrollArea*> m_scrollAreas;

    FileNameEditor* m_filenameEditor;
    QWidget* m_filenameEditorTab;

    ShortcutsWidget* m_shortcuts;
    QWidget* m_shortcutsTab;

    GeneralConf* m_generalConfig;
    QWidget* m_generalConfigTab;

    VisualsEditor* m_visuals;
    QWidget* m_visualsTab;

    void initErrorIndicator(QWidget* tab, QWidget* widget);
};
