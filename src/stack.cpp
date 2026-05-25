#include "../include/stack.h"
#include <stdexcept>

void UndoRedoStack::pushInsert(int position, const std::string& text) {
    undoStack.push({position, text});
    while (!redoStack.empty()) {
        redoStack.pop();
    }
}

std::pair<int, std::string> UndoRedoStack::undo() {
    if (undoStack.empty()) {
        throw std::runtime_error("Nothing to undo");
    }
    
    auto action = undoStack.top();
    undoStack.pop();
    redoStack.push(action);
    
    return action;
}

std::pair<int, std::string> UndoRedoStack::redo() {
    if (redoStack.empty()) {
        throw std::runtime_error("Nothing to redo");
    }
    
    auto action = redoStack.top();
    redoStack.pop();
    undoStack.push(action);
    
    return action;
}

std::pair<int, std::string> UndoRedoStack::undo(const std::string& currentState) {
    if (undoStack.empty()) {
        throw std::runtime_error("Nothing to undo");
    }

    auto action = undoStack.top();
    undoStack.pop();

    // Park where we are now, so redo has somewhere to return to.
    redoStack.push({action.first, currentState});

    return action;
}

std::pair<int, std::string> UndoRedoStack::redo(const std::string& currentState) {
    if (redoStack.empty()) {
        throw std::runtime_error("Nothing to redo");
    }

    auto action = redoStack.top();
    redoStack.pop();

    undoStack.push({action.first, currentState});

    return action;
}

void UndoRedoStack::clearRedo() {
    while (!redoStack.empty()) {
        redoStack.pop();
    }
}