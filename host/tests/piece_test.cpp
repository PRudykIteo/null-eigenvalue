// The token format, which is the one thing in this app that has to stay
// byte-identical forever: a token written down last week has to name the same
// piece next year, and the same piece in a different build.
#include <cstdio>
#include <cstdlib>
#include <string>

#include "piece.h"

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("FAIL  %s\n", what.c_str());
        ++failures;
    }
}

void expect_token(uint32_t seed, int mood, float x, float y,
                  const std::string& want) {
    ne::Piece p{seed, mood, x, y};
    const std::string got = p.quantised().token();
    if (got != want) {
        std::printf("FAIL  seed=%u mood=%d x=%.3f y=%.3f\n        want %s\n        got  %s\n",
                    seed, mood, (double)x, (double)y, want.c_str(), got.c_str());
        ++failures;
    }
}

void round_trip(uint32_t seed, int mood, float x, float y) {
    const ne::Piece a = ne::Piece{seed, mood, x, y}.quantised();
    ne::Piece b;
    if (!ne::parse_piece(a.token(), &b)) {
        std::printf("FAIL  %s did not parse back\n", a.token().c_str());
        ++failures;
        return;
    }
    check(a == b, "round trip " + a.token());
    check(a.token() == b.token(), "token stable through a round trip");
}

}  // namespace

int main() {
    // ---- the format is pinned, not merely self-consistent -------------------
    // These came from the Dart implementation the C++ was ported from. If a
    // change here is deliberate, the prefix goes to NE2 - see piece.h.
    expect_token(0u, 0, 0.0f, 0.0f, "NE1-0000-0000-0017");
    expect_token(1u, 0, 0.0f, 0.0f, "NE1-0000-0080-003Q");
    expect_token(0xFFFFFFFFu, 5, 1.0f, 1.0f, "NE1-ZZZZ-ZZXZ-ZR2S");
    expect_token(0x4E756C6Cu, 1, 0.5f, 0.45f, "NE1-9STP-RV1G-7002");
    expect_token(12345u, 2, 0.62f, 0.38f, "NE1-0003-0EAK-P069");
    expect_token(999u, 2, 0.5f, 0.5f, "NE1-0000-7STG-805E");

    // ---- round trips --------------------------------------------------------
    round_trip(0u, 0, 0.0f, 0.0f);
    round_trip(0xFFFFFFFFu, 5, 1.0f, 1.0f);
    round_trip(12345u, 2, 0.62f, 0.38f);
    round_trip(0x80000000u, 3, 0.999f, 0.001f);

    // ---- quantising is idempotent ------------------------------------------
    {
        const ne::Piece a = ne::Piece{7u, 1, 0.333f, 0.777f}.quantised();
        check(a == a.quantised(), "quantise twice is quantise once");
    }

    // ---- the corners survive, which is why the divisor is 63 ----------------
    {
        ne::Piece p;
        check(ne::parse_piece(ne::Piece{1u, 0, 0.0f, 0.0f}.quantised().token(), &p),
              "corner 0,0 parses");
        check(p.x == 0.0f && p.y == 0.0f, "0,0 survives a token exactly");
        check(ne::parse_piece(ne::Piece{1u, 0, 1.0f, 1.0f}.quantised().token(), &p),
              "corner 1,1 parses");
        check(p.x == 1.0f && p.y == 1.0f, "1,1 survives a token exactly");
    }

    // ---- liberal about how a token arrives ---------------------------------
    {
        const std::string t = ne::Piece{999u, 2, 0.5f, 0.5f}.quantised().token();
        ne::Piece a, b, c, d;
        check(ne::parse_piece("listen to this: " + t + " !!", &a),
              "prefix found mid-sentence");
        check(ne::parse_piece(t + "\n", &b), "trailing whitespace");
        std::string lower;
        for (char ch : t) lower += (char)(ch >= 'A' && ch <= 'Z' ? ch + 32 : ch);
        check(ne::parse_piece(lower, &c), "lower case");
        std::string spaced;
        for (char ch : t) {
            if (ch != '-') spaced += ch;
        }
        check(ne::parse_piece(spaced, &d), "separators removed");
        check(a == b && b == c && c == d, "all four read the same piece");
    }

    // ---- Crockford's substitutions -----------------------------------------
    {
        const std::string t = ne::Piece{0u, 0, 0.0f, 0.0f}.quantised().token();
        // Only the body. The NE1 in front is a version marker rather than
        // data, and it is matched literally - a token whose prefix has been
        // mangled is one this build should not be guessing about.
        std::string misread = t.substr(0, 3);
        for (size_t i = 3; i < t.size(); ++i) {
            const char ch = t[i];
            misread += (ch == '0') ? 'O' : (ch == '1' ? 'L' : ch);
        }
        ne::Piece p;
        check(ne::parse_piece(misread, &p), "O for 0 and L for 1 still read");
        check(p == ne::Piece{0u, 0, 0.0f, 0.0f}.quantised(), "and read correctly");
    }

    // ---- what must be refused ----------------------------------------------
    {
        ne::Piece p;
        check(!ne::parse_piece("", &p), "empty");
        check(!ne::parse_piece("NE1-0000-0000-0000", &p), "bad checksum refused");
        check(!ne::parse_piece("NE2-0000-0000-05GX", &p), "a future prefix refused");
        check(!ne::parse_piece("NE1-0000-0000-05G", &p), "too short");
        // 6 is a generated instrument and is legal; 7 names nothing.
        int refused = 0, accepted = 0;
        for (uint32_t seed = 0; seed < 64; ++seed) {
            ne::Piece gen{seed, 6, 0.0f, 0.0f};
            if (ne::parse_piece(gen.quantised().token(), &p)) ++accepted;
            ne::Piece bad{seed, 7, 0.0f, 0.0f};
            if (!ne::parse_piece(bad.quantised().token(), &p)) ++refused;
        }
        check(accepted == 64, "a generated instrument is a legal piece");
        check(refused == 64, "a mood this build does not have is refused, not clamped");
    }

    // ---- a mistyped character must not become different music ---------------
    {
        const ne::Piece a = ne::Piece{0x4E756C6Cu, 1, 0.5f, 0.45f}.quantised();
        const std::string t = a.token();
        int accepted = 0, changed = 0;
        for (size_t i = 0; i < t.size(); ++i) {
            if (t[i] == '-') continue;
            for (int v = 0; v < 32; ++v) {
                std::string m = t;
                m[i] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ"[v];
                if (m == t) continue;
                ne::Piece got;
                if (ne::parse_piece(m, &got)) {
                    ++accepted;
                    if (!(got == a)) ++changed;
                }
            }
        }
        std::printf("  single-character typos accepted: %d (of which different music: %d)\n",
                    accepted, changed);
        // An eight-bit checksum lets roughly 1 in 256 through; the point is
        // that it is rare, not that it is impossible.
        check(accepted <= 4, "almost every single-character typo is rejected");
    }

    // ---- a moment in a piece ------------------------------------------------
    {
        ne::Piece a = ne::Piece{12345u, 2, 0.62f, 0.38f}.quantised();
        check(a.token().find('+') == std::string::npos,
              "a piece with no moment has no suffix");

        a.at_minutes = 32;
        const std::string t = a.token();
        check(t.size() > 18 && t.substr(t.size() - 3) == "+32", "the suffix is written");

        ne::Piece b;
        check(ne::parse_piece(t, &b), "and read back");
        check(b.at_minutes == 32, "with the minutes intact");
        check(b == a, "as the same piece");

        // Backward compatibility is the whole reason this is a suffix rather
        // than more bits: a token written before moments existed still reads,
        // and means the beginning.
        ne::Piece c;
        check(ne::parse_piece("NE1-0003-0EAK-P069", &c), "an old token still reads");
        check(c.at_minutes == 0, "and starts at the beginning");

        // Punctuation after a token is somebody else's, not a moment.
        ne::Piece d;
        check(ne::parse_piece("NE1-0003-0EAK-P069!!", &d), "trailing punctuation");
        check(d.at_minutes == 0, "is not mistaken for a moment");

        ne::Piece e;
        check(ne::parse_piece("listen: " + t + " it gets good there", &e),
              "a moment survives being pasted out of a sentence");
        check(e.at_minutes == 32, "with its minutes");
    }

    if (failures == 0) {
        std::printf("piece: all checks passed\n");
        return 0;
    }
    std::printf("piece: %d FAILED\n", failures);
    return 1;
}
