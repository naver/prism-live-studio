#include "TestCase.h"

namespace tests {
    std::list<tests::TestCase*>& getTestCases() {
        static std::list<tests::TestCase*> testcases;
        return testcases;
    }
}