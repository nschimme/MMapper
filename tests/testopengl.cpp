#include <opengl/OpenGLConfig.h>
#include <opengl/OpenGLProber.h>

#include <QtTest>

class TestOpenGL : public QObject
{
    Q_OBJECT

private slots:
    void testParseSurveyResult_GL()
    {
        OpenGLProber prober;
        QByteArray json = R"({"backend": "GL", "major": 4, "minor": 5})";
        auto result = prober.parseSurveyResult(json);

        QCOMPARE(result.backendType, OpenGLProber::BackendType::GL);
        QCOMPARE(result.format.majorVersion(), 4);
        QCOMPARE(result.format.minorVersion(), 5);
        QCOMPARE(result.format.profile(), QSurfaceFormat::CoreProfile);
        QCOMPARE(result.highestVersionString, std::string("GL4.5core"));

        QCOMPARE(OpenGLConfig::getGLVersionString(), std::string("GL4.5core"));
    }

    void testParseSurveyResult_GLES()
    {
        OpenGLProber prober;
        QByteArray json = R"({"backend": "GLES", "major": 3, "minor": 1})";
        auto result = prober.parseSurveyResult(json);

        QCOMPARE(result.backendType, OpenGLProber::BackendType::GLES);
        QCOMPARE(result.format.majorVersion(), 3);
        QCOMPARE(result.format.minorVersion(), 1);
        QCOMPARE(result.format.renderableType(), QSurfaceFormat::OpenGLES);
        QCOMPARE(result.highestVersionString, std::string("ES3.1"));

        QCOMPARE(OpenGLConfig::getESVersionString(), std::string("ES3.1"));
    }

    void testParseSurveyResult_Invalid()
    {
        OpenGLProber prober;
        QByteArray json = R"({"backend": "None"})";
        auto result = prober.parseSurveyResult(json);
        QCOMPARE(result.backendType, OpenGLProber::BackendType::None);

        json = "garbage";
        result = prober.parseSurveyResult(json);
        // Fallback is GL 3.3 Core
        QCOMPARE(result.backendType, OpenGLProber::BackendType::GL);
        QCOMPARE(result.highestVersionString, std::string("Fallback"));
    }

    void testGetBackendType()
    {
        OpenGLConfig::setBackendType(OpenGLProber::BackendType::GLES);
        QCOMPARE(OpenGLConfig::getBackendType(), OpenGLProber::BackendType::GLES);
    }

    void testMaxSamples()
    {
        OpenGLConfig::setMaxSamples(4);
        QCOMPARE(OpenGLConfig::getMaxSamples(), 4);
    }
};

QTEST_MAIN(TestOpenGL)
#include "testopengl.moc"
