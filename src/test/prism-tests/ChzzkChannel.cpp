#include  "ChzzkChannel.h"

void ChzzkChannel::initTestCase() {
    QVERIFY(ChzzkLogin::instance()->streamKey == "gggggggggggggggggggggggggg");
}
void ChzzkChannel::cleanupTestCase() {}
