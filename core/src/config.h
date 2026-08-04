#pragma once
#include <json.hpp>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using nlohmann::json;

class ConfigSaver;

namespace config_detail {
    // Reported when a stored value can't be converted to the type the caller asked
    // for, i.e. a hand-edited or stale config file.
    void logTypeMismatch(std::string_view key, const char* what);
    void logDefaultRepair(std::string_view key);

    // Lookup that never inserts. json::operator[] would leave a null behind on a
    // miss, which both bloats the file and flips the next run's contains() check.
    inline json* find(json& node, std::string_view key) {
        if (!node.is_object()) { return NULL; }
        const auto it = node.find(key);
        return (it == node.end()) ? NULL : &(*it);
    }

    inline const json* find(const json& node, std::string_view key) {
        if (!node.is_object()) { return NULL; }
        const auto it = node.find(key);
        return (it == node.end()) ? NULL : &(*it);
    }

    // Floating-point fields accept stored integers, but integer fields reject
    // stored floats so reads cannot silently truncate them.
    inline bool typeCompatible(const json& a, const json& b) {
        // Integer fields accept either signed or unsigned JSON integers, while a
        // floating-point default also accepts an integer. Do not accept a stored
        // float for an integer field: get<int>() would silently truncate it.
        if (b.is_number_float()) { return a.is_number(); }
        if (b.is_number_integer()) { return a.is_number_integer(); }
        return a.type() == b.type();
    }

    template <class T>
    bool integerFits(const json& value) {
        using Integer = std::decay_t<T>;
        if (value.is_number_unsigned()) {
            const std::uint64_t number = value.get<std::uint64_t>();
            return number <= static_cast<std::uint64_t>((std::numeric_limits<Integer>::max)());
        }
        if (!value.is_number_integer()) { return false; }
        const std::int64_t number = value.get<std::int64_t>();
        if constexpr (std::is_signed<Integer>::value) {
            return number >= static_cast<std::int64_t>((std::numeric_limits<Integer>::min)()) &&
                   number <= static_cast<std::int64_t>((std::numeric_limits<Integer>::max)());
        }
        return number >= 0 &&
               static_cast<std::uint64_t>(number) <=
                   static_cast<std::uint64_t>((std::numeric_limits<Integer>::max)());
    }

    // json's compatible-string trait does accept a string_view, but spell the
    // conversion out rather than lean on it: a view is the one thing that must
    // never end up stored by reference.
    inline json toJson(std::string_view value) { return json(std::string(value)); }
    template <class T>
    json toJson(const T& value) { return json(value); }

    inline bool setValue(json& node, std::string_view key, json value) {
        const auto it = node.find(key);
        if (it != node.end()) {
            if (*it == value) { return false; }
            *it = std::move(value);
            return true;
        }
        node[std::string(key)] = std::move(value);
        return true;
    }

    template <class T>
    bool ensureValue(json& node, std::string_view key, json def) {
        const auto it = node.find(key);
        bool compatible = it != node.end() && typeCompatible(*it, def);
        if constexpr (std::is_integral<std::decay_t<T>>::value &&
                      !std::is_same<std::decay_t<T>, bool>::value) {
            compatible = compatible && integerFits<T>(*it);
        }
        if (compatible) { return false; }
        if (it != node.end()) {
            logDefaultRepair(key);
            *it = std::move(def);
            return true;
        }
        node[std::string(key)] = std::move(def);
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
    bool getValue(const json* node, std::string_view key, T& out) {
        const json* v = node ? find(*node, key) : NULL;
        if (!v || v->is_null()) { return false; }
        try {
            if constexpr (std::is_integral<T>::value && !std::is_same<T, bool>::value) {
                if (!integerFits<T>(*v)) {
                    logTypeMismatch(key, "integer value is not representable by the destination type");
                    return false;
                }
            }
            out = v->get<T>();
        }
        catch (const std::exception& e) {
            logTypeMismatch(key, e.what());
            return false;
        }
        return true;
    }
}

namespace config_detail {
    struct ConfigManagerTestAccess;
}

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();
    void setPath(std::string file);
    void load(json def);
    bool save();
    void enableAutoSave();
    void disableAutoSave();
    // Synchronously saves every dirty manager currently registered for
    // autosave, without changing their lifecycle state. Intended for platforms
    // that may resume after a save-state event. It deliberately makes one save
    // attempt per manager, leaving background retry to the autosaver. Continues
    // after individual failures and returns true only if every attempted save
    // succeeded.
    static bool flushAll();
    // Terminal operation: rejects new access, drains the autosaver, and commits
    // the final dirty state. A failed shutdown may be retried, but the manager
    // remains frozen in between so only an external condition can unblock it.
    bool shutdown();

