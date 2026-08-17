// The two HTTPS calls this app makes, and nothing else.
//
// WinHTTP on Windows and libcurl everywhere else. Windows gets its own backend
// rather than bundling libcurl because WinHTTP is already in the OS: the
// alternative is shipping a TLS stack and its certificate handling inside an
// installer, for one GET and one download.
#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace ne {

// Body on success, empty on any failure. Deliberately not distinguishing
// between "no network", "404" and "GitHub is having a day": every one of them
// means the same thing to this app, which is that it does not yet know whether
// there is a newer version.
std::string http_get(const std::string& url);

// Streams `url` to `path`. `on_progress` is called with 0..1, or with a
// negative value when the length is not known; returning false from it aborts
// the download, which is how closing the panel mid-download stops it.
bool http_download(const std::string& url, const std::string& path,
                   const std::function<bool(float)>& on_progress);

}  // namespace ne
