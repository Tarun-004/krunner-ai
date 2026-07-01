#pragma once

#include <KRunner/AbstractRunner>

class GroqClient;

/**
 * KRunner plugin that answers natural-language questions using Groq's API.
 *
 * Design goals (deliberately conservative so it never gets in the way of
 * KRunner's normal behaviour):
 *  - Only ever activates for queries that clearly look like a natural-language
 *    question (multi-word, question-word/leading verb, or trailing "?").
 *    Single words like "downloads", "firefox", "calculator" never trigger it,
 *    so file/app/calculator runners are completely unaffected.
 *  - Runs at the lowest match priority and returns at most one match, so it
 *    never outranks a real file/app/action match.
 *  - Debounces: waits briefly and re-checks the query is still current before
 *    making any network call, so it doesn't fire an API request per keystroke.
 *  - The answer is shown directly under the query in KRunner's own result
 *    list (no extra UI, no popup) - pressing Enter just copies the answer.
 */
class GroqRunner : public KRunner::AbstractRunner
{
    Q_OBJECT

public:
    GroqRunner(QObject *parent, const KPluginMetaData &metaData);
    ~GroqRunner() override;

    void match(KRunner::RunnerContext &context) override;
    void run(const KRunner::RunnerContext &context, const KRunner::QueryMatch &match) override;

private:
    bool looksLikeQuestion(const QString &query) const;
    QString loadApiKey() const;

    GroqClient *m_client = nullptr; // created lazily, lives in the runner's worker thread
    quint64 m_requestCounter = 0;
};
