// SPDX-License-Identifier: GPL-3.0-or-later
// Minimal self-contained test helpers (no external framework).
#pragma once
#include <cstdio>
#include <cmath>

namespace pt { inline int g_failures = 0; }

#define CHECK(cond) do { if (!(cond)) { std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++pt::g_failures; } } while (0)
#define CHECK_EQ(a, b) do { auto _a = (a); auto _b = (b); if (!(_a == _b)) { std::printf("  FAIL %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b); ++pt::g_failures; } } while (0)
#define CHECK_NEAR(a, b, eps) do { double _d = (double)(a) - (double)(b); if (_d < 0) _d = -_d; if (_d > (eps)) { std::printf("  FAIL %s:%d: |%s-%s|<=%g (diff %g)\n", __FILE__, __LINE__, #a, #b, (double)(eps), _d); ++pt::g_failures; } } while (0)
#define RUN(fn) do { std::printf("[test] %s\n", #fn); fn(); } while (0)
#define REPORT() (pt::g_failures == 0 ? (std::printf("OK\n"), 0) : (std::printf("%d FAILURE(S)\n", pt::g_failures), 1))
