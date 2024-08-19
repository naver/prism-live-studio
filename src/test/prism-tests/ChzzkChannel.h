#pragma once

#include "TestCase.h"

#include "ChzzkLogin.h"

class ChzzkChannel : public QObject {
    Q_OBJECT
    PLS_TESTCASE(ChzzkChannel, ChzzkLogin)

private slots:
    void initTestCase();
    void cleanupTestCase();
};