    // A read access owns the config mutex but exposes only operations that cannot
    // modify the document. An edit access adds writers and reports whether those
    // writers actually changed anything. Keep either object scoped as tightly as
    // possible; helpers should accept the existing capability by reference. No
    // ConfigManager may be entered while another config access is active on the
    // thread: copy values out and release the access before calling external code.
    class ReadAccess;
    class EditAccess;
    class ReadSection;
    class EditSection;

    ReadAccess read();
    EditAccess edit();

private:
    json conf;
    void acquire(ReadAccess* access);
    void release(ReadAccess* access, bool modified) noexcept;
    bool anyAccessHeldByCurrentThread() const noexcept;
    void disableAutoSaveImpl();

    enum class SaveRetryMode {
        SingleAttempt,
        TransientWindowsLocks
    };

    // Called only by the one process-wide ConfigSaver. The dirty flag is guarded
    // by mtx and is cleared only after a successful disk commit.
    bool saveIfDirty();
    bool saveImpl(bool onlyIfDirty, bool allowClosing = false,
                  SaveRetryMode retryMode = SaveRetryMode::SingleAttempt);

    friend class ConfigSaver;
    friend struct config_detail::ConfigManagerTestAccess;

    std::string path = "";
    enum class LifecycleState {
        Open,
        Closing,
        Closed
    };
    LifecycleState lifecycleState = LifecycleState::Open;
    // Baseline used to identify local changes during a multi-instance merge. It
    // may be a default document whose first write failed, hence it is not named
    // "persisted".
    json baseline;
    bool baselineValid = false;
    bool dirty = false;
    // Generation observed only while mtx is held. Edits, load() and setPath()
    // advance it. saveImpl() may reconcile conf without advancing it because
    // persistenceMtx excludes every other observer of that transition.
    std::uint64_t revision = 0;
    std::atomic<bool> autoSaveEnabled{ false };
    std::mutex mtx;
    // Serializes persistence attempts for this manager, but is never taken by a
    // read or edit access. Disk I/O therefore cannot block in-memory config use.
    std::mutex persistenceMtx;
};

class ConfigManager::ReadAccess {
    friend class ConfigManager;
    friend class ConfigManager::EditAccess;
    friend class ConfigManager::ReadSection;
    friend class ConfigManager::EditSection;

public:
    ReadAccess(const ReadAccess&) = delete;
    ReadAccess& operator=(const ReadAccess&) = delete;
    ReadAccess(ReadAccess&&) = delete;
    ReadAccess& operator=(ReadAccess&&) = delete;

    ~ReadAccess() {
        if (lifetime) { lifetime->active.store(false, std::memory_order_release); }
        mgr->release(this, changed);
    }

    template <class T>
    bool tryGet(std::string_view key, T& out) const {
        return config_detail::getValue(&mgr->conf, key, out);
    }

    template <class T>
    config_detail::ValueType<T> value(std::string_view key, const T& def) const {
        config_detail::ValueType<T> out(def);
        config_detail::getValue(&mgr->conf, key, out);
        return out;
    }

    bool contains(std::string_view key) const {
        return config_detail::find(mgr->conf, key) != NULL;
    }

