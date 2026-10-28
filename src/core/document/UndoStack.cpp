#include "document/UndoStack.h"

#include <numeric>

namespace paint {

UndoStack::UndoStack(QObject *parent)
    : QObject(parent)
{}

UndoStack::~UndoStack() = default;

void UndoStack::setLimits(int steps, qint64 bytes)
{
    m_stepLimit = std::max(1, steps);
    m_byteLimit = std::max<qint64>(0, bytes);
    const bool wasClean = isClean();
    trim();
    notify(wasClean);
}

void UndoStack::push(std::unique_ptr<UndoCommand> command)
{
    const bool wasClean = isClean();
    // A new step replaces what could have been redone.
    m_commands.erase(m_commands.begin() + m_index, m_commands.end());
    if (m_cleanIndex > m_index) {
        m_cleanIndex = -1;
    }
    m_commands.push_back(std::move(command));
    ++m_index;
    trim();
    notify(wasClean);
}

void UndoStack::undo()
{
    if (!canUndo()) {
        return;
    }
    const bool wasClean = isClean();
    m_commands[size_t(--m_index)]->undo();
    notify(wasClean);
}

void UndoStack::redo()
{
    if (!canRedo()) {
        return;
    }
    const bool wasClean = isClean();
    m_commands[size_t(m_index++)]->redo();
    notify(wasClean);
}

QString UndoStack::undoText() const
{
    return canUndo() ? m_commands[size_t(m_index - 1)]->text() : QString();
}

QString UndoStack::redoText() const
{
    return canRedo() ? m_commands[size_t(m_index)]->text() : QString();
}

qint64 UndoStack::cost() const
{
    return std::accumulate(m_commands.cbegin(), m_commands.cend(), qint64(0),
                           [](qint64 sum, const auto &command) { return sum + command->cost(); });
}

void UndoStack::setClean()
{
    const bool wasClean = isClean();
    m_cleanIndex = m_index;
    notify(wasClean);
}

void UndoStack::clear()
{
    const bool wasClean = isClean();
    m_commands.clear();
    m_index = 0;
    m_cleanIndex = 0;
    notify(wasClean);
}

void UndoStack::trim()
{
    // Only steps that have been applied go, oldest first, and never the newest one.
    while (m_index > 1 && (count() > m_stepLimit || cost() > m_byteLimit)) {
        m_commands.erase(m_commands.begin());
        --m_index;
        m_cleanIndex = m_cleanIndex > 0 ? m_cleanIndex - 1 : -1;
    }
}

void UndoStack::notify(bool wasClean)
{
    Q_EMIT indexChanged(m_index);
    if (isClean() != wasClean) {
        Q_EMIT cleanChanged(isClean());
    }
}

} // namespace paint
