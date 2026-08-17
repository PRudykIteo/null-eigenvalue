#include "piece.h"

#include <chrono>
#include <cmath>

#include "palette.h"

// For kMoodGenerated: what the mood field is allowed to say.
#include "harmony.h"

namespace ne {
namespace {

const char kPrefix[] = "NE1";
constexpr int kPrefixLen = 3;
constexpr int kTokenChars = 12;
constexpr int kChecksumBits = 8;

// Crockford's base32: no I, L, O or U.
const char kAlphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

int value_of(char c) {
    // The substitutions Crockford specifies, which are the whole point of
    // choosing it: someone reading a token aloud says "oh" and "ell".
    if (c == 'O') return 0;
    if (c == 'I' || c == 'L') return 1;
    for (int i = 0; i < 32; ++i) {
        if (kAlphabet[i] == c) return i;
    }
    return -1;
}

// 63 rather than 64 as the divisor, so both ends of the field are exactly
// representable and a token made at a corner reproduces there.
uint32_t encode_axis(float v) {
    const float c = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    long q = std::lround(c * 63.0f);
    if (q < 0) q = 0;
    if (q > 63) q = 63;
    return (uint32_t)q;
}

float decode_axis(uint32_t v) { return (float)v / 63.0f; }

uint64_t payload_of(const Piece& p) {
    return ((uint64_t)(p.seed) << 20) | ((uint64_t)(p.mood & 0x7) << 17) |
           ((uint64_t)encode_axis(p.x) << 11) | ((uint64_t)encode_axis(p.y) << 5);
}

uint32_t checksum(uint64_t payload) {
    // FNV-1a over the payload's seven bytes. Not a cryptographic anything; it
    // is here to turn a mistyped character into "that is not a token" instead
    // of into a different piece of music.
    uint32_t h = 0x811c9dc5u;
    for (int shift = 48; shift >= 0; shift -= 8) {
        h ^= (uint32_t)((payload >> shift) & 0xFF);
        h = (uint32_t)(h * 0x01000193u);
    }
    return h & 0xFF;
}

bool decode_at(const std::string& s, size_t start, Piece* out) {
    if (start + kTokenChars > s.size()) return false;

    uint64_t full = 0;
    for (size_t i = start; i < start + kTokenChars; ++i) {
        const int v = value_of(s[i]);
        if (v < 0) return false;
        full = (full << 5) | (uint64_t)v;
    }

    const uint64_t payload = full >> kChecksumBits;
    if ((uint32_t)(full & ((1u << kChecksumBits) - 1)) != checksum(payload)) {
        return false;
    }

    const int mood = (int)((payload >> 17) & 0x7);
    // 0..5 are the six named instruments; 6 means the instrument is derived
    // from this piece's own seed. 7 is unused and refused - a token from a
    // build that knows something this one does not names music that is not
    // here, and clamping would play the wrong piece silently, which is exactly
    // what the checksum exists to avoid.
    if (mood > kMoodGenerated) return false;

    out->seed = (uint32_t)((payload >> 20) & 0xFFFFFFFFull);
    out->mood = mood;
    out->x = decode_axis((uint32_t)((payload >> 11) & 0x3F));
    out->y = decode_axis((uint32_t)((payload >> 5) & 0x3F));
    return true;
}

}  // namespace

Piece Piece::quantised() const {
    Piece p;
    p.seed = seed;
    p.mood = mood;
    p.at_minutes = at_minutes < 0 ? 0 : at_minutes;
    p.x = decode_axis(encode_axis(x));
    p.y = decode_axis(encode_axis(y));
    return p;
}

std::string Piece::token() const {
    const uint64_t payload = payload_of(*this);
    const uint64_t full = (payload << kChecksumBits) | checksum(payload);

    char body[kTokenChars + 1] = {};
    for (int i = 0; i < kTokenChars; ++i) {
        const int shift = (kTokenChars - 1 - i) * 5;
        body[i] = kAlphabet[(full >> shift) & 0x1F];
    }

    std::string out(kPrefix);
    out += '-';
    out.append(body, 4);
    out += '-';
    out.append(body + 4, 4);
    out += '-';
    out.append(body + 8, 4);
    if (at_minutes > 0) {
        char suffix[16];
        std::snprintf(suffix, sizeof(suffix), "+%d", at_minutes);
        out += suffix;
    }
    return out;
}

bool Piece::operator==(const Piece& o) const {
    return seed == o.seed && mood == o.mood && at_minutes == o.at_minutes &&
           payload_of(*this) == payload_of(o);
}

bool parse_piece(const std::string& input, Piece* out) {
    if (!out) return false;
    // The plus is kept along with the alphanumerics: it is the one separator
    // that means something rather than being decoration a chat window added.
    std::string cleaned;
    cleaned.reserve(input.size());
    for (char c : input) {
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || c == '+') {
            cleaned += c;
        }
    }

    // Every place the prefix appears, not only the start: the checksum is what
    // decides whether a candidate really is one.
    for (size_t at = cleaned.find(kPrefix); at != std::string::npos;
         at = cleaned.find(kPrefix, at + 1)) {
        if (decode_at(cleaned, at + kPrefixLen, out)) {
            // A "+32" immediately after the body is a moment in the piece.
            // Anything else there is somebody else's punctuation.
            size_t k = at + kPrefixLen + kTokenChars;
            out->at_minutes = 0;
            if (k < cleaned.size() && cleaned[k] == '+') {
                int mins = 0, digits = 0;
                for (++k; k < cleaned.size() && cleaned[k] >= '0' &&
                          cleaned[k] <= '9' && digits < 5;
                     ++k, ++digits) {
                    mins = mins * 10 + (cleaned[k] - '0');
                }
                if (digits > 0) out->at_minutes = mins;
            }
            return true;
        }
    }
    return false;
}

Piece random_piece(int mood, float x, float y) {
    // The clock is the only entropy this app needs: nobody is guessing these
    // and there is nothing to protect.
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const uint64_t ns = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
    // Splitmix64, so that two pieces made a millisecond apart do not share
    // their high bits - a raw clock value walks, and the engine seeds several
    // streams from this one number.
    uint64_t z = ns + 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z = z ^ (z >> 31);

    Piece p;
    p.seed = (uint32_t)(z & 0xFFFFFFFFull);
    p.mood = mood;
    p.x = x;
    p.y = y;
    return p.quantised();  // a new piece always starts at its beginning
}

}  // namespace ne
