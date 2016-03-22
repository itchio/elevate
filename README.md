
# elevate

![MIT licensed](https://img.shields.io/badge/license-MIT-blue.svg)
[![cppcheck badge](https://img.shields.io/badge/cppcheck-vigilant-ff69b4.svg)](https://github.com/itchio/elevate/blob/master/.travis.yml)

Simple command-line tool to run executables that require elevated
privileges on Windows.

Think [node-runas][] but as a standalone tool, or [jpassing/elevate][]
but that compiles on MinGW and is continuously getting built by our CI server.

[node-runas]: https://github.com/atom/node-runas
[jpassing/elevate]: https://github.com/jpassing/elevate

Used by [itch][] to run some installers on
Windows.

[itch]: https://github.com/itchio/itch

### License

elevate is released under the MIT license, see `LICENSE.txt` for details
