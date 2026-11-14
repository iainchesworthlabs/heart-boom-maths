// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJSValue>
#include <QMetaProperty>
#include <QPointF>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QVariantMap>
#include <algorithm>
#include <set>
#include <utility>
#include <vector>

using namespace heart_boom_maths;
using namespace heart_boom_maths::game;

namespace hbm_test
{

namespace
{

GameServices make_services(UiRig& rig)
{
    GameServices services;
    services.store = &rig.store;
    services.audio = &rig.audio;
    services.scores = &rig.scores;
    services.links = &rig.links;
    services.clock = &rig.clock;
    return services;
}

constexpr std::uint64_t FIRST_GATE_SEED = 1000;

} // namespace

GateChallenge UiRig::gate_challenge(int nth)
{
    return make_gate_challenge(FIRST_GATE_SEED + static_cast<std::uint64_t>(nth));
}

UiRig::UiRig(std::uint64_t seed, bool scores_available, std::string saved_text)
    : scores(scores_available), store(saved_text.empty() ? MemorySaveStore() : MemorySaveStore(std::move(saved_text))),
      controller(make_services(*this),
                 [this, seed]
                 {
                     GameOptions options;
                     options.session_seed = seed;
                     options.gate_seed_source = [this]
                     { return FIRST_GATE_SEED + static_cast<std::uint64_t>(gate_seeds_used++); };
                     return options;
                 }())
{
}

UiRig::UiRig(std::uint64_t seed, std::string save_text) : UiRig(seed, false, std::move(save_text)) {}

void UiRig::play_for_ms(int milliseconds)
{
    for (int elapsed = 0; elapsed < milliseconds; elapsed += 50)
    {
        clock.advance_ms(50);
        controller.advance_time();
    }
}

void UiRig::answer_right()
{
    controller.answer(controller.session_view().question.correct_tile);
}

void UiRig::answer_wrong()
{
    const SessionView& v = controller.session_view();
    for (int tile = 0; tile < 4; ++tile)
    {
        if (tile != v.question.correct_tile && ((v.removed_tiles >> tile) & 1) == 0)
        {
            controller.answer(tile);
            return;
        }
    }
}

void UiRig::play_level_right()
{
    for (int i = 0; i < 10; ++i)
    {
        answer_right();
    }
}

QmlHarness::QmlHarness(GameController* game, const QString& component, QSize size, const QString& extra_name,
                       QObject* extra)
    : m_View(std::make_unique<QQuickView>())
{
    QVariantMap initial;
    if (game != nullptr)
    {
        initial.insert(QStringLiteral("game"), QVariant::fromValue(game));
    }
    if (!extra_name.isEmpty())
    {
        initial.insert(extra_name, QVariant::fromValue(extra));
    }
    m_View->setInitialProperties(initial);
    m_View->setResizeMode(QQuickView::SizeRootObjectToView);
    m_View->resize(size);
    m_View->setSource(QUrl(QStringLiteral("qrc:/qt/qml/HeartBoomMaths/") + component));
    finish_loading(component);
}

QmlHarness::QmlHarness(FromSource, const QByteArray& qml, QSize size) : m_View(std::make_unique<QQuickView>())
{
    m_View->setResizeMode(QQuickView::SizeRootObjectToView);
    m_View->resize(size);
    // Written to a file in a temporary directory; the module is found through the resource import path like any screen.
    m_Dir = std::make_unique<QTemporaryDir>();
    REQUIRE(m_Dir->isValid());
    QFile file(m_Dir->filePath(QStringLiteral("Inline.qml")));
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write(qml);
    file.close();
    m_View->setSource(QUrl::fromLocalFile(file.fileName()));
    finish_loading(QStringLiteral("inline QML"));
}

QmlHarness QmlHarness::from_source(const QByteArray& qml, QSize size)
{
    return QmlHarness(FromSource{}, qml, size);
}

void QmlHarness::finish_loading(const QString& what)
{
    if (m_View->status() != QQuickView::Ready)
    {
        QStringList messages;
        for (const auto& error : m_View->errors())
        {
            messages << error.toString();
        }
        FAIL("The QML component " << what.toStdString() << " did not load: " << messages.join("; ").toStdString());
    }
    m_View->show();
    settle();
}

QQuickItem* QmlHarness::root() const
{
    return m_View->rootObject();
}

QQuickView& QmlHarness::view()
{
    return *m_View;
}

QObject* QmlHarness::find(const QString& object_name) const
{
    // Breadth first over both trees: the objects a QML file declares hang off QObject parents, and the items a Repeater
    // or a Loader makes are reached through their visual parents.
    std::vector<QObject*> queue;
    std::set<QObject*> seen;
    if (root() != nullptr)
    {
        queue.push_back(root());
    }
    for (std::size_t next = 0; next < queue.size(); ++next)
    {
        QObject* object = queue[next];
        if (!seen.insert(object).second)
        {
            continue;
        }
        if (object->objectName() == object_name)
        {
            return object;
        }
        for (QObject* child : object->children())
        {
            queue.push_back(child);
        }
        if (auto* item = qobject_cast<QQuickItem*>(object))
        {
            for (QQuickItem* child : item->childItems())
            {
                queue.push_back(child);
            }
        }
    }
    return nullptr;
}

QQuickItem* QmlHarness::find_item(const QString& object_name) const
{
    return qobject_cast<QQuickItem*>(find(object_name));
}

void QmlHarness::settle()
{
    // A frame needs the window exposed; grabbing one polishes and renders the scene so layouts have run.
    QCoreApplication::processEvents();
    (void)m_View->grabWindow();
    QCoreApplication::processEvents();
    // A Loader deletes the screen it replaces later; until then the old items would still turn up in a search.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

void QmlHarness::resize(QSize size)
{
    m_View->resize(size);
    settle();
}

bool QmlHarness::grab(const QString& path)
{
    settle();
    const QImage image = m_View->grabWindow();
    if (image.isNull())
    {
        return false;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    return image.save(path);
}

QVariant property_of(const QObject* object, const char* name)
{
    return object == nullptr ? QVariant() : object->property(name);
}

QStringList string_list(const QVariant& value)
{
    // A `var` property holding a JavaScript array arrives as a QJSValue.
    return value.canConvert<QJSValue>() ? value.value<QJSValue>().toVariant().toStringList() : value.toStringList();
}

void click(QQuickItem* item)
{
    REQUIRE(item != nullptr);
    QQuickWindow* window = item->window();
    REQUIRE(window != nullptr);
    const QPointF scene = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, scene.toPoint());
    QCoreApplication::processEvents();
}

} // namespace hbm_test
