#ifndef STACK_H
#define STACK_H

#include <stack>
#include <string>
#include <utility>

class UndoRedoStack {
private:
    std::stack<std::pair<int, std::string>> undoStack;
    std::stack<std::pair<int, std::string>> redoStack;

public:
    void pushInsert(int position, const std::string& text);

    // Log-style undo, used by the CLI to report what happened.
    std::pair<int, std::string> undo();
    std::pair<int, std::string> redo();

    // State-swapping undo, for the editor. The no-arg version cannot redo:
    // it pushes back the state it just popped, so the document never moves
    // forward. These park the caller's current state on the opposite stack.
    std::pair<int, std::string> undo(const std::string& currentState);
    std::pair<int, std::string> redo(const std::string& currentState);
    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }
    void clearRedo();
};

#endif