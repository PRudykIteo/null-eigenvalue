#include "updater.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "net.h"

namespace ne {
namespace {

const char kRepo[] = "doctorspider42/null-eigenvalue";

// Fixed asset names, with the version in the release title, so
// /releases/latest/download/<name> is a stable URL.
#if defined(_WIN32)
const char kAsset[] = "NullEigenvalue-Setup.exe";
#elif defined(__APPLE__)
const char kAsset[] = "NullEigenvalue.dmg";
#else
const char kAsset[] = "NullEigenvalue-x86_64.AppImage";
#endif

// Just enough JSON to read two strings out of a release. A parser is not worth
// a dependency here: the two fields wanted are flat, and anything unexpected
// has to end as "could not check" regardless of why.
std::string json_string(const std::string& doc, const std::string& key,
                        size_t from = 0) {
    const std::string needle = "\"" + key + "\"";
    size_t at = doc.find(needle, from);
    if (at == std::string::npos) return std::string();
    at = doc.find(':', at + needle.size());
    if (at == std::string::npos) return std::string();
    while (at < doc.size() && (doc[at] == ':' || doc[at] == ' ')) ++at;
    if (at >= doc.size() || doc[at] != '"') return std::string();
    ++at;
    std::string out;
    while (at < doc.size() && doc[at] != '"') {
        if (doc[at] == '\\' && at + 1 < doc.size()) ++at;
        out += doc[at++];
    }
    return out;
}

// The download URL of the one asset this platform wants.
std::string asset_url(const std::string& doc) {
    const std::string want = std::string("\"name\":\"") + kAsset + "\"";
    // The API emits assets as objects with name and browser_download_url in
    // them; find the wanted name and read the URL after it.
    size_t at = doc.find(want);
    if (at == std::string::npos) {
        // Some responses put whitespace in. Fall back to a looser search.
        at = doc.find(kAsset);
        if (at == std::string::npos) return std::string();
    }
    return json_string(doc, "browser_download_url", at);
}

std::vector<long> parts_of(const std::string& v) {
    std::vector<long> out;
    size_t i = 0;
    while (i < v.size()) {
        size_t j = i;
        while (j < v.size() && v[j] >= '0' && v[j] <= '9') ++j;
        if (j == i) return std::vector<long>();  // not a number: unparseable
        out.push_back(std::strtol(v.substr(i, j - i).c_str(), nullptr, 10));
        if (j >= v.size()) break;
        if (v[j] != '.') return std::vector<long>();  // trailing rubbish
        i = j + 1;
    }
    return out;
}

std::string prefs_dir() {
    char* base = SDL_GetPrefPath("nulleigenvalue", "NullEigenvalue");
    if (!base) return std::string();
    std::string p(base);
    SDL_free(base);
    return p;
}

}  // namespace

bool is_newer_version(const std::string& candidate, const std::string& current) {
    std::string a = candidate, b = current;
    if (!a.empty() && (a[0] == 'v' || a[0] == 'V')) a = a.substr(1);
    if (!b.empty() && (b[0] == 'v' || b[0] == 'V')) b = b.substr(1);
    const std::vector<long> pa = parts_of(a), pb = parts_of(b);
    if (pa.empty() || pb.empty()) return false;
    const size_t n = pa.size() > pb.size() ? pa.size() : pb.size();
    for (size_t i = 0; i < n; ++i) {
        const long x = i < pa.size() ? pa[i] : 0;
        const long y = i < pb.size() ? pb[i] : 0;
        if (x != y) return x > y;
    }
    return false;
}

Updater::Updater(std::string current) : current_(std::move(current)) {}

Updater::~Updater() {
    cancel_.store(true);
    join_worker();
}

void Updater::join_worker() {
    if (worker_.joinable()) worker_.join();
}

std::string Updater::latest() const {
    std::lock_guard<std::mutex> g(m_);
    return latest_;
}

std::string Updater::status_line() const {
    switch (stage_.load()) {
        case UpdateStage::Idle:
            return auto_.load() ? "NOT CHECKED YET" : "AUTOMATIC IS OFF";
        case UpdateStage::Checking:
            return "CHECKING";
        case UpdateStage::UpToDate:
            return "UP TO DATE";
        case UpdateStage::CheckFailed:
            return "COULD NOT REACH GITHUB";
        case UpdateStage::Available:
            return latest() + " IS AVAILABLE";
        case UpdateStage::Downloading: {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "DOWNLOADING %d%%",
                          (int)(progress_.load() * 100.0f));
            return std::string(buf);
        }
        case UpdateStage::Ready: {
            std::lock_guard<std::mutex> g(m_);
            return handoff_.empty() ? "RESTARTING" : handoff_;
        }
        case UpdateStage::Failed:
            return "DOWNLOAD FAILED";
    }
    return std::string();
}

