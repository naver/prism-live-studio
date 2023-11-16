#include <qcoreapplication.h>

#include <stdio.h>
#include <qjsonarray.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qfile.h>
#include <qprocess.h>
#include <qpair.h>
#include <qdebug.h>
#include <qdatetime.h>
#include <qdir.h>
#include <qthreadpool.h>
#include <qcryptographichash.h>
#include <qmap.h>
#include <memory>
#include <tuple>
#include <iostream>

class SignProcess {
public:
    enum class State {
        None,
        Running,
        Completed,
        Failed
    };

public:
    SignProcess()
        : m_id(0)
        , m_state(State::None)
        , m_command()
        , m_process(new QProcess)
    {
        //std::cout<<" current path "<<QDir::currentPath().toStdString()<<std::endl;
        m_process->setWorkingDirectory(QDir::currentPath());
    }
    ~SignProcess()
    {
        //qDebug() << "~SignProcess()";
        clearConnections();
        m_process->deleteLater();
    }

public:
    bool isUnused() const
    {
        return m_state == State::None;
    }
    bool isRunning() const
    {
        return (m_state == State::Running);
    }
    bool isCompleted() const
    {
        return (m_state == State::Completed);
    }
    bool isFailed() const
    {
        return (m_state == State::Failed);
    }
    void setState(State state)
    {
        m_state = state;
    }
    QProcess* process()
    {
        return m_process;
    }
    void start(const QString& md5)
    {
        //m_md5 = md5;
        m_process->start(m_command);
        // qDebug().noquote() << "start command: " << m_command << md5;
    }
    void readOutput(QProcess::ProcessChannel channel)
    {
        m_process->setReadChannel(channel);
        while (m_process->canReadLine()) {
            if (QByteArray text = m_process->readLine().replace("\\r", "").replace("\\n", ""); !text.trimmed().isEmpty()) {
                std::cout << trimmedRight(text).constData() << std::endl;
            }
        }
    }
    void addConnection(const QMetaObject::Connection& connection)
    {
        m_connections.append(connection);
    }
    void clearConnections()
    {
        while (!m_connections.isEmpty()) {
            QObject::disconnect(m_connections.takeFirst());
        }
    }
    QByteArray& trimmedRight(QByteArray& text)
    {
        while ((!text.isEmpty()) && isspace(text.back())) {
            text.resize(text.length() - 1);
        }
        return text;
    }
    static QString md5(const QString& filePath)
    {
        QFile file(filePath);
        if (!file.open(QFile::ReadOnly)) {
            return QString();
        }

        QCryptographicHash hash(QCryptographicHash::Md5);
        while (!file.atEnd()) {
            hash.addData(file.read(1024 * 1024));
        }

        QString smd5;
        QByteArray bmd5 = hash.result();
        for (int i = 0, length = bmd5.length(); i < length; ++i) {
            smd5.append(QString::number((uint)(quint8)bmd5[i], 16).rightJustified(2, '0'));
        }
        return smd5;
    }

    friend void signProcessOk(std::shared_ptr<SignProcess> signProcess, const QString& md5);
    friend void signProcessFailed(std::shared_ptr<SignProcess> signProcess);

public:
    int m_id;
    State m_state;
    QString m_command;
    QString m_file;
    QString m_filePath;
    //QString m_md5;
    QProcess* m_process;
    QList<QMetaObject::Connection> m_connections;
};

bool g_running = true;
int g_exitCode = 0;
QList<std::shared_ptr<SignProcess>> g_signProcesses;
QList<std::tuple<int, QString, QString, QString>> g_signCommands; // id, command, file, filepath
//QMap<QPair<int, QString>, QString> g_fileMd5s; // <id, file>, md5


QJsonArray loadArray(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly)) {
        return QJsonArray();
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        return QJsonArray();
    }
    return doc.array();
}

QStringList getStringList(const QJsonObject& object, const QString& name)
{
    QJsonArray array = object[name].toArray();

    QStringList stringList;
    for (int i = 0, count = array.count(); i < count; ++i) {
        stringList.append(array[i].toString());
    }
    return stringList;
}