    template <class... Keys>
    ReadSection section(Keys&&... keys) const;

    // Read-only escape hatch for iteration and other queries not covered above.
    const json& peek() const { return mgr->conf; }

private:
    struct Lifetime {
        std::atomic<bool> active{ true };
    };

    explicit ReadAccess(ConfigManager* mgr) : mgr(mgr) {
        mgr->acquire(this);
    }

    std::shared_ptr<Lifetime> lifetimeForSection() const {
        if (!lifetime) { lifetime = std::make_shared<Lifetime>(); }
        return lifetime;
    }

    ConfigManager* mgr;
    // Always false for a plain ReadAccess. EditAccess writers set it and the
    // shared destructor consumes it to update dirty/revision state on release.
    bool changed = false;
    mutable std::shared_ptr<Lifetime> lifetime;
};

class ConfigManager::EditAccess : public ConfigManager::ReadAccess {
    friend class ConfigManager;
    friend class ConfigManager::EditSection;

public:
    EditAccess(const EditAccess&) = delete;
    EditAccess& operator=(const EditAccess&) = delete;
    EditAccess(EditAccess&&) = delete;
    EditAccess& operator=(EditAccess&&) = delete;

    template <class T>
    bool set(std::string_view key, const T& value) {
        const bool wrote = config_detail::setValue(
            mgr->conf, key, config_detail::toJson(value));
        changed |= wrote;
        return wrote;
    }

    // Installs a missing default or explicitly repairs an incompatible value.
    template <class T>
    bool ensure(std::string_view key, const T& def) {
        const bool wrote = config_detail::ensureValue<T>(
            mgr->conf, key, config_detail::toJson(def));
        changed |= wrote;
        return wrote;
    }

    bool erase(std::string_view key) {
        bool wrote = false;
        if (mgr->conf.is_object()) {
            const auto it = mgr->conf.find(key);
            if (it != mgr->conf.end()) {
                mgr->conf.erase(it);
                wrote = true;
            }
        }
        changed |= wrote;
        return wrote;
    }

    bool reset(json doc) {
        const bool wrote = mgr->conf != doc;
        if (wrote) { mgr->conf = std::move(doc); }
        changed |= wrote;
        return wrote;
    }

    using ReadAccess::section;
    template <class... Keys>
    EditSection section(Keys&&... keys);

private:
    explicit EditAccess(ConfigManager* mgr) : ReadAccess(mgr) {}
};

class ConfigManager::ReadSection {
    friend class ConfigManager::ReadAccess;
    friend class ConfigManager::EditAccess;
    friend class ConfigManager::EditSection;

public:
    template <class T>
    bool tryGet(std::string_view key, T& out) const {
        return config_detail::getValue(resolve(), key, out);
    }

    template <class T>
    config_detail::ValueType<T> value(std::string_view key, const T& def) const {
        config_detail::ValueType<T> out(def);
        config_detail::getValue(resolve(), key, out);
        return out;
    }

    bool contains(std::string_view key) const {
        const json* self = resolve();
        return self && config_detail::find(*self, key) != NULL;
    }

    bool exists() const { return resolve() != NULL; }

    template <class... Keys>
    ReadSection section(Keys&&... keys) const {
        std::vector<std::string> subPath = path;
        subPath.reserve(path.size() + sizeof...(Keys));
        (subPath.emplace_back(std::forward<Keys>(keys)), ...);
        return ReadSection(access, lifetime, std::move(subPath));
    }

    const json* peek() const { return resolve(); }

private:
    ReadSection(const ReadAccess* access,
                std::shared_ptr<ReadAccess::Lifetime> lifetime,
                std::vector<std::string> path)
        : access(access), lifetime(std::move(lifetime)), path(std::move(path)) {}

    const ReadAccess& checkedAccess() const {
        if (!lifetime || !lifetime->active.load(std::memory_order_acquire)) {
            throw std::logic_error("ConfigManager section outlived its access");
        }
        return *access;
    }

