# Run tests

Suites are automatically discovered from `tests/*_test.cc`.
The default target builds and runs them.

Requires Make, a C++11 compiler, and an Intel Decimal Floating-Point library built
for the host:

```sh
make -f tests/Makefile BUILD_DIR=/tmp/free42-tests \
  DECIMAL_LIBRARY=/path/to/native/libbid.a
```

Set `CXX` to select the compiler, `DECIMAL_INCLUDE` to override the bundled decimal
headers, and `SANITIZE=1` to enable AddressSanitizer. Use a new `BUILD_DIR` when
changing compiler or sanitizer flags.

To select suites, pass `SUITES`:

```sh
make -f tests/Makefile BUILD_DIR=/tmp/free42-tests \
  DECIMAL_LIBRARY=/path/to/native/libbid.a SUITES="history_test program_history_invalidation_test"
```
