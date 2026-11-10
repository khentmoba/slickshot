//
// Created by yuriypuchkov on 09.12.2020.
//

#pragma once

#include "utils/appupdate.h"

#include <QPointer>
#include <QWidget>

class QVBoxLayout;
class QPropertyAnimation;
class QScrollArea;
class QPushButton;
class QLabel;

class UpdateNotificationWidget : public QWidget
{
    Q_OBJECT
public:
    explicit UpdateNotificationWidget(QWidget* parent,
                                      const AppUpdateInfo& info);
    void setAppLatestVersion(const QString& latestVersion);
    void setUpdateInfo(const AppUpdateInfo& info);

    void hide();
    void show();

private:
    void initInternalPanel();

public slots:
    void ignoreButton();
    void laterButton();
    void updateButton();

private:
    // class members
    AppUpdateInfo m_info;
    QVBoxLayout* m_layout;
    QLabel* m_notification;
    QScrollArea* m_internalPanel;
    QPropertyAnimation* m_showAnimation;
    QPropertyAnimation* m_hideAnimation;
};
