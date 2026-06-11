#include "widget.h"
#include "./ui_widget.h"

#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QDebug>

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    on_refresh_clicked();
}

Widget::~Widget()
{
    delete ui;
}

void Widget::on_refresh_clicked()
{
    bool prism = ui->app->currentIndex() == 0;

#ifdef Q_OS_WIN
    QSettings settings(QStringLiteral("NAVER Corporation"), prism ? QStringLiteral("Prism Live Studio") : QStringLiteral("PRISM Lens"));
#else
    auto path = QDir::homePath() + (prism ? QStringLiteral("/Library/Preferences/com.prismlive.prismlivestudio.plist") : QStringLiteral("/Library/Preferences/com.prismlive.camstudio.plist"));
    QSettings settings(path, QSettings::NativeFormat);
#endif

    if (settings.contains(QStringLiteral("DevServer"))) {
        ui->devServerExists->setChecked(true);
        ui->devServer->setChecked(settings.value(QStringLiteral("DevServer"), false).toBool());
    }
    else {
        ui->devServerExists->setChecked(false);
    }

    if (settings.contains(QStringLiteral("LocalLog"))) {
        ui->localLogExists->setChecked(true);
        ui->localLog->setChecked(settings.value(QStringLiteral("LocalLog"), false).toBool());
    }
    else {
        ui->localLogExists->setChecked(false);
    }

    if (settings.contains(QStringLiteral("IgnoreRepull"))) {
        ui->ignoreRepullExists->setChecked(true);
        ui->ignoreRepull->setChecked(settings.value(QStringLiteral("IgnoreRepull"), false).toBool());
    }
    else {
        ui->ignoreRepullExists->setChecked(false);
    }

    if (settings.contains(QStringLiteral("SupportExportTemplates"))) {
        ui->supportExportTemplatesExists->setChecked(true);
        ui->supportExportTemplates->setChecked(settings.value(QStringLiteral("SupportExportTemplates"), false).toBool());
    }
    else {
        ui->supportExportTemplatesExists->setChecked(false);
    }

    if (settings.contains(QStringLiteral("MultiLanguageTest"))) {
        ui->multiLanguageTestExists->setChecked(true);
        ui->multiLanguageTest->setChecked(settings.value(QStringLiteral("MultiLanguageTest"), false).toBool());
    }
    else {
        ui->multiLanguageTestExists->setChecked(false);
    }

    if (settings.contains(QStringLiteral("UpdateSpecifyApi"))) {
        ui->updateSpecifyApiExists->setChecked(true);
        auto value = settings.value(QStringLiteral("UpdateSpecifyApi"), QString()).toString();

        QJsonParseError error;
        auto update = QJsonDocument::fromJson(value.toUtf8(), &error).object();
        if (error.error == QJsonParseError::NoError) {
            ui->version->setText(update[QStringLiteral("version")].toString());
            ui->pkg->setText(update[QStringLiteral("fileUrl")].toString());

            QJsonObject updateInfoUrlList = update[QStringLiteral("updateInfoUrlList")].toObject();
            ui->updateInfoEn->setText(updateInfoUrlList[QStringLiteral("en")].toString());
            ui->updateInfoKr->setText(updateInfoUrlList[QStringLiteral("kr")].toString());
        }
    }
    else {
        ui->updateSpecifyApiExists->setChecked(false);
    }
}


void Widget::on_modify_clicked()
{
    bool prism = ui->app->currentIndex() == 0;

#ifdef Q_OS_WIN
    QSettings settings(QStringLiteral("NAVER Corporation"), prism ? QStringLiteral("Prism Live Studio") : QStringLiteral("PRISM Lens"));
#else
    auto path = QDir::homePath() + (prism ? QStringLiteral("/Library/Preferences/com.prismlive.prismlivestudio.plist") : QStringLiteral("/Library/Preferences/com.prismlive.camstudio.plist"));
    QSettings settings(path, QSettings::NativeFormat);
#endif

    if (ui->devServerExists->isChecked()) {
        settings.setValue(QStringLiteral("DevServer"), ui->devServer->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
    }
    else {
        settings.remove(QStringLiteral("DevServer"));
    }

    if (ui->localLogExists->isChecked()) {
        settings.setValue(QStringLiteral("LocalLog"), ui->localLog->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
    }
    else {
        settings.remove(QStringLiteral("LocalLog"));
    }

    if (ui->ignoreRepullExists->isChecked()) {
        settings.setValue(QStringLiteral("IgnoreRepull"), ui->ignoreRepull->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
    }
    else {
        settings.remove(QStringLiteral("IgnoreRepull"));
    }

    if (ui->supportExportTemplatesExists->isChecked()) {
        settings.setValue(QStringLiteral("SupportExportTemplates"), ui->supportExportTemplates->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
    }
    else {
        settings.remove(QStringLiteral("SupportExportTemplates"));
    }

    if (ui->multiLanguageTestExists->isChecked()) {
        settings.setValue(QStringLiteral("MultiLanguageTest"), ui->multiLanguageTest->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
    }
    else {
        settings.remove(QStringLiteral("MultiLanguageTest"));
    }

    if (ui->updateSpecifyApiExists->isChecked()) {
        QJsonObject update;
#ifdef Q_OS_WIN
        update[QStringLiteral("platformType")] = QStringLiteral("WIN64");
#else
        update[QStringLiteral("platformType")] = QStringLiteral("MAC");
#endif
        update[QStringLiteral("updateType")] = QStringLiteral("UPDATE");
        update[QStringLiteral("version")] = ui->version->text();
        update[QStringLiteral("fileUrl")] = ui->pkg->text();

        QJsonObject updateInfoUrlList;
        updateInfoUrlList[QStringLiteral("en")] = ui->updateInfoEn->text();
        updateInfoUrlList[QStringLiteral("kr")] = ui->updateInfoKr->text();
        update[QStringLiteral("updateInfoUrlList")] = updateInfoUrlList;
        update[QStringLiteral("appType")] = prism ? QStringLiteral("LIVE_STUDIO") : QStringLiteral("LENS");

        QString text = QString::fromUtf8(QJsonDocument(update).toJson(QJsonDocument::Compact));
        settings.setValue(QStringLiteral("UpdateSpecifyApi"), text);
    }
    else {
        settings.remove(QStringLiteral("UpdateSpecifyApi"));
    }
}


void Widget::on_cancel_clicked()
{
    close();
}


void Widget::on_app_currentIndexChanged(int index)
{
    on_refresh_clicked();
}

