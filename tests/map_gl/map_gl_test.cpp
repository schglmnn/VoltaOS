#include <QGuiApplication>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QQmlContext>
#include <QQuickView>
#include <QSignalSpy>
#include <QTest>
#include <atomic>

class MapGLTest : public QObject {
    Q_OBJECT
private slots:
    void inheritedErrorDoesNotAbortMapUpload() {
        std::atomic<int> polluted{0};
        QQuickView view;
        view.rootContext()->setContextProperty("testStyleUrl", QUrl::fromLocalFile(QStringLiteral(TEST_SOURCE_DIR "/style.json")).toString());
        // Reproduce a GL error left by another renderer in the shared context.
        connect(&view, &QQuickWindow::beforeSynchronizing, &view, [&] {
            if (auto *context = QOpenGLContext::currentContext()) {
                context->functions()->glEnable(GLenum(0xffffffffu));
                ++polluted;
            }
        }, Qt::DirectConnection);
        view.setSource(QUrl::fromLocalFile(QStringLiteral(TEST_SOURCE_DIR "/scene.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QImage rendered;
        bool sawRed = false;
        for (int i = 0; i < 60 && !sawRed; ++i) {
            view.update();
            QTest::qWait(50);
            rendered = view.grabWindow();
            if (!rendered.isNull()) {
                const QColor center = rendered.pixelColor(rendered.width()/2, rendered.height()/2);
                sawRed = center.red() > 180 && center.green() < 100 && center.blue() < 100;
            }
        }
        QVERIFY(polluted.load() > 0);
        QVERIFY2(sawRed, "The offline GeoJSON polygon must render despite an inherited OpenGL error");
        view.hide();
        view.releaseResources();
    }
};

int main(int argc, char **argv) {
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QGuiApplication app(argc, argv);
    QCoreApplication::addLibraryPath(qEnvironmentVariable("VOLTA_MAPLIBRE_DIR") + "/plugins");
    MapGLTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "map_gl_test.moc"