    const json* resolve() const {
        return resolve(checkedAccess());
    }

    const json* resolve(const ReadAccess& owner) const {
        const json* node = &owner.mgr->conf;
        for (const auto& key : path) {
            node = config_detail::find(*node, key);
            if (!node) { return NULL; }
        }
        return node;
    }

    const ReadAccess* access;
    std::shared_ptr<ReadAccess::Lifetime> lifetime;
    std::vector<std::string> path;
};

class ConfigManager::EditSection : public ConfigManager::ReadSection {
    friend class ConfigManager::EditAccess;

public:
    template <class T>
    bool set(std::string_view key, const T& value) {
        EditAccess& owner = checkedEditor();
        const bool wrote = config_detail::setValue(
            materialize(owner), key, config_detail::toJson(value));
        owner.changed |= wrote;
        return wrote;
    }

    template <class T>
    bool ensure(std::string_view key, const T& def) {
        EditAccess& owner = checkedEditor();
        const bool wrote = config_detail::ensureValue<T>(
            materialize(owner), key, config_detail::toJson(def));
        owner.changed |= wrote;
        return wrote;
    }

    bool erase(std::string_view key) {
        EditAccess& owner = checkedEditor();
        json* self = resolveMutable(owner);
        bool wrote = false;
        if (self && self->is_object()) {
            const auto it = self->find(key);
            if (it != self->end()) {
                self->erase(it);
                wrote = true;
            }
        }
        owner.changed |= wrote;
        return wrote;
    }

    using ReadSection::section;
    template <class... Keys>
    EditSection section(Keys&&... keys) {
        std::vector<std::string> subPath = path;
        subPath.reserve(path.size() + sizeof...(Keys));
        (subPath.emplace_back(std::forward<Keys>(keys)), ...);
        return EditSection(&checkedEditor(), lifetime, std::move(subPath));
    }

private:
    EditSection(EditAccess* editor,
                std::shared_ptr<ReadAccess::Lifetime> lifetime,
                std::vector<std::string> path)
        : ReadSection(editor, std::move(lifetime), std::move(path)) {}

    EditAccess& checkedEditor() const {
        // Private construction always starts with an EditAccess*. Nested edit
        // sections preserve that invariant by deriving their owner here.
        return const_cast<EditAccess&>(
            static_cast<const EditAccess&>(checkedAccess()));
    }

    json* resolveMutable(EditAccess& owner) const {
        // The shared resolver exposes a const view, but an EditAccess owns the
        // underlying non-const document.
        return const_cast<json*>(resolve(owner));
    }

    json& materialize(EditAccess& owner) {
        json* node = &owner.mgr->conf;
        for (const auto& key : path) {
            if (!node->is_object() && !node->is_null()) {
                *node = json::object();
                owner.changed = true;
            }
            json& child = (*node)[key];
            if (!child.is_object()) {
                child = json::object();
                owner.changed = true;
            }
            node = &child;
        }
        return *node;
    }
};

template <class... Keys>
inline ConfigManager::ReadSection ConfigManager::ReadAccess::section(Keys&&... keys) const {
    std::vector<std::string> path;
    path.reserve(sizeof...(Keys));
    (path.emplace_back(std::forward<Keys>(keys)), ...);
    return ReadSection(this, lifetimeForSection(), std::move(path));
}

template <class... Keys>
inline ConfigManager::EditSection ConfigManager::EditAccess::section(Keys&&... keys) {
    std::vector<std::string> path;
    path.reserve(sizeof...(Keys));
    (path.emplace_back(std::forward<Keys>(keys)), ...);
    return EditSection(this, lifetimeForSection(), std::move(path));
}

inline ConfigManager::ReadAccess ConfigManager::read() {
    return ReadAccess(this);
}

inline ConfigManager::EditAccess ConfigManager::edit() {
    return EditAccess(this);
}
