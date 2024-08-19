//#include <obs.h>

#include "TestCase.h"

#include <algorithm>
#include <iostream>

#include <qapplication.h>

bool runTestCase(int& result, tests::TestCase* tc);
bool runDeps(int& result, const std::list<tests::TestCase*>& deps);
int runTests();

int main(int argc, char* argv[]) {
    QApplication a(argc, argv);
    QMetaObject::invokeMethod(&a, []() { QApplication::exit(runTests()); }, Qt::QueuedConnection);
    return a.exec();
}

bool runDeps(int& result, const std::list<tests::TestCase*>& deps) {
    for (auto dep : deps)
        if (!runTestCase(result, dep))
            return false;
    return true;
}
bool runTestCase(int& result, tests::TestCase* tc) {
    if (tc->result)
        return tc->result.value() == 0;
    else if (!runDeps(result, tc->deps))
        return false;

    tc->result = QTest::qExec(tc->qobj);
    result += tc->result.value();
    return (tc->result.value() == 0);
}
int runTests() {
    int result = 0;
    for (auto tc : pls_get_testcases())
        runTestCase(result, tc);
    return result;
}