QJsonArray createTaskJson()
{
    QString OUTPUT_DIR = qEnvironmentVariable("OUTPUT_DIR",".");
    std::cout<<" OUTPUT_DIR "<<OUTPUT_DIR.toStdString()<<std::endl;

    QFile file(OUTPUT_DIR+"/codefiles.txt");
    QJsonArray exeList;
    QJsonArray dllList;

    if(file.open(QIODevice::ReadOnly|QIODevice::Text))
    {
        while (!file.atEnd()) {
            QString line = QString::fromLocal8Bit(file.readLine().trimmed());
            if(line.endsWith(".exe", Qt::CaseInsensitive))
            {
                std::cout<<" exe: "<<line.toStdString()<<std::endl;
                if (!(line.endsWith(QStringLiteral("PRISMLens.exe"), Qt::CaseInsensitive)
                      || line.endsWith(QStringLiteral("PrismLensInstall.exe"), Qt::CaseInsensitive)))
                    exeList.append(line);
                dllList.append(line);
            }
            if(line.endsWith(".dll", Qt::CaseInsensitive))
            {
                std::cout<<" dll: "<<line.toStdString()<<std::endl;
                dllList.append(line);
            }
            if(line.endsWith(".node", Qt::CaseInsensitive))
            {
                std::cout<<" node: "<<line.toStdString()<<std::endl;
                dllList.append(line);
            }
        }

    }
    //local task
    QString CURRENT_SCRIPT_DIR = qEnvironmentVariable("CURRENT_SCRIPT_DIR");

    QJsonObject task1;
    task1.insert("id",1);
    task1.insert("command","test.bat ${VERSION} \"${FILE}\"");
    task1.insert("dir","");
    task1.insert("files",QJsonArray{"test case for run first "});

    QJsonObject task2;
    task2.insert("id",2);
    task2.insert("command","sign.bat ${VERSION} \"${FILE}\"");
    //task1.insert("command","test.bat ${VERSION} \"${FILE}\"");
    task2.insert("dir","");
    task2.insert("files",dllList);

    QJsonObject task3;
    task3.insert("id",3);
    task3.insert("command","Powershell.exe -executionpolicy remotesigned -File \"${PROJECT_DIR}/build/windows/ahf.ps1\" -FilePath \"${FILE}\"");
    task3.insert("dir","");
    task3.insert("files",exeList);


    QJsonArray docArray;
    docArray.append(task1);
    docArray.append(task2);
    docArray.append(task3);


    return docArray;
}

void loadSignCommands()
{
    // env: ${VERSION}
    //      ${PROJECT_DIR}
    //      ${FILE}

    QString PROJECT_DIR = QString::fromUtf8(qgetenv("PROJECT_DIR"));
    QString VERSION = qEnvironmentVariable("VERSION");
    std::cout<<" env "<<VERSION.toStdString()<<" proc dir "<<PROJECT_DIR.toStdString()<<std::endl;

    QJsonArray configs = createTaskJson();

    for (int i = 0; i < configs.count(); ++i) {
        QJsonObject config = configs[i].toObject();
        int id = config[QStringLiteral("id")].toInt();
        QString command = config[QStringLiteral("command")].toString();

        command.replace("${PROJECT_DIR}", PROJECT_DIR)
                .replace("${VERSION}", VERSION);
        std::cout<<" orgin cmd "<<command.toStdString()<<std::endl;

        QStringList files = getStringList(config, QStringLiteral("files"));
        for (QString file : files) {
            QString tmpCommand = command;
            tmpCommand.replace("${FILE}", file);
            //std::cout<<" tmp "<<tmpCommand.toStdString()<<std::endl;
            g_signCommands.append(std::make_tuple(id, tmpCommand, file, file));
        }
    }
}

int getParallelCount()
{
    if (int parallelCount = QString::fromUtf8(qgetenv("PARALLEL_COUNT")).toInt(); parallelCount > 0) {
        return parallelCount;
    }
    return 4;
}

void startSignCommand(std::shared_ptr<SignProcess> signProcess, const std::tuple<int, QString, QString, QString>& signCommand)
{
    int id = std::get<0>(signCommand);
    QString file = std::get<2>(signCommand);

    signProcess->m_id = id;
    signProcess->m_command = QDir::toNativeSeparators(std::get<1>(signCommand));
    signProcess->m_file = file;
    signProcess->m_filePath = QDir::toNativeSeparators(std::get<3>(signCommand));
    //signProcess->m_md5 = oldMd5;
    signProcess->m_state = SignProcess::State::Running;
    signProcess->start(QString());
    std::cout<<" start "<<id <<"  "<<std::get<1>(signCommand).toStdString()<<std::endl;


}
void startSignCommand(std::shared_ptr<SignProcess> signProcess)
{
    if (g_running && (signProcess->isUnused() || signProcess->isCompleted()) && (!g_signCommands.isEmpty())) {
        startSignCommand(signProcess, g_signCommands.takeFirst());
    }
}