void Updater::check(bool force) {
    if (!enabled()) return;
    // The switch is the whole point: off means the app does not go to the
    // network on its own. A button press is not "on its own".
    if (!force && !auto_.load()) return;
    const UpdateStage s = stage_.load();
    if (s == UpdateStage::Checking || s == UpdateStage::Downloading) return;

    join_worker();
    stage_.store(UpdateStage::Checking);
    worker_ = std::thread([this] {
        const std::string url =
            std::string("https://api.github.com/repos/") + kRepo + "/releases/latest";
        const std::string doc = http_get(url);
        if (doc.empty()) {
            stage_.store(UpdateStage::CheckFailed);
            return;
        }
        const std::string tag = json_string(doc, "tag_name");
        const std::string url2 = asset_url(doc);
        if (tag.empty()) {
            stage_.store(UpdateStage::CheckFailed);
            return;
        }
        {
            std::lock_guard<std::mutex> g(m_);
            latest_ = tag[0] == 'v' ? tag.substr(1) : tag;
            asset_url_ = url2;
        }
        if (is_newer_version(tag, current_) && !url2.empty()) {
            stage_.store(UpdateStage::Available);
        } else {
            stage_.store(UpdateStage::UpToDate);
        }
    });
}

void Updater::install() {
    if (stage_.load() != UpdateStage::Available) return;
    std::string url;
    {
        std::lock_guard<std::mutex> g(m_);
        url = asset_url_;
    }
    if (url.empty()) return;

    join_worker();
    progress_.store(0);
    stage_.store(UpdateStage::Downloading);
    worker_ = std::thread([this, url] {
        const std::string dir = prefs_dir();
        if (dir.empty()) {
            stage_.store(UpdateStage::Failed);
            return;
        }
        const std::string path = dir + kAsset;
        const bool ok = http_download(url, path, [this](float p) {
            if (p >= 0) progress_.store(p);
            return !cancel_.load();
        });
        if (!ok) {
            stage_.store(UpdateStage::Failed);
            return;
        }

        // The hand-off is per platform and is deliberately the last thing:
        // past here the new version is the OS's problem, not this app's.
        //
        // Every one of these is checked. A hand-off that quietly failed and
        // still said "restart to finish" would be worse than one that failed
        // loudly, because the next launch would be the old version again with
        // no explanation.
        std::string note;
#if defined(_WIN32)
        // The installer puts the app under the user profile, so it needs no
        // administrator and can run while this copy is still open.
        if (std::system(("start \"\" \"" + path + "\"").c_str()) != 0) {
            stage_.store(UpdateStage::Failed);
            return;
        }
        note = "INSTALLER STARTED";
#elif defined(__APPLE__)
        // Opening the disk image is as far as this goes: dragging the app into
        // Applications is the convention every Mac user already knows, and
        // replacing a running bundle from inside itself is not.
        if (std::system(("open \"" + path + "\"").c_str()) != 0) {
            stage_.store(UpdateStage::Failed);
            return;
        }
        note = "DISK IMAGE OPENED";
#else
        // An AppImage is one file, so the update is a copy over itself - but
        // only when this really is running from one.
        if (std::system(("chmod +x \"" + path + "\"").c_str()) != 0) {
            stage_.store(UpdateStage::Failed);
            return;
        }
        const char* running = std::getenv("APPIMAGE");
        if (running && running[0]) {
            const std::string cp =
                "cp \"" + path + "\" \"" + std::string(running) + "\"";
            if (std::system(cp.c_str()) != 0) {
                // Most likely the AppImage is somewhere this user cannot
                // write. The download is still good, so say where it is.
                note = "DOWNLOADED TO " + path;
            } else {
                note = "RESTART TO FINISH";
            }
        } else {
            note = "DOWNLOADED TO " + path;
        }
#endif
        {
            std::lock_guard<std::mutex> g(m_);
            handoff_ = note;
        }
        stage_.store(UpdateStage::Ready);
    });
}

}  // namespace ne
