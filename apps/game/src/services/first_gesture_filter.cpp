// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/first_gesture_filter.hpp"

#include <QEvent>

namespace heart_boom_maths::game
{

namespace
{

// The events a browser accepts as a user's activation: the press and release of a button, the touch and a key.
bool is_gesture(QEvent::Type type)
{
    switch (type)
    {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::TouchBegin:
    case QEvent::TouchEnd:
    case QEvent::KeyPress:
        return true;
    default:
        return false;
    }
}

} // namespace

FirstGestureFilter::FirstGestureFilter(AudioService& audio, QObject* parent) : QObject(parent), m_Audio(audio) {}

bool FirstGestureFilter::eventFilter(QObject* watched, QEvent* event)
{
    if (!m_Unlocked && is_gesture(event->type()))
    {
        m_Unlocked = true;
        m_Audio.unlock();
    }
    return QObject::eventFilter(watched, event);
}

} // namespace heart_boom_maths::game
