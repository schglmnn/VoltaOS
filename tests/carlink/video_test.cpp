#include "CarVideoDecoder.h"
#include <QProcess>
#include <QRegularExpression>
#include <QTest>

class VideoTest : public QObject {
    Q_OBJECT
    QByteArray sps, pps, idr, predicted;
private slots:
    void initTestCase() {
        // Real encoded frames exercise FFmpeg rather than a mocked decoder.
        QProcess encoder;
        encoder.start("ffmpeg", {"-v", "error", "-f", "lavfi", "-i", "color=size=64x64:rate=10",
            "-frames:v", "3", "-c:v", "libx264", "-threads", "1", "-tune", "zerolatency",
            "-x264-params", "keyint=30:scenecut=0", "-f", "h264", "pipe:1"});
        QVERIFY(encoder.waitForFinished(15000));
        QCOMPARE(encoder.exitCode(), 0);
        const auto stream = encoder.readAllStandardOutput();
        const QString bytes = QString::fromLatin1(stream);
        QRegularExpression codes(QStringLiteral("\\x00\\x00(?:\\x00)?\\x01"));
        auto matches = codes.globalMatch(bytes);
        QList<QPair<int, int>> starts;
        while (matches.hasNext()) {
            const auto match = matches.next();
            starts.append({int(match.capturedStart()), int(match.capturedLength())});
        }
        for (int i = 0; i < starts.size(); ++i) {
            const int start = starts[i].first, header = start + starts[i].second;
            const auto nal = stream.mid(start, (i + 1 < starts.size() ? starts[i + 1].first : int(stream.size())) - start);
            switch (uchar(stream[header]) & 31) {
            case 7: sps = nal; break;
            case 8: pps = nal; break;
            case 5: idr = nal; break;
            case 1: predicted = nal; break;
            }
        }
        QVERIFY(!sps.isEmpty() && !pps.isEmpty() && !idr.isEmpty() && !predicted.isEmpty());
    }
    void resumedStreamWithoutHeaders() {
        CarVideoDecoder decoder;
        for (int i = 0; i < 100; ++i) decoder.decode(predicted);
        QCOMPARE(decoder.frames(), 0);
        QVERIFY(decoder.needsKeyframe());
        decoder.decode(sps + pps + idr);
        QVERIFY(decoder.frames() > 0);
        QVERIFY(!decoder.needsKeyframe());
    }
    void separateParameterPacketsAndReconnect() {
        CarVideoDecoder decoder;
        for (int reconnect = 0; reconnect < 2; ++reconnect) {
            decoder.reset();
            decoder.decode(sps);
            decoder.decode(idr); // PPS missing: still cannot decode.
            QCOMPARE(decoder.frames(), 0);
            QVERIFY(decoder.needsKeyframe());
            decoder.decode(pps);
            decoder.decode(idr);
            QVERIFY(decoder.frames() > 0);
            QVERIFY(!decoder.needsKeyframe());
            const int before = decoder.frames();
            decoder.decode(predicted);
            QVERIFY(decoder.frames() > before);
        }
    }
    void damagedStreamCanRecover() {
        CarVideoDecoder decoder;
        decoder.decode(sps + pps + idr);
        QVERIFY(decoder.frames() > 0);
        decoder.decode(QByteArray::fromHex("0000000165ff"));
        QVERIFY(decoder.needsKeyframe());
        const int before = decoder.frames();
        decoder.decode(predicted);
        QCOMPARE(decoder.frames(), before);
        decoder.decode(idr); // Cached SPS/PPS allow resynchronization.
        QVERIFY(decoder.frames() > before);
        QVERIFY(!decoder.needsKeyframe());
    }
};
QTEST_GUILESS_MAIN(VideoTest)
#include "video_test.moc"
