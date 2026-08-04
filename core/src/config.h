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

    // True if this thread currently holds the config lock. Only meant for
    // assertions: the lock isn't recursive, so opening a transaction while
    // already holding it deadlocks.
    bool heldByCurrentThread() const;

    // Scoped access to the document. Locks on construction, and on destruction
    // unlocks while reporting whether any of its writes actually changed
    // something. Unlike a bare acquire()/release() pair it survives an early
    // return or a throw out of nlohmann.
    class Transaction;
    // A lazily resolved position inside the document. Reads never create the
    // path; the first write materializes it.
    class Node;

    Transaction transaction();

    // Single-shot helpers, each holding the lock for the duration of one call.
    // The last argument is the value, anything before it is a path:
    //   set("showMenu", showMenu);
    //   set("devices", devName, "gain", gainId);
    // Every writer returns true iff it modified the document.
    template <class... Args> bool set(Args&&... pathThenValue);
    template <class... Args> bool ensure(Args&&... pathThenDefault);
    template <class... Args> bool getTo(Args&&... pathThenOut);
    template <class T>
    config_detail::ValueType<T> value(std::string_view key, const T& def);
    template <class A, class B, class... Rest>
    auto value(std::string_view key, A&& a, B&& b, Rest&&... rest);

    json conf;

private:
    void autoSaveWorker();

    std::string path = "";
    volatile bool changed = false;
    volatile bool autoSaveEnabled = false;
    std::thread autoSaveThread;
    std::mutex mtx;
    std::atomic<std::thread::id> owner{ std::thread::id() };

    std::mutex termMtx;
    std::condition_variable termCond;
    volatile bool termFlag = false;
};

class ConfigManager::Transaction {
    friend class ConfigManager;
    friend class ConfigManager::Node;

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

    // Writers. Two arguments are always key + value; three or more treat every
    // argument but the last as a path element.
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
    bool getTo(std::string_view key, T& out) {
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

    // Path forms, e.g. set("devices", devName, "gain", gainId).
    template <class A, class B, class... Rest>
    bool set(std::string_view key, A&& a, B&& b, Rest&&... rest);
    template <class A, class B, class... Rest>
    bool ensure(std::string_view key, A&& a, B&& b, Rest&&... rest);
    template <class A, class B, class... Rest>
    bool getTo(std::string_view key, A&& a, B&& b, Rest&&... rest);
    template <class A, class B, class... Rest>
    auto value(std::string_view key, A&& a, B&& b, Rest&&... rest);

    // Binds a path prefix so a block of related fields reads and writes without
    // repeating it. Must not outlive the transaction it came from.
    template <class... Keys>
    Node node(Keys&&... keys);

    // Escape hatch for what the helpers don't cover: iteration, arrays, wholesale
    // assignment. Pair any write through it with markChanged().
    json& conf() { return mgr->conf; }
    void markChanged() { changed = true; }
    bool modified() const { return changed; }

private:
    explicit Transaction(ConfigManager* mgr) : mgr(mgr) {
        mgr->acquire();
    }

    ConfigManager* mgr;
    bool changed = false;
};

class ConfigManager::Node {
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
    bool getTo(std::string_view key, T& out) {
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

    template <class A, class B, class... Rest>
    bool set(std::string_view key, A&& a, B&& b, Rest&&... rest) {
        return node(key).set(std::forward<A>(a), std::forward<B>(b), std::forward<Rest>(rest)...);
    }

    template <class A, class B, class... Rest>
    bool ensure(std::string_view key, A&& a, B&& b, Rest&&... rest) {
        return node(key).ensure(std::forward<A>(a), std::forward<B>(b), std::forward<Rest>(rest)...);
    }

    template <class A, class B, class... Rest>
    bool getTo(std::string_view key, A&& a, B&& b, Rest&&... rest) {
        return node(key).getTo(std::forward<A>(a), std::forward<B>(b), std::forward<Rest>(rest)...);
    }

    template <class A, class B, class... Rest>
    auto value(std::string_view key, A&& a, B&& b, Rest&&... rest) {
        return node(key).value(std::forward<A>(a), std::forward<B>(b), std::forward<Rest>(rest)...);
    }

    template <class... Keys>
    Node node(Keys&&... keys) {
        Node sub(*this);
        sub.path.reserve(path.size() + sizeof...(Keys));
        (sub.path.emplace_back(std::forward<Keys>(keys)), ...);
        return sub;
    }

    // Escape hatch. Creates the path, so only call it when about to write.
    json& conf() { return materialize(); }
    void markChanged() { txn->changed = true; }

    // Read-only escape hatch: the subtree as it stands, or NULL when the path
    // doesn't exist. Never creates.
    json* peek() { return resolve(); }

private:
    Node(Transaction* txn, std::vector<std::string> path)
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
inline ConfigManager::Node ConfigManager::Transaction::node(Keys&&... keys) {
    std::vector<std::string> path;
    path.reserve(sizeof...(Keys));
    (path.emplace_back(std::forward<Keys>(keys)), ...);
    return Node(this, std::move(path));
}

template <class A, class B, class... Rest>
inline bool ConfigManager::Transaction::set(std::string_view key, A&& a, B&& b, Rest&&... rest) {
    return node(key).set(std::forward<A>(a), std::forward<B>(b), std::forward<Rest>(rest)...);
}

template <class A, class B, class... Rest>
inline bool ConfigManager::Transaction::ensure(std::string_view key, A&& a, B&& b, Rest&&... rest) {
    return node(key).ensure(std::forward<A>(a), std::forward<B>(b), std::forward<Rest>(rest)...);
}

template <class A, class B, class... Rest>
inline bool ConfigManager::Transaction::getTo(std::string_view key, A&& a, B&& b, Rest&&... rest) {
    return node(key).getTo(std::forward<A>(a), std::forward<B>(b), std::forward<Rest>(rest)...);
}

template <class A, class B, class... Rest>
inline auto ConfigManager::Transaction::value(std::string_view key, A&& a, B&& b, Rest&&... rest) {
    return node(key).value(std::forward<A>(a), std::forward<B>(b), std::forward<Rest>(rest)...);
}

inline ConfigManager::Transaction ConfigManager::transaction() {
    assert(!heldByCurrentThread() && "nested ConfigManager transaction deadlocks");
    return Transaction(this);
}

template <class... Args>
inline bool ConfigManager::set(Args&&... pathThenValue) {
    return transaction().set(std::forward<Args>(pathThenValue)...);
}

template <class... Args>
inline bool ConfigManager::ensure(Args&&... pathThenDefault) {
    return transaction().ensure(std::forward<Args>(pathThenDefault)...);
}

template <class... Args>
inline bool ConfigManager::getTo(Args&&... pathThenOut) {
    return transaction().getTo(std::forward<Args>(pathThenOut)...);
}

template <class T>
inline config_detail::ValueType<T> ConfigManager::value(std::string_view key, const T& def) {
    return transaction().value(key, def);
}

template <class A, class B, class... Rest>
inline auto ConfigManager::value(std::string_view key, A&& a, B&& b, Rest&&... rest) {
    return transaction().value(key, std::forward<A>(a), std::forward<B>(b), std::forward<Rest>(rest)...);
}
