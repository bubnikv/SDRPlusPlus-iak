#pragma once
#include <json.hpp>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

using nlohmann::json;

namespace config_detail {
    // Reported when a stored value can't be converted to the type the caller asked
    // for, i.e. a hand-edited or stale config file.
    void logTypeMismatch(std::string_view key, const char* what);

    // Lookup that never inserts. json::operator[] would leave a null behind on a
    // miss, which both bloats the file and flips the next run's contains() check.
    inline json* find(json& node, std::string_view key) {
        if (!node.is_object()) { return NULL; }
        const auto it = node.find(std::string(key));
        return (it == node.end()) ? NULL : &(*it);
    }

    // Numbers count as compatible across int/float, so a value stored as 0 isn't
    // considered corrupt just because the caller now hands it a double.
    inline bool typeCompatible(const json& a, const json& b) {
        if (a.is_number() && b.is_number()) { return true; }
        return a.type() == b.type();
    }

    // json's compatible-string trait does accept a string_view, but spell the
    // conversion out rather than lean on it: a view is the one thing that must
    // never end up stored by reference.
    inline json toJson(std::string_view value) { return json(std::string(value)); }
    template <class T>
    json toJson(const T& value) { return json(value); }

    inline bool setValue(json& node, std::string_view key, json value) {
        const std::string k(key);
        const auto it = node.find(k);
        if (it != node.end() && *it == value) { return false; }
        node[k] = std::move(value);
        return true;
    }

    inline bool ensureValue(json& node, std::string_view key, json def) {
        const std::string k(key);
        const auto it = node.find(k);
        if (it != node.end() && typeCompatible(*it, def)) { return false; }
        node[k] = std::move(def);
        return true;
    }

    // What value() hands back for a given default. Anything string-like becomes a
    // std::string: deducing the default's own type would return char[N] for a
    // literal, which is not a legal return type, and would return a view into the
    // document for a string_view, which dangles the moment the lock is dropped.
    template <class T>
    using ValueType = std::conditional_t<
        std::is_same<std::decay_t<T>, json>::value,
        json,
        std::conditional_t<
            std::is_constructible<std::string, const std::decay_t<T>&>::value,
            std::string,
            std::decay_t<T>>>;

    template <class T>
    bool getValue(json* node, std::string_view key, T& out) {
        json* v = node ? find(*node, key) : NULL;
        if (!v || v->is_null()) { return false; }
        try {
            out = v->get<T>();
        }
        catch (const std::exception& e) {
            logTypeMismatch(key, e.what());
            return false;
        }
        return true;
    }
}

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();
    void setPath(std::string file);
    void load(json def, bool lock = true);
    void save(bool lock = true);
    void enableAutoSave();
    void disableAutoSave();
    void acquire();
    void release(bool modified = false);

#ifndef NDEBUG
    // True if this thread currently holds the config lock. Exists only for the
    // assert in transaction(): the lock isn't recursive, so opening a transaction
    // while already holding it deadlocks. Debug-only, because tracking the owner
    // costs an atomic store on every acquire/release and nothing else reads it.
    bool heldByCurrentThread() const;
#endif

    // Scoped access to the document. Locks on construction, and on destruction
    // unlocks while reporting whether any of its writes actually changed
    // something. Unlike a bare acquire()/release() pair it survives an early
    // return or a throw out of nlohmann.
    class Transaction;
    // A lazily resolved section of the document, named by its path. Reads never
    // create the path; the first write materializes it.
    class Section;

    Transaction transaction();

    // Single-shot helpers, each holding the lock for the duration of one call:
    //   set("showMenu", showMenu);
    // They reach the top level of the document only. For anything nested, open a
    // transaction and name the path with section(), which keeps the path and the
    // key from being confused for one another. Every writer returns true iff it
    // modified the document.
    template <class T> bool set(std::string_view key, const T& value);
    template <class T> bool ensure(std::string_view key, const T& def);
    template <class T> bool tryGet(std::string_view key, T& out);
    template <class T>
    config_detail::ValueType<T> value(std::string_view key, const T& def);

    json conf;

private:
    void autoSaveWorker();

    std::string path = "";
    volatile bool changed = false;
    volatile bool autoSaveEnabled = false;
    std::thread autoSaveThread;
    std::mutex mtx;
#ifndef NDEBUG
    std::atomic<std::thread::id> owner{ std::thread::id() };
#endif

    std::mutex termMtx;
    std::condition_variable termCond;
    volatile bool termFlag = false;
};

class ConfigManager::Transaction {
    friend class ConfigManager;
    friend class ConfigManager::Section;

public:
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    Transaction& operator=(Transaction&&) = delete;

    Transaction(Transaction&& other) noexcept : mgr(other.mgr), changed(other.changed) {
        other.mgr = NULL;
    }

    ~Transaction() {
        if (mgr) { mgr->release(changed); }
    }

    // Writers, addressing the top level of the document. Nested keys go through
    // section().
    template <class T>
    bool set(std::string_view key, const T& value) {
        const bool wrote = config_detail::setValue(mgr->conf, key, config_detail::toJson(value));
        changed |= wrote;
        return wrote;
    }

    // Writes only when the key is missing or holds an incompatible type, which is
    // the "seed the defaults for a device we've never seen" case.
    template <class T>
    bool ensure(std::string_view key, const T& def) {
        const bool wrote = config_detail::ensureValue(mgr->conf, key, config_detail::toJson(def));
        changed |= wrote;
        return wrote;
    }

