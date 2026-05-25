
#include <cassert>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <fstream>

#include "../include/tst.h"
#include "../include/freq_store.h"
#include "../include/stack.h"
#include "../include/phrase_store.h"

using namespace std::chrono;

static void testPrefixSearchRespectsK() {
    TST tst;
    for (int i = 0; i < 1000; i++) {
        tst.insert("test" + std::to_string(i));
    }

    assert(tst.prefixSearch("test", 5).size() == 5);
    assert(tst.prefixSearch("test", 10).size() == 10);
    assert(tst.prefixSearch("test", 0).empty());

    tst.insert("zebra");
    assert(tst.prefixSearch("zeb", 10).size() == 1);

    std::cout << "  prefixSearch honours k                        ok\n";
}

static void testPrefixSearchStillCorrect() {
    // Early termination must not change *which* words come back: the in-order
    // walk is alphabetical, so a bounded search returns the alphabetically
    // first k - exactly what the old resize(k) produced.
    TST tst;
    for (const auto& w : {"apple", "apply", "applied", "apt", "banana"}) {
        tst.insert(w);
    }

    auto r = tst.prefixSearch("app", 10);
    assert(r.size() == 3);
    assert(r[0] == "apple");
    assert(r[1] == "applied");
    assert(r[2] == "apply");

    // A prefix that is itself a word must be included.
    tst.insert("app");
    auto r2 = tst.prefixSearch("app", 10);
    assert(r2.size() == 4);
    assert(r2[0] == "app");

    // getAllWords stays unbounded.
    std::vector<std::string> all;
    tst.getAllWords(all);
    assert(all.size() == 6);

    std::cout << "  bounded search returns the same words         ok\n";
}

static void testPrefixSearchIsNotLinearInMatches() {
    // The regression: 40k words share the prefix, we display 10. If the
    // traversal is bounded, lookup time is independent of how many match.
    TST tst;
    for (int i = 0; i < 40000; i++) {
        tst.insert("common" + std::to_string(i));
    }
    for (int i = 0; i < 10; i++) {
        tst.insert("rare" + std::to_string(i));
    }

    auto timeIt = [&](const std::string& prefix) {
        auto t0 = high_resolution_clock::now();
        for (int r = 0; r < 200; r++) {
            auto res = tst.prefixSearch(prefix, 10);
            if (res.size() != 10) std::abort();
        }
        auto t1 = high_resolution_clock::now();
        return duration_cast<nanoseconds>(t1 - t0).count() / 200.0 / 1000.0;
    };

    double common = timeIt("common");   // 40000 candidates
    double rare   = timeIt("rare");     // 10 candidates

    std::printf("  40000 matches: %6.2f us | 10 matches: %6.2f us\n", common, rare);

    // Before the fix this ratio was ~1000x. Allow generous headroom so the
    // test is not flaky on a loaded machine, while still failing loudly if the
    // unbounded traversal ever returns.
    assert(common < rare * 20 + 50.0);

    std::cout << "  lookup cost independent of match count        ok\n";
}

//FreqStore

static void testFreqStoreConcurrentBumps() {
    const std::string path = "/tmp/freq_store_regression.txt";
    std::remove(path.c_str());

    const int threads = 8;
    const int perThread = 2000;

    {
        FreqStore store(path);

        std::vector<std::thread> pool;
        for (int t = 0; t < threads; t++) {
            pool.emplace_back([&store, perThread] {
                for (int i = 0; i < perThread; i++) {
                    store.bump("shared", 1);
                    store.get("shared");        // concurrent readers
                }
            });
        }
        for (auto& th : pool) th.join();

        // Every increment must be present: no lost updates, no deadlock.
        assert(store.get("shared") == threads * perThread);
        std::cout << "  " << threads << " threads x " << perThread
                  << " bumps, no lost updates          ok\n";
    } // destructor stops the writer thread and flushes

    // The background writer must have persisted the final value.
    std::ifstream in(path);
    assert(in.is_open());
    std::string token; int freq = 0; bool found = false;
    while (in >> token >> freq) {
        if (token == "shared") { found = true; break; }
    }
    assert(found);
    assert(freq == threads * perThread);
    std::cout << "  background writer persisted final state       ok\n";

    std::remove(path.c_str());
}

