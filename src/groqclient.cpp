#include "groqclient.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

static const char *GROQ_ENDPOINT = "https://api.groq.com/openai/v1/chat/completions";
// Small, fast model: good for one-line factual answers with low latency.
static const char *GROQ_MODEL = "llama-3.1-8b-instant";

GroqClient::GroqClient(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
}

void GroqClient::setApiKey(const QString &key)
{
    m_apiKey = key;
}

bool GroqClient::hasApiKey() const
{
    return !m_apiKey.isEmpty();
}

void GroqClient::ask(const QString &question, quint64 requestId)
{
    if (!hasApiKey()) {
        Q_EMIT errorOccurred(requestId, QStringLiteral("No Groq API key configured"));
        return;
    }

    QNetworkRequest request{QUrl(QString::fromLatin1(GROQ_ENDPOINT))};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", "Bearer " + m_apiKey.toUtf8());

    QJsonObject systemMsg{
        {QStringLiteral("role"), QStringLiteral("system")},
        {QStringLiteral("content"),
         QStringLiteral("You answer in one short sentence (max ~25 words). "
                         "No markdown, no preamble, no 'Sure!' - just the direct answer.")}};
    QJsonObject userMsg{
        {QStringLiteral("role"), QStringLiteral("user")},
        {QStringLiteral("content"), question}};

    QJsonObject body{
        {QStringLiteral("model"), QString::fromLatin1(GROQ_MODEL)},
        {QStringLiteral("messages"), QJsonArray{systemMsg, userMsg}},
        {QStringLiteral("max_tokens"), 80},
        {QStringLiteral("temperature"), 0.3}};

    QNetworkReply *reply = m_manager->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_activeReplies.insert(requestId, reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId]() {
        onReplyFinished(reply, requestId);
    });
}

void GroqClient::cancel(quint64 requestId)
{
    if (QNetworkReply *reply = m_activeReplies.take(requestId)) {
        reply->disconnect();
        reply->abort();
        reply->deleteLater();
    }
}

void GroqClient::onReplyFinished(QNetworkReply *reply, quint64 requestId)
{
    // Already cancelled/removed - ignore stale callback.
    if (m_activeReplies.take(requestId) != reply) {
        reply->deleteLater();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        Q_EMIT errorOccurred(requestId, reply->errorString());
        reply->deleteLater();
        return;
    }

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    const QJsonArray choices = doc.object().value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        Q_EMIT errorOccurred(requestId, QStringLiteral("Empty response from Groq"));
        return;
    }

    const QString answer = choices.first()
                                .toObject()
                                .value(QStringLiteral("message"))
                                .toObject()
                                .value(QStringLiteral("content"))
                                .toString()
                                .trimmed();

    if (answer.isEmpty()) {
        Q_EMIT errorOccurred(requestId, QStringLiteral("Empty answer text"));
        return;
    }

    Q_EMIT answerReady(requestId, answer);
}