    bool erase(std::string_view key) {
        const bool wrote = mgr->conf.is_object() && mgr->conf.erase(std::string(key)) != 0;
        changed |= wrote;
        return wrote;
    }

    // Readers. Never insert, and leave the destination untouched when the key is
    // absent or holds the wrong type.
    template <class T>
    bool tryGet(std::string_view key, T& out) {
        return config_detail::getValue(&mgr->conf, key, out);
    }

    template <class T>
    config_detail::ValueType<T> value(std::string_view key, const T& def) {
        config_detail::ValueType<T> out(def);
        config_detail::getValue(&mgr->conf, key, out);
        return out;
    }

    bool contains(std::string_view key) {
        return config_detail::find(mgr->conf, key) != NULL;
    }

    // Names a path, e.g. section("devices", devName).set("gain", gainId). Binds a
    // prefix so a block of related fields reads and writes without repeating it,
    // and keeps path elements from being mistaken for a key or a value the way a
    // single variadic call would. A section must not outlive the transaction it
    // came from.
    template <class... Keys>
    Section section(Keys&&... keys);

    // Read-only escape hatch for what the readers don't cover, mainly iteration.
    // There is deliberately no mutable counterpart: every write goes through
    // set()/ensure()/erase(), which is what keeps modified() honest.
    const json& peek() const { return mgr->conf; }

    bool modified() const { return changed; }

private:
    explicit Transaction(ConfigManager* mgr) : mgr(mgr) {
        mgr->acquire();
    }

    ConfigManager* mgr;
    bool changed = false;
};

class ConfigManager::Section {
    friend class ConfigManager::Transaction;

public:
    template <class T>
    bool set(std::string_view key, const T& value) {
        const bool wrote = config_detail::setValue(materialize(), key, config_detail::toJson(value));
        txn->changed |= wrote;
        return wrote;
    }

    template <class T>
    bool ensure(std::string_view key, const T& def) {
        const bool wrote = config_detail::ensureValue(materialize(), key, config_detail::toJson(def));
        txn->changed |= wrote;
        return wrote;
    }

    bool erase(std::string_view key) {
        json* self = resolve();
        const bool wrote = self && self->is_object() && self->erase(std::string(key)) != 0;
        txn->changed |= wrote;
        return wrote;
    }

    template <class T>
    bool tryGet(std::string_view key, T& out) {
        return config_detail::getValue(resolve(), key, out);
    }

    template <class T>
    config_detail::ValueType<T> value(std::string_view key, const T& def) {
        config_detail::ValueType<T> out(def);
        config_detail::getValue(resolve(), key, out);
        return out;
    }

    bool contains(std::string_view key) {
        json* self = resolve();
        return self && config_detail::find(*self, key) != NULL;
    }

    // True if the path is already in the document, i.e. this device or section has
    // been seen before.
    bool exists() { return resolve() != NULL; }

    // Extends this section's path, so a nested block can be named in one step.
    template <class... Keys>
    Section section(Keys&&... keys) {
        Section sub(*this);
        sub.path.reserve(path.size() + sizeof...(Keys));
        (sub.path.emplace_back(std::forward<Keys>(keys)), ...);
        return sub;
    }

    // Read-only escape hatch: the subtree as it stands, or NULL when the path
    // doesn't exist. Never creates, so inspecting a section can't dirty the
    // document. Writes go through set()/ensure()/erase(); to replace a subtree
    // wholesale, build the json and hand it to set(), which compares first and
    // so keeps the change flag accurate.
    const json* peek() { return resolve(); }

private:
    Section(Transaction* txn, std::vector<std::string> path)
        : txn(txn), path(std::move(path)) {}

    // Walks the path without touching the document.
    json* resolve() {
        json* node = &txn->mgr->conf;
        for (const auto& key : path) {
            node = config_detail::find(*node, key);
            if (!node) { return NULL; }
        }
        return node;
    }

    // Walks the path, creating the objects it's missing. Anything found along the
    // way that isn't an object is replaced, which is how a corrupted section heals
    // itself.
    json& materialize() {
        json* node = &txn->mgr->conf;
        for (const auto& key : path) {
            if (!node->is_object() && !node->is_null()) {
                *node = json::object();
                txn->changed = true;
            }
            const auto it = node->find(key);
            if (it != node->end() && it->is_object()) {
                node = &(*it);
                continue;
            }
            (*node)[key] = json::object();
            txn->changed = true;
            node = &(*node)[key];
        }
        return *node;
    }

    Transaction* txn;
    std::vector<std::string> path;
};

template <class... Keys>
inline ConfigManager::Section ConfigManager::Transaction::section(Keys&&... keys) {
    std::vector<std::string> path;
    path.reserve(sizeof...(Keys));
    (path.emplace_back(std::forward<Keys>(keys)), ...);
    return Section(this, std::move(path));
}

inline ConfigManager::Transaction ConfigManager::transaction() {
    assert(!heldByCurrentThread() && "nested ConfigManager transaction deadlocks");
    return Transaction(this);
}

template <class T>
inline bool ConfigManager::set(std::string_view key, const T& value) {
    return transaction().set(key, value);
}

template <class T>
inline bool ConfigManager::ensure(std::string_view key, const T& def) {
    return transaction().ensure(key, def);
}

template <class T>
inline bool ConfigManager::tryGet(std::string_view key, T& out) {
    return transaction().tryGet(key, out);
}

template <class T>
inline config_detail::ValueType<T> ConfigManager::value(std::string_view key, const T& def) {
    return transaction().value(key, def);
}