static void testFreqStoreKeystrokePathDoesNotBlockOnDisk() {
    const std::string path = "/tmp/freq_store_latency.txt";
    std::remove(path.c_str());

    FreqStore store(path);
    for (int i = 0; i < 500; i++) {
        store.set("token" + std::to_string(i), i);
    }

    auto t0 = high_resolution_clock::now();
    for (int i = 0; i < 5000; i++) {
        store.bump("hot", 1);
    }
    auto t1 = high_resolution_clock::now();
    double perBump = duration_cast<nanoseconds>(t1 - t0).count() / 5000.0 / 1000.0;

    std::printf("  bump() cost: %.3f us\n", perBump);

    assert(perBump < 5.0);
    assert(store.get("hot") == 5000);

    std::cout << "  bump() stays off the disk path                ok\n";
    std::remove(path.c_str());
}

//Undo/redo

// Tokens can contain spaces (an accepted phrase); '|' appears in real code.
static void testPersistenceRoundTrip() {
    std::remove("/tmp/rt_freq.txt");
    { FreqStore fs("/tmp/rt_freq.txt"); fs.set("printf", 5); fs.set("while const", 7); }
    { FreqStore fs("/tmp/rt_freq.txt");
      assert(fs.get("printf") == 5);
      assert(fs.get("while const") == 7); }        // was dropped on load

    std::remove("/tmp/rt_phr.txt");
    { PhraseStore ps("/tmp/rt_phr.txt"); ps.addPhrase("if", "if(a||b){}"); ps.save(); }
    { PhraseStore ps("/tmp/rt_phr.txt");           // used to abort via stoi("")
      assert(ps.getTotalPhrases() == 1);
      assert(ps.getPhrases("if")[0].snippet == "if(a||b){}"); }

    std::cout << "  spaces and pipes survive a round-trip         ok\n";
    std::remove("/tmp/rt_freq.txt"); std::remove("/tmp/rt_phr.txt");
}

// A hand-edited file must be skipped, not fatal.
static void testCorruptRecordsAreSkipped() {
    { std::ofstream f("/tmp/rt_bad.txt"); f << "good 5\nnocount\nbad x\n"; }
    FreqStore fs("/tmp/rt_bad.txt");
    assert(fs.get("good") == 5);
    assert(fs.get("nocount") == 0);
    std::cout << "  corrupt records skipped, not fatal            ok\n";
    std::remove("/tmp/rt_bad.txt");
}

static void testUndoRedoRoundTrip() {
    UndoRedoStack st;

    std::string doc = "v1";
    st.pushInsert(0, doc);   doc = "v2";
    st.pushInsert(0, doc);   doc = "v3";

    // Walk backwards.
    auto a = st.undo(doc);   doc = a.second;
    assert(doc == "v2");
    auto b = st.undo(doc);   doc = b.second;
    assert(doc == "v1");
    assert(!st.canUndo());

    // ...and forwards again. This is what the no-arg redo could not do.
    auto c = st.redo(doc);   doc = c.second;
    assert(doc == "v2");
    auto d = st.redo(doc);   doc = d.second;
    assert(doc == "v3");
    assert(!st.canRedo());

    std::cout << "  undo/redo round-trips through history         ok\n";
}

static void testNewEditClearsRedo() {
    UndoRedoStack st;

    std::string doc = "v1";
    st.pushInsert(0, doc);  doc = "v2";

    auto a = st.undo(doc);  doc = a.second;
    assert(doc == "v1");
    assert(st.canRedo());

    // Editing after an undo abandons the redo branch.
    st.pushInsert(0, doc);
    assert(!st.canRedo());

    std::cout << "  a new edit discards the redo branch           ok\n";
}

int main() {
    std::cout << "\nTST\n";
    testPrefixSearchRespectsK();
    testPrefixSearchStillCorrect();
    testPrefixSearchIsNotLinearInMatches();

    std::cout << "\nFreqStore\n";
    testFreqStoreConcurrentBumps();
    testFreqStoreKeystrokePathDoesNotBlockOnDisk();

    std::cout << "\nPersistence\n";
    testPersistenceRoundTrip();
    testCorruptRecordsAreSkipped();

    std::cout << "\nUndoRedoStack\n";
    testUndoRedoRoundTrip();
    testNewEditClearsRedo();

    std::cout << "\nAll regression tests passed.\n\n";
    return 0;
}
