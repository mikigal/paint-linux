#pragma once

#include <QObject>
#include <QString>

#include <memory>
#include <vector>

namespace paint {

// One step of the undo history. It is recorded after the change was made, so the stack never
// calls redo() when it is pushed.
class UndoCommand
{
public:
    explicit UndoCommand(QString text)
        : m_text(std::move(text))
    {}
    virtual ~UndoCommand() = default;
    UndoCommand(const UndoCommand &) = delete;
    UndoCommand &operator=(const UndoCommand &) = delete;

    virtual void undo() = 0;
    virtual void redo() = 0;
    // The memory the step holds on to, in bytes.
    virtual qint64 cost() const = 0;
    QString text() const { return m_text; }

private:
    QString m_text;
};

// An undo stack in the manner of QUndoStack that also stays within a memory budget: the oldest
// steps go once it holds more than its step limit or its byte limit (the newest step always stays).
class UndoStack : public QObject
{
    Q_OBJECT

public:
    explicit UndoStack(QObject *parent = nullptr);
    ~UndoStack() override;

    void setLimits(int steps, qint64 bytes);
    int stepLimit() const { return m_stepLimit; }
    qint64 byteLimit() const { return m_byteLimit; }

    // Takes a step that has already been applied; steps that could have been redone are dropped.
    void push(std::unique_ptr<UndoCommand> command);
    void undo();
    void redo();
    bool canUndo() const { return m_index > 0; }
    bool canRedo() const { return m_index < count(); }
    // Steps held, including those that can be redone; index() of them are applied.
    int count() const { return int(m_commands.size()); }
    int index() const { return m_index; }
    QString undoText() const;
    QString redoText() const;
    // Memory the steps hold, in bytes.
    qint64 cost() const;

    // The state that matches the saved file; it can be lost when the steps leading to it go.
    bool isClean() const { return m_index == m_cleanIndex; }
    void setClean();
    void clear();

Q_SIGNALS:
    void indexChanged(int index);
    void cleanChanged(bool clean);

private:
    void trim();
    void notify(bool wasClean);

    std::vector<std::unique_ptr<UndoCommand>> m_commands;
    int m_index = 0;
    int m_cleanIndex = 0; // -1: no reachable step matches the saved file
    int m_stepLimit = 100;
    qint64 m_byteLimit = 1024LL * 1024 * 1024;
};

} // namespace paint
