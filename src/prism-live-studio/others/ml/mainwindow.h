#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QHash>
#include <QList>
#include <QPair>
#include <QStringList>
#include <QSettings>
#include <QButtonGroup>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_localeDirBrowse_clicked();
    void on_outFileBrowse_clicked();
    void on_gen_clicked();
    void on_fillColorFileBrowse_clicked();
    void on_chooseColor_clicked();
    void on_fillColor_clicked();
    void on_import_2_clicked();
    void on_clearLog_clicked();
    void on_cmpSrcDirBrowse_clicked();
    void on_cmpDstDirBrowse_clicked();
    void on_cmp_clicked();

    void on_toolButton_triggered(QAction *arg1);

    void on_toolButton_2_triggered(QAction *arg1);

    void on_pushButton_clicked();

    void on_pushButton_4_clicked();

    private:
    void loadOldIni();
    void creatSettings(const QString &filePath);
    void genIni();
private:
    Ui::MainWindow *ui;
    // feature => <keys , lang => < key => text >  >
    QHash<QString, QPair<QStringList, QHash<QString, QHash<QString, QString>>>> m_texts;
    QHash<QString, QHash<QString, QMap<QString, QString>>>m_contents;
    QStringList m_keys;
    QStringList m_newKeys;
    QButtonGroup m_fileFlagGroup;
    QString m_repalaceSrcDir;
    QString m_replaceDstDir;

};
#endif // MAINWINDOW_H
