
# elevate

[![Build Status](https://travis-ci.org/itchio/elevate.svg)](https://travis-ci.org/itchio/elevate)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/7034/badge.svg)](https://scan.coverity.com/projects/itchio-elevate)

Simple command-line tool to run executables that require elevated
privileges on Windows

Think [node-runas][] but as a standalone tool, or [jpassing/elevate][]
but that compiles on MinGW and is continuously getting built on Travis CI.

[node-runas]: https://github.com/atom/node-runas
[jpassing/elevate]: https://github.com/jpassing/elevate

Used by [the itch.io desktop client][] to run some installers on
Windows.

[the itch.io desktop client]: https://github.com/itchio/itchio-app

### License

elevate is released under the MIT license, see LICENSE.md for details.
