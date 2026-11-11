from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout


class blustone(ConanFile):
    name = "blustone"
    version = "0.0.1"
    settings = "os", "compiler", "build_type", "arch"
    description = "Keep Moving Forward"

    options = {
        "with_postgres": [True, False],   # Tier 4 sink; not until Phase 7
        "with_profiling": [True, False],  # gperftools CPU profiler, dev only
    }

    default_options = {
        "with_postgres": False,
        "with_profiling": False,
    }

    def requirements(self):
        # ==================================================================
        # HOT — permitted in hot-path translation units
        # ==================================================================
        # None. The hot path depends on the standard library, generated SBE
        # decoders (header-only, vendored under generated/), and our own
        # headers. Every third-party library below is INIT, WARM, COLD, or
        # DEV. This is deliberate: the hot path has no allocation, no
        # exceptions in flight, no locks, no syscalls — and therefore no
        # library that might do any of those on our behalf.

        # ==================================================================
        # INIT / NETWORK EDGE — used at boot and on I/O threads, not in the
        # strategy loop
        # ==================================================================

        # WebSocket (Beast) + async I/O (Asio) + TLS (Asio SSL). The only
        # networking stack. `without_cobalt` avoids the C++20 coroutine
        # library we do not use.
        self.requires("boost/1.91.0", options={"without_cobalt": True})

        self.requires("jemalloc/5.4.0")

        # REQUIRED, not optional. Beast's TLS stream is boost::asio::ssl,
        # which is an OpenSSL binding. Binance is wss:// only. The previous
        # manifest had this commented out, which cannot have linked.
        self.requires("openssl/4.1.0")

        # JSON for everything Binance sends as text: WebSocket API requests
        # and responses, subscription control frames, `serverShutdown`,
        # error frames, `exchangeInfo`. Use the On-Demand API with padded
        # buffers; it does not allocate per document.
        self.requires("simdjson/5.0.5")

        # ==================================================================
        # DEV — tests, benchmarks, profiling. Never in a shipped binary.
        # ==================================================================
        self.test_requires("gtest/1.18.0")
        self.test_requires("benchmark/1.9.5")

        self.requires("flatbuffers/25.12.19")
        self.requires("abseil/20260817.0")
        # self.requires("armadillo/15.2.6")
        # self.requires("eigen/5.0.1")
        # self.requires("mlpack/4.4.0")
        # self.requires("dlib/19.24.2")
        # self.requires("libpq/17.11")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BLUSTONE_WITH_POSTGRES"] = self.options.with_postgres
        tc.variables["BLUSTONE_WITH_PROFILING"] = self.options.with_profiling
        tc.generate()
        CMakeDeps(self).generate()
