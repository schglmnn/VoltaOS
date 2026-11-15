#include "GpsService.h"
#include <QSignalSpy>
#include <QTest>

class GpsTest : public QObject {
    Q_OBJECT
private slots:
    void init() {
        // Invalid-input tests must never probe an attached serial device.
        qputenv("GPS_PORT", "/nonexistent-volta-gps-test");
    }
    void stationaryFix() {
        qputenv("VOLTA_GPS_SIMULATE", "48.1,10.2");
        GpsService gps;
        QSignalSpy fixes(&gps, &GpsService::updated);
        QVERIFY(gps.connected());
        QVERIFY(gps.source().contains("Simulated"));
        QVERIFY(gps.port().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(fixes.count() >= 2, 1000);
        for (const auto &args : fixes) {
            const auto fix = args[0].toMap();
            QCOMPARE(fix["lat"].toDouble(), 48.1);
            QCOMPARE(fix["lon"].toDouble(), 10.2);
            QCOMPARE(fix["speed"].toDouble(), 0.0);
            QCOMPARE(fix["sats"].toInt(), 0);
            QVERIFY(fix["fix"].toBool());
            QVERIFY(fix["simulated"].toBool());
        }
    }
    void invalidCoordinates_data() {
        QTest::addColumn<QByteArray>("coordinates");
        for (const char *value : {"91,10", "48,181", "-91,0", "0,-181", "nan,0", "inf,0", "48", "48,10,20", "x,10", ""})
            QTest::newRow(value[0] ? value : "unset") << QByteArray(value);
    }
    void invalidCoordinates() {
        QFETCH(QByteArray, coordinates);
        qputenv("VOLTA_GPS_SIMULATE", coordinates);
        GpsService gps;
        QSignalSpy fixes(&gps, &GpsService::updated);
        QTest::qWait(150);
        QVERIFY(!gps.connected());
        QVERIFY(fixes.isEmpty());
    }
};
QTEST_GUILESS_MAIN(GpsTest)
#include "gps_test.moc"
