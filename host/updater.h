// Finding out that a newer version exists, and installing it.
//
// The builds are downloaded rather than installed from a store, so they have
// to notice a new release themselves. Everything here runs on its own thread:
// a check that blocks the frame loop is a check that stutters the music, and
// this is the least important thing the app does.
#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace ne {

enum class UpdateStage {
    Idle,
    Checking,
    UpToDate,
    CheckFailed,
    Available,
    Downloading,
    Ready,
    Failed,
};

// True when `candidate` is a strictly newer version than `current`.
//
// Compared component by component as numbers, not as text. The bug this exists
// to prevent: CI's patch number is the run number, so it goes past 9 on the
// tenth push, and string ordering would then stop offering updates for good.
// A missing component counts as zero; anything unparseable is not newer, so a
// malformed tag can fail in one direction only.
bool is_newer_version(const std::string& candidate, const std::string& current);

class Updater {
 public:
    // `current` is empty in a build CI did not cut, which disables the whole
    // thing: there is nothing to compare against a release, so "up to date"
    // would be a guess.
    explicit Updater(std::string current);
    ~Updater();

    bool enabled() const { return !current_.empty(); }
    bool automatic() const { return auto_; }
    void set_automatic(bool on) { auto_ = on; }

    UpdateStage stage() const { return stage_.load(); }
    float progress() const { return progress_.load(); }
    std::string latest() const;
    std::string status_line() const;

    // Both return immediately; the work happens on a thread.
    void check(bool force = false);
    void install();

 private:
    void join_worker();

    std::string current_;
    std::atomic<bool> auto_{true};
    std::atomic<UpdateStage> stage_{UpdateStage::Idle};
    std::atomic<float> progress_{0};
    std::atomic<bool> cancel_{false};

    mutable std::mutex m_;
    std::string latest_;
    std::string asset_url_;
    std::string handoff_;

    std::thread worker_;
};

}  // namespace ne
