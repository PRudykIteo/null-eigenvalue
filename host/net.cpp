#include "net.h"

#include <cstdio>
#include <vector>

#if defined(_WIN32)

#include <windows.h>
#include <winhttp.h>

namespace ne {
namespace {

// GitHub refuses a request with no user agent, which is the kind of failure
// that looks like "the network is broken" from inside the app.
const wchar_t kAgent[] = L"NullEigenvalue";

struct Url {
    std::wstring host;
    std::wstring path;
    bool ok = false;
};

Url split(const std::string& url) {
    Url u;
    std::wstring w(url.begin(), url.end());
    URL_COMPONENTS c{};
    c.dwStructSize = sizeof(c);
    c.dwHostNameLength = (DWORD)-1;
    c.dwUrlPathLength = (DWORD)-1;
    c.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(w.c_str(), (DWORD)w.size(), 0, &c)) return u;
    u.host.assign(c.lpszHostName, c.dwHostNameLength);
    u.path.assign(c.lpszUrlPath, c.dwUrlPathLength + c.dwExtraInfoLength);
    u.ok = true;
    return u;
}

// One request, following redirects, handing the caller each chunk as it
// arrives. Both entry points are the same conversation; only what is done with
// the bytes differs.
bool request(const std::string& url,
             const std::function<bool(const char*, size_t, int64_t)>& on_chunk) {
    const Url u = split(url);
    if (!u.ok) return false;

    HINTERNET session = WinHttpOpen(kAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return false;
    HINTERNET conn = WinHttpConnect(session, u.host.c_str(),
                                    INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!conn) {
        WinHttpCloseHandle(session);
        return false;
    }
    HINTERNET req = WinHttpOpenRequest(conn, L"GET", u.path.c_str(), nullptr,
                                       WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES,
                                       WINHTTP_FLAG_SECURE);
    bool ok = false;
    if (req && WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                  WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(req, nullptr)) {
        DWORD status = 0, len = sizeof(status);
        WinHttpQueryHeaders(req,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &len,
                            WINHTTP_NO_HEADER_INDEX);
        if (status == 200) {
            int64_t total = -1;
            DWORD cl = 0;
            len = sizeof(cl);
            if (WinHttpQueryHeaders(
                    req, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX, &cl, &len, WINHTTP_NO_HEADER_INDEX)) {
                total = (int64_t)cl;
            }
            ok = true;
            std::vector<char> buf(64 * 1024);
            for (;;) {
                DWORD got = 0;
                if (!WinHttpReadData(req, buf.data(), (DWORD)buf.size(), &got)) {
                    ok = false;
                    break;
                }
                if (got == 0) break;
                if (!on_chunk(buf.data(), (size_t)got, total)) {
                    ok = false;
                    break;
                }
            }
        }
    }
    if (req) WinHttpCloseHandle(req);
    WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return ok;
}

}  // namespace

std::string http_get(const std::string& url) {
    std::string body;
    if (!request(url, [&](const char* p, size_t n, int64_t) {
            body.append(p, n);
            return true;
        })) {
        return std::string();
    }
    return body;
}

bool http_download(const std::string& url, const std::string& path,
                   const std::function<bool(float)>& on_progress) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    int64_t written = 0;
    const bool ok = request(url, [&](const char* p, size_t n, int64_t total) {
        if (fwrite(p, 1, n, f) != n) return false;
        written += (int64_t)n;
        return on_progress(total > 0 ? (float)written / (float)total : -1.0f);
    });
    fclose(f);
    if (!ok) remove(path.c_str());
    return ok;
}

}  // namespace ne

#else  // ---------------------------------------------------------------------

#include <curl/curl.h>

namespace ne {
namespace {

struct Sink {
    std::string* body = nullptr;
    FILE* file = nullptr;
    const std::function<bool(float)>* progress = nullptr;
    bool aborted = false;
};

size_t on_write(char* p, size_t sz, size_t n, void* user) {
    Sink* s = (Sink*)user;
    const size_t bytes = sz * n;
    if (s->body) s->body->append(p, bytes);
    if (s->file && fwrite(p, 1, bytes, s->file) != bytes) return 0;
    return bytes;
}

int on_xfer(void* user, curl_off_t total, curl_off_t now, curl_off_t, curl_off_t) {
    Sink* s = (Sink*)user;
    if (!s->progress) return 0;
    if (!(*s->progress)(total > 0 ? (float)now / (float)total : -1.0f)) {
        s->aborted = true;
        return 1;  // non-zero aborts the transfer
    }
    return 0;
}

bool run(const std::string& url, Sink* sink) {
    CURL* c = curl_easy_init();
    if (!c) return false;
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    // GitHub refuses a request with no user agent.
    curl_easy_setopt(c, CURLOPT_USERAGENT, "NullEigenvalue");
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, on_write);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, sink);
    curl_easy_setopt(c, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 15L);
    if (sink->progress) {
        curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, on_xfer);
        curl_easy_setopt(c, CURLOPT_XFERINFODATA, sink);
    }
    const CURLcode r = curl_easy_perform(c);
    curl_easy_cleanup(c);
    return r == CURLE_OK;
}

}  // namespace

std::string http_get(const std::string& url) {
    std::string body;
    Sink s;
    s.body = &body;
    if (!run(url, &s)) return std::string();
    return body;
}

bool http_download(const std::string& url, const std::string& path,
                   const std::function<bool(float)>& on_progress) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    Sink s;
    s.file = f;
    s.progress = &on_progress;
    const bool ok = run(url, &s);
    fclose(f);
    if (!ok) remove(path.c_str());
    return ok;
}

}  // namespace ne

#endif
