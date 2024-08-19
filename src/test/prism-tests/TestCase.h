#pragma once

#include <functional>
#include <list>
#include <optional>

#include <qtest.h>

#define pls_get_testcases() tests::getTestCases()
#define PLS_TESTCASE(Test, ...) \
public: \
    static auto testCase() { \
        return &tests::Singleton<tests::WrapTestCase<Test, __VA_ARGS__>>::instance; \
    } \
    static Test* instance() { \
        return testCase()->obj(); \
    }

class QObject;

namespace tests {
    template<typename T>
    struct Singleton {
        static T instance;
    };
    template<typename T>
    T Singleton<T>::instance;

    struct TestCase {
        QObject* qobj;
        std::list<tests::TestCase*> deps;
        std::optional<int> result;
    };

    std::list<tests::TestCase*>& getTestCases();

    template<typename... Deps>
    struct GetDeps;
    template<typename Dep, typename... Deps>
    struct GetDeps<Dep, Deps...> {
        static void get(std::list<tests::TestCase*>& deps) {
            deps.push_back(Dep::testCase());
            GetDeps<Deps...>::get(deps);
        }
    };
    template<>
    struct GetDeps<> {
        static void get(const std::list<tests::TestCase*>&) {
        }
    };

    template<typename Test, typename... Deps>
    struct WrapTestCase : TestCase {
        WrapTestCase() {
            qobj = new Test();
            GetDeps<Deps...>::get(deps);
            pls_get_testcases().push_back(this);
        }
        Test* obj() {
            return static_cast<Test*>(qobj);
        }
    };
}