void checkSignProcessFinished()
{
    for (std::shared_ptr<SignProcess> signProcess : g_signProcesses) {
        if (signProcess->isRunning()) {
            return;
        }
    }

    while (!g_signProcesses.isEmpty()) {
        std::shared_ptr<SignProcess> signProcess = g_signProcesses.takeLast();
        signProcess->clearConnections();
    }

    QCoreApplication::processEvents();

    if (!g_exitCode) {
        std::cout << "Sign Succeeded." << std::endl;
    }
    else {
        std::cout << "Sign Failed." << std::endl;
    }

    qApp->exit(g_exitCode);
}

void signProcessOk(std::shared_ptr<SignProcess> signProcess, const QString& md5)
{
    signProcess->setState(SignProcess::State::Completed);

    //g_fileMd5s[QPair<int, QString>(signProcess->m_id, signProcess->m_file)] = md5;

    startSignCommand(signProcess);

    QMetaObject::invokeMethod(qApp, &checkSignProcessFinished, Qt::QueuedConnection);
}

void signProcessFailed(std::shared_ptr<SignProcess> signProcess)
{
    g_running = false;
    g_exitCode += 1;

    signProcess->setState(SignProcess::State::Failed);

    //g_fileMd5s.remove(QPair<int, QString>(signProcess->m_id, signProcess->m_file));
    std::cout<<" fail "<<signProcess->m_process->errorString().toUtf8().constData()<< " error "
            <<signProcess->m_process->error()<<std::endl;
    QMetaObject::invokeMethod(qApp, &checkSignProcessFinished, Qt::QueuedConnection);
}

int main(int argc, char* argv[])
{
    std::cout << "start time: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss").toLocal8Bit().constData() << std::endl;

    QCoreApplication a(argc, argv);

    QObject::connect(&a, &QObject::destroyed, [applicationDirPath = QCoreApplication::applicationDirPath()]() {
        //saveFileMd5s(applicationDirPath);
        std::cout << "end time: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss").toLocal8Bit().constData() << std::endl;
    });

    loadSignCommands();
    if (g_signCommands.isEmpty()) {
        std::cout<<" empty command "<<std::endl;
        return 0;
    }

    //loadFileMd5s();

    for (int i = 0, parallelCount = qMin(getParallelCount(), g_signCommands.size()); i < parallelCount; ++i) {
        std::shared_ptr<SignProcess> signProcess = std::make_shared<SignProcess>();
        g_signProcesses.append(signProcess);

        signProcess->addConnection(
                    QObject::connect(signProcess->process(), static_cast<void (QProcess::*)(int exitCode, QProcess::ExitStatus exitStatus)>(&QProcess::finished), &a, [signProcess](int exitCode, QProcess::ExitStatus exitStatus) {
            if ((exitCode == 0) && (exitStatus == QProcess::NormalExit)) {
                signProcessOk(signProcess, QString());
                /*QThreadPool::globalInstance()->start([signProcess]() {
                        QString md5 = SignProcess::md5(signProcess->m_filePath);
                        QMetaObject::invokeMethod(qApp, [signProcess, md5]() { signProcessOk(signProcess, md5); }, Qt::QueuedConnection);
                        });*/
            }
            else {
                signProcessFailed(signProcess);
            }
        }, Qt::QueuedConnection));
        signProcess->addConnection(
                    QObject::connect(signProcess->process(), &QProcess::errorOccurred, &a, [signProcess](QProcess::ProcessError error) {
                        signProcessFailed(signProcess);
                    }, Qt::QueuedConnection));
        signProcess->addConnection(
                    QObject::connect(signProcess->process(), &QProcess::readyReadStandardOutput, &a, [signProcess]() {
            signProcess->readOutput(QProcess::StandardOutput);
        }, Qt::QueuedConnection));
        signProcess->addConnection(
                    QObject::connect(signProcess->process(), &QProcess::readyReadStandardError, &a, [signProcess]() {
            signProcess->readOutput(QProcess::StandardError);
        }, Qt::QueuedConnection));

        startSignCommand(signProcess);
    }

    return a.exec();
}
