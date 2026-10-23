# Contributing to MONEU

Thanks for your interest in improving MONEU! This guide covers how to build
the project, run the test suite, and submit a change.

## Building

MONEU builds on Ubuntu 22.04 (or newer) and Debian. Boost 1.74+ is required.

Install the dependencies:

    sudo apt install -y build-essential cmake \
        libboost-filesystem-dev libboost-system-dev \
        libboost-thread-dev libboost-program-options-dev \
        libssl-dev nlohmann-json3-dev

LevelDB is built from `src/leveldb`, so no system package is needed for it.

Configure and build:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j$(nproc)

This produces `moneud`, `moneu-cli` and `moneu-test-noise` in `build/src`.

If an old `build/` directory is left over from a previous attempt, remove it
first — CMake caches old paths and the build may fail with a confusing error.

See [BUILD.md](BUILD.md) for Windows builds and more detail.

## Running the tests

Before submitting a change, always run the full self-test:

    ./build/src/moneu-test-noise

It must end with:

    ALL TESTS DONE

Anything else means the build is not fit to run, and the output names the
check that failed.

## Submitting a change

1. Fork the repository and create a branch for your change:

       git checkout -b my-change

2. Keep each pull request small and focused on a single thing — it is much
   easier to review and to accept.

3. Make your change, then build and run `moneu-test-noise` to confirm it
   still ends with `ALL TESTS DONE`.

4. Commit with a clear message describing what changed and why.

5. Push your branch to your fork and open a pull request against `main`.

Continuous integration builds the project and runs the test suite on every
pull request automatically, so you can get a green check before a maintainer
reviews the change.

## A note on consensus

Changes to how blocks or transactions are validated alter the rules every
node must agree on. Please avoid touching consensus logic unless that is the
explicit purpose of the pull request, and say so clearly when it is.
