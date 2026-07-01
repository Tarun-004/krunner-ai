#include "groqrunner.h"
#include "groqclient.h"

#include <KRunner/QueryMatch>
#include <KRunner/RunnerContext>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>
#include <KConfigGroup>

#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <QClipboard>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QProcessEnvironment>
#include <QLoggingCategory>

K_PLUGIN_CLASS_WITH_JSON(GroqRunner, "krunner_groq.json")

// Words that strongly indicate a natural-language question rather than an
// app/file/action name. Kept lowercase; matched against the first word only.
static const QStringList QUESTION_LEAD_WORDS = {
    QStringLiteral("what"),   QStringLiteral("why"),    QStringLiteral("how"),
    QStringLiteral("who"),    QStringLiteral("when"),   QStringLiteral("where"),
    QStringLiteral("which"),  QStringLiteral("explain"),QStringLiteral("define"),
    QStringLiteral("describe"),QStringLiteral("is"),    QStringLiteral("are"),
    QStringLiteral("does"),   QStringLiteral("do"),     QStringLiteral("can"),
    QStringLiteral("could"),  QStringLiteral("should"), QStringLiteral("will"),
    QStringLiteral("convert"),QStringLiteral("translate")
};

GroqRunner::GroqRunner(QObject *parent, const KPluginMetaData &metaData)
    : KRunner::AbstractRunner(parent, metaData)
{
    qWarning() << "[groq] GroqRunner constructed, pluginId:" << metaData.pluginId();
    // Lowest priority no longer exists as a concept in KF6 Runner; ranking is
    // purely relevance-based now (see CategoryRelevance::Lowest on the match
    // itself, set below). Keep a minimum query length so we never even
    // consider near-empty queries.
    setMinLetterCount(6);
}

GroqRunner::~GroqRunner() = default;

bool GroqRunner::looksLikeQuestion(const QString &query) const
{
    const QString trimmed = query.trimmed();
    if (trimmed.length() < 6) {
        return false;
    }

    // Anything that already looks like a path or a shell/command invocation
    // is none of our business.
    if (trimmed.startsWith(QLatin1Char('/')) || trimmed.startsWith(QLatin1String("~"))
        || trimmed.startsWith(QLatin1String("./"))) {
        return false;
    }

    const QStringList words = trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);

    // A single word is virtually always meant for the app/file/calculator
    // runners ("downloads", "firefox", "settings"...) - never treat that as
    // a question, regardless of what the word is.
    if (words.size() < 2) {
        return false;
    }

    if (trimmed.endsWith(QLatin1Char('?'))) {
        return true;
    }

    const QString firstWord = words.first().toLower();
    return QUESTION_LEAD_WORDS.contains(firstWord);
}

QString GroqRunner::loadApiKey() const
{
    // Preferred: environment variable, so the key never has to live on disk.
    const QString envKey = QProcessEnvironment::systemEnvironment().value(QStringLiteral("GROQ_API_KEY"));
    if (!envKey.isEmpty()) {
        return envKey;
    }

    // Fallback: ~/.config/krunner_groqrc, [General] ApiKey=...
    KConfigGroup grp(KSharedConfig::openConfig(QStringLiteral("krunner_groqrc")), QStringLiteral("General"));
    return grp.readEntry("ApiKey", QString());
}

void GroqRunner::match(KRunner::RunnerContext &context)
{
    const QString query = context.query();
    qWarning() << "[groq] match() called with query:" << query;

    if (!looksLikeQuestion(query)) {
        return; // Let every other runner do its job untouched.
    }

    qWarning() << "[groq] query looks like a question:" << query;

    const QString apiKey = loadApiKey();
    if (apiKey.isEmpty()) {
        qWarning() << "[groq] no API key found (checked GROQ_API_KEY env var and ~/.config/krunner_groqrc)";
        return; // Silently no-op rather than showing an error in everyone's face.
    }
    qWarning() << "[groq] API key found, length:" << apiKey.length();

    // --- Idle debounce ---
    // match() executes on a worker thread dedicated to this runner, so it is
    // safe to sleep here without blocking the UI or other runners. We wait a
    // short moment and bail out immediately if the user kept typing
    // (context.isValid() becomes false as soon as the query changes/ends).
    QThread::msleep(350);
    if (!context.isValid()) {
        qWarning() << "[groq] query went stale during debounce, aborting";
        return;
    }

    if (!m_client) {
        // Created lazily on this runner's worker thread - QNetworkAccessManager
        // must be used from the thread it was created in.
        m_client = new GroqClient();
        m_client->setApiKey(apiKey);
    }

    const quint64 requestId = ++m_requestCounter;

    QEventLoop loop;
    QString answer;
    bool success = false;

    connect(m_client, &GroqClient::answerReady, &loop, [&](quint64 id, const QString &text) {
        if (id != requestId) {
            return;
        }
        qWarning() << "[groq] answer received:" << text;
        answer = text;
        success = true;
        loop.quit();
    });
    connect(m_client, &GroqClient::errorOccurred, &loop, [&](quint64 id, const QString &msg) {
        if (id != requestId) {
            return;
        }
        qWarning() << "[groq] request error:" << msg;
        loop.quit();
    });

    // Watchdog: never block KRunner for more than ~4s, and bail out early as
    // soon as the query becomes stale (user kept typing / cancelled).
    QTimer watchdog;
    watchdog.setInterval(100);
    connect(&watchdog, &QTimer::timeout, &loop, [&]() {
        if (!context.isValid()) {
            m_client->cancel(requestId);
            loop.quit();
        }
    });
    watchdog.start();

    QTimer::singleShot(4000, &loop, &QEventLoop::quit);

    m_client->ask(query, requestId);
    loop.exec();

    if (!success || answer.isEmpty() || !context.isValid()) {
        qWarning() << "[groq] not adding a match. success=" << success << "answerEmpty=" << answer.isEmpty()
                    << "contextValid=" << context.isValid();
        return;
    }

    qWarning() << "[groq] adding match to context";

    KRunner::QueryMatch match(this);
    match.setCategoryRelevance(KRunner::QueryMatch::CategoryRelevance::Lowest);
    match.setRelevance(0.0); // never outrank real matches; just appear beneath them
    match.setIconName(QStringLiteral("edit-find"));
    match.setText(i18n("Groq: %1", answer));
    match.setSubtext(i18n("Press Enter to copy"));
    match.setMultiLine(true); // let the full answer wrap instead of getting cut off with "..."
    match.setData(answer);

    context.addMatch(match);
}

void GroqRunner::run(const KRunner::RunnerContext & /*context*/, const KRunner::QueryMatch &match)
{
    // Keep it simple: Enter just copies the answer to the clipboard.
    // No extra dialog, no extra window - stays entirely inside KRunner's own UI.
    if (auto *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(match.data().toString());
    }
}

#include "groqrunner.moc"