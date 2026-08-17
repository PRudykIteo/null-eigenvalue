// The updater's one piece of pure logic, and the one place it could do harm:
// say yes wrongly and it downloads an installer nobody asked for.
//
// Everything past that guard is an HTTPS request, and a test that reaches
// GitHub is a test that fails whenever the runner has no network.
#include <cstdio>

#include "updater.h"

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL  %s\n", what);
        ++failures;
    }
}

}  // namespace

int main() {
    using ne::is_newer_version;

    check(is_newer_version("0.1.42", "0.1.41"), "a higher patch is newer");
    check(!is_newer_version("0.1.41", "0.1.42"), "a lower patch is not");
    check(!is_newer_version("0.1.7", "0.1.7"), "the same version is not newer");

    // The bug this exists to prevent: CI's patch number is the run number, so
    // it goes past 9 on the tenth push and string ordering would then stop
    // offering updates for good.
    check(is_newer_version("0.1.10", "0.1.9"), "components compare as numbers");
    check(!is_newer_version("0.2.0", "0.10.0"), "and so does the minor");

    check(is_newer_version("0.2", "0.1.9"), "a missing component counts as zero");
    check(!is_newer_version("0.1", "0.1.0"), "and equally on the other side");

    // A tag arrives as v0.1.42; the version baked into the build does not.
    check(is_newer_version("v0.1.42", "0.1.41"), "a leading v is tolerated");

    // A malformed tag must be able to fail in one direction only.
    check(!is_newer_version("nightly", "0.1.7"), "unparseable is not newer");
    check(!is_newer_version("0.1.7-rc1", "0.1.6"), "a suffix is not a version");
    check(!is_newer_version("0.1.8", ""), "nothing to compare against");
    check(!is_newer_version("", "0.1.8"), "and the other way round");

    // A dev build has no version, so the whole thing stays switched off.
    check(!ne::Updater("").enabled(), "a build CI did not cut says nothing");
    check(ne::Updater("0.1.0").enabled(), "one it did cut does");

    if (failures == 0) {
        std::printf("updater: all checks passed\n");
        return 0;
    }
    std::printf("updater: %d FAILED\n", failures);
    return 1;
}
