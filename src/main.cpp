/*
 * Command-line interface for testing the Smart Autocomplete Engine.
 *
 * This file provides a simple REPL-based interface to interact with the
 * autocomplete system without any UI dependencies. It was used to develop,
 * debug, and validate the core autocomplete logic before integrating it into
 * the ncurses-based editor.
 *
 * The CLI allows:
 * - prefix-based token suggestions
 * - frequency- and context-aware ranking
 * - phrase learning and persistence
 * - undo/redo of accepted tokens
 * - optional substring matching using KMP
 * - inspection of the co-occurrence graph
 *
 * This file is not required for running the terminal editor, but is retained
 * as a lightweight testing and demonstration tool for the autocomplete engine.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include "../include/tst.h"
#include "../include/minheap.h"
#include "../include/lru.h"
#include "../include/kmp.h"
#include "../include/stack.h"
#include "../include/graph.h"
#include "../include/freq_store.h"
#include "../include/ranker.h"
#include "../include/phrase_store.h"

class AutocompleteEngine {
private:
    TST tst;
    LRUCache cache;
    FreqStore freqStore;
    CooccurrenceGraph graph;
    Ranker ranker;
    UndoRedoStack undoRedo;
    PhraseStore phraseStore;
    std::string lastAccepted;
    bool useSubstringSearch;
    bool usePhraseCompletion;

    void loadSeeds(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()){
            std::cerr << "Warning: Could not open " << filename << std::endl;
            return;
        }

        std::string word;
        int count = 0;
        while (file >> word) {
            if (!word.empty()) {
                tst.insert(word);
                count++;
            }
        }
        file.close();
        std::cout << "Loaded " << count << " tokens from seed file." << std::endl;
    }

    std::vector<std::string> substringSearch(const std::string& prefix){
        std::vector<std::string> allWords;
        tst.getAllWords(allWords);

        std::vector<std::string> results;
        for (const auto& word : allWords) {
            if (KMP::contains(word, prefix)) {
                results.push_back(word);
            }
        }

        return results;
    }


public:
    AutocompleteEngine()
        : cache(50),
        freqStore("data/frequency.txt"),
        phraseStore("data/phrases.txt"),
        ranker(&freqStore, &graph),
        useSubstringSearch(false),
        usePhraseCompletion(true){

        loadSeeds("data/words.txt");
        std::cout << "Loaded " << phraseStore.getTotalPhrases() << " learned phrases." << std::endl;
    }

    std::vector<std::pair<std::string, double>> getSuggestions(const std::string& prefix, int k = 5){
        if (prefix.empty()) {
            return std::vector<std::pair<std::string, double>>();
        }

        if (cache.exists(prefix)) {
            auto cached = cache.get(prefix);
            std::vector<std::pair<std::string, double>> result;
            for (const auto& token : cached) {
                result.push_back(std::make_pair(token, freqStore.get(token)));
            }
            return result;
        }

        std::vector<std::string> candidates = tst.prefixSearch(prefix, k * 2);

        if (candidates.size()<3 && useSubstringSearch) {
            auto substringResults = substringSearch(prefix);
            candidates.insert(candidates.end(), substringResults.begin(), substringResults.end());
            
            std::sort(candidates.begin(), candidates.end());
            candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
        }

        ranker.setLastToken(lastAccepted);
        auto ranked = ranker.rankResults(candidates, k);

        std::vector<std::string> toCache;
        for (const auto& [token, score] : ranked){
            toCache.push_back(token);
        }
        cache.put(prefix, toCache);

        return ranked;
    }

    void acceptSuggestion(const std::string& token){
        freqStore.bump(token, 1);
        
        if (!lastAccepted.empty()){
            graph.addEdge(lastAccepted, token);
        }
        
        undoRedo.pushInsert(0, token);
        lastAccepted = token;
        
        std::cout << "Accepted: "<< token << std::endl;
    }

    void bumpToken(const std::string& token) {
        freqStore.bump(token, 5);
        std::cout << "Bumped frequency of '" << token << "' by 5" << std::endl;
    }

    void performUndo() {
        if (undoRedo.canUndo()) {
            auto [pos, token] = undoRedo.undo();
            std::cout << "Undo: Removed '" << token << "'" << std::endl;
        } else {
            std::cout << "Nothing to undo" << std::endl;
        }
    }

    void performRedo() {
        if (undoRedo.canRedo()) {
            auto [pos, token] = undoRedo.redo();
            std::cout << "Redo: Restored '" << token << "'" << std::endl;
        } else {
            std::cout << "Nothing to redo" << std::endl;
        }
    }

    void toggleSubstringSearch() {
        useSubstringSearch = !useSubstringSearch;
        std::cout << "Substring search: " << (useSubstringSearch ? "ON" : "OFF") << std::endl;
    }

    void togglePhraseCompletion() {
        usePhraseCompletion = !usePhraseCompletion;
        std::cout << "Phrase completion: " << (usePhraseCompletion ? "ON" : "OFF") << std::endl;
    }

    void learnPhrase(const std::string& trigger, const std::string& fullText) {
        phraseStore.addPhrase(trigger, fullText);
        std::cout << "Learned phrase: \"" << fullText << "\" for trigger \"" << trigger << "\"" << std::endl;
    }

    std::vector<std::string> getPhraseSuggestions(const std::string& prefix) {
        std::vector<std::string> suggestions;

        if (!usePhraseCompletion) {
            return suggestions;
        }

        // Get learned phrases for this prefix
        auto phrases = phraseStore.getTopPhrases(prefix, 3);

        for (const auto& phrase : phrases) {
            suggestions.push_back(phrase.snippet);
        }

        return suggestions;
    }

    void displayGraph() {
        graph.display();
    }

    void savePhrases() {
        phraseStore.save();
        std::cout << "Saved " << phraseStore.getTotalPhrases() << " learned phrases." << std::endl;
    }

    void showHelp() {
        std::cout << "\n=== Smart Autocomplete Engine ===" << std::endl;
        std::cout << "\nCommands:" << std::endl;
        std::cout << ":help - Show this help message" << std::endl;
        std::cout << ":exit or :q - Exit the program" << std::endl;
        std::cout << ":bump <token> - Increase frequency of a token" << std::endl;
        std::cout << ":undo - Undo last accepted token" << std::endl;
        std::cout << ":redo - Redo last undone token" << std::endl;
        std::cout << ":toggle_contains - Toggle substring search" << std::endl;
        std::cout << ":toggle_phrases - Toggle phrase completion" << std::endl;
        std::cout << ":learn <trigger> <full_text> - Manually teach a phrase" << std::endl;
        std::cout << ":graph - Display co-occurrence graph" << std::endl;
        std::cout << ":save - Save learned phrases to disk" << std::endl;
        std::cout << "\nUsage:" << std::endl;
        std::cout << " - Type a prefix to get suggestions" << std::endl;
        std::cout << " - Select by number or type the full token" << std::endl;
        std::cout << " - After accepting, type full code (e.g., for(i=0;i<n;i++))" << std::endl;
        std::cout << " - Press Enter to learn the phrase, or skip" << std::endl;
        std::cout << std::endl;
    }
};

int main() {
    AutocompleteEngine engine;
    
    std::cout << "\n=== Smart Autocomplete Engine ===" << std::endl;
    std::cout << "Type ':help' for commands" << std::endl;
    std::cout << "Type ':exit' or ':q' to quit\n" << std::endl;

    std::string input;
    
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);

        if (input.empty()) {
            continue;
        }

        if (input == ":exit" || input == ":q") {
            std::cout << "Goodbye!" << std::endl;
            break;
        }

        
        if (input == ":help") {
            engine.showHelp();
            continue;
        }

        if (input == ":undo") {
            engine.performUndo();
            continue;
        }

        if (input == ":redo") {
            engine.performRedo();
            continue;
        }

        if (input == ":toggle_contains") {
            engine.toggleSubstringSearch();
            continue;
        }

        if (input == ":toggle_phrases") {
            engine.togglePhraseCompletion();
            continue;
        }

        if (input == ":graph") {
            engine.displayGraph();
            continue;
        }

        if (input == ":save") {
            engine.savePhrases();
            continue;
        }

        if (input.substr(0, 7) == ":learn ") {
            std::string rest = input.substr(7);
            size_t spacePos = rest.find(' ');
            if (spacePos != std::string::npos) {
                std::string trigger = rest.substr(0, spacePos);
                std::string fullText = rest.substr(spacePos + 1);
                engine.learnPhrase(trigger, fullText);
            } else {
                std::cout << "Usage: :learn <trigger> <full_text>" << std::endl;
            }
            continue;
        }

        if (input.substr(0, 6) == ":bump ") {
            std::string token = input.substr(6);
            engine.bumpToken(token);
            continue;
        }

        auto suggestions = engine.getSuggestions(input, 5);
        auto phraseSuggestions = engine.getPhraseSuggestions(input);

        if (suggestions.empty() && phraseSuggestions.empty()) {
            std::cout << "No suggestions found for '" << input << "'" << std::endl;
            continue;
        }

        std::cout << "\nSuggestions:" << std::endl;

        // Show learned phrases first (marked with [PHRASE])
        int index = 1;
        for (const auto& phrase : phraseSuggestions) {
            std::cout << "  " << index++ << ". [PHRASE] " << phrase << std::endl;
        }

        // Then show regular token suggestions
        for (size_t i = 0; i < suggestions.size(); i++) {
            std::cout << "  " << index++ << ". " << suggestions[i].first
                    << " (score=" << suggestions[i].second << ")" << std::endl;
        }

        std::cout << "\nAccept by number or token (or press Enter to skip): ";
        std::string choice;
        std::getline(std::cin, choice);

        if (choice.empty()) {
            continue;
        }

        std::string acceptedToken;
        bool isPhrase = false;

        if (isdigit(choice[0]) && choice.length() <= 2) {
            int num = std::stoi(choice);
            int totalSuggestions = phraseSuggestions.size() + suggestions.size();

            if (num >= 1 && num <= totalSuggestions) {
                if (num <= phraseSuggestions.size()) {
                    // Selected a phrase
                    acceptedToken = phraseSuggestions[num - 1];
                    isPhrase = true;
                    std::cout << "Accepted phrase: " << acceptedToken << std::endl;
                } else {
                    // Selected a regular token
                    int tokenIndex = num - phraseSuggestions.size() - 1;
                    acceptedToken = suggestions[tokenIndex].first;
                    engine.acceptSuggestion(acceptedToken);

                    // Prompt for phrase learning
                    std::cout << "\nType complete code for '" << acceptedToken
                            << "' (or press Enter to skip learning): ";
                    std::string fullCode;
                    std::getline(std::cin, fullCode);

                    if (!fullCode.empty() && fullCode != acceptedToken) {
                        engine.learnPhrase(acceptedToken, fullCode);
                    }
                }
            } else {
                std::cout << "Invalid selection" << std::endl;
            }
        }
        else{
            // Manual token entry
            bool found = false;
            for (const auto& [token, score] : suggestions) {
                if (token == choice) {
                    engine.acceptSuggestion(token);
                    acceptedToken = token;
                    found = true;

                    // Prompt for phrase learning
                    std::cout << "\nType complete code for '" << acceptedToken
                            << "' (or press Enter to skip learning): ";
                    std::string fullCode;
                    std::getline(std::cin, fullCode);

                    if (!fullCode.empty() && fullCode != acceptedToken) {
                        engine.learnPhrase(acceptedToken, fullCode);
                    }
                    break;
                }
            }
            if (!found) {
                std::cout << "Token not in suggestions" << std::endl;
            }
        }

        std::cout << std::endl;
    }

    return 0;
}