// One piece of music, as the four numbers that produce it.
//
// The seed on its own is not enough, and that is the whole reason this exists
// rather than a uint32. The engine's random streams decide which notes the
// voices walk to and where the bells fall, but the *mood* decides the scale
// they walk in, the register and half the effects; and the field decides
// brightness and density, and through them the filter, the timbre, how many
// voices are sounding at all and how often a bell arrives. Two people on the
// same seed with their pointers in different corners are not listening to the
// same thing, and a token that only carried the seed would make that
// everybody's problem.
//
// So a token carries all four. Loading one puts the field where it was; after
// that the field is an instrument again and moving it makes a variant, which
// is what `token()` then says.
#pragma once

#include <cstdint>
#include <string>

namespace ne {

struct Piece {
    uint32_t seed = 0;
    int mood = 1;
    float x = 0.5f;   // brightness
    float y = 0.45f;  // density

    // Minutes into the piece, or 0 for the beginning.
    //
    // Not part of the sixty bits, and deliberately: a piece has no end, so a
    // moment in one is a position rather than an identity - the same
    // difference as between a record and a timecode. It rides as a suffix,
    // which also means every token written before this existed still reads.
    int at_minutes = 0;

    // The field survives a token at six bits per axis, so a piece that has
    // been through one is not quite the piece that was playing. Rounding on
    // the way *in* rather than only on the way out means what is on screen is
    // what the token says, instead of the two disagreeing in the last decimal.
    Piece quantised() const;

    // `NE1-K7M2-9QRX-4B2F`, or `NE1-K7M2-9QRX-4B2F+32` for half an hour in.
    // Short enough to read down a phone either way.
    //
    // Sixty bits: the seed, the mood, six bits per field axis, five spare and
    // an eight-bit checksum, in Crockford's base32 - so I/L/O cannot be
    // mistaken for 1/1/0 and there is no U for a token to spell something
    // with.
    //
    // The NE1 in front is not decoration. Everything after it is an index into
    // the engine's behaviour, so changing the harmony weights, a mood's
    // parameters or the order the voices are set up in changes what a token
    // produces. When that happens the prefix goes to NE2 and old tokens are
    // refused by name rather than quietly playing something else.
    std::string token() const;

    bool operator==(const Piece& o) const;
};

// Reads a token back. Returns false if the input does not contain one.
//
// Liberal about how it arrives - any case, any separators, any surrounding
// whitespace, and the prefix is looked for anywhere rather than only at the
// start - because the realistic way one of these travels is pasted out of a
// chat window with "listen to this:" stuck to the front. Strict about the two
// things that decide whether the music is right: the version prefix and the
// checksum.
bool parse_piece(const std::string& input, Piece* out);

// A piece nobody has heard yet, seeded from the clock.
Piece random_piece(int mood, float x, float y);

}  // namespace ne
