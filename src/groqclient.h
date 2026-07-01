#pragma once

#include <QObject>
#include <QString>
#include <QHash>

class QNetworkAccessManager;
class QNetworkReply;

/**
 * Minimal async client for the Groq OpenAI-compatible chat completions endpoint.
 * Emits answerReady(requestId, text) on success or errorOccurred(requestId, message) on failure.
 * Every call is tagged with a requestId so the runner can discard stale/cancelled answers
 * (the user kept typing) instead of showing an outdated result.
 */
class GroqClient : public QObject
{
    Q_OBJECT

public:
    explicit GroqClient(QObject *parent = nullptr);

    // Fire off a question. Returns immediately; result arrives via signals.
    void ask(const QString &question, quint64 requestId);

    // Cancels any in-flight request matching this id (no-op if already finished).
    void cancel(quint64 requestId);

    void setApiKey(const QString &key);
    bool hasApiKey() const;

Q_SIGNALS:
    void answerReady(quint64 requestId, const QString &answer);
    void errorOccurred(quint64 requestId, const QString &message);

private:
    void onReplyFinished(QNetworkReply *reply, quint64 requestId);

    QNetworkAccessManager *m_manager;
    QString m_apiKey;
    QHash<quint64, QNetworkReply *> m_activeReplies;
};
