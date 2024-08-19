#pragma once

#include "TestCase.h"

#include <qobject.h>

class ChzzkLogin : public QObject {
    Q_OBJECT
    PLS_TESTCASE(ChzzkLogin)

private slots:
    void initTestCase();
    void cleanupTestCase();
    void login();
    void logout();

public:
    QString streamKey{ "gggggggggggggggggggggggggg" };
    QString streamUrl{ "gggggggggggggggggggggggggg" };
};