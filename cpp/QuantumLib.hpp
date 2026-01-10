#ifndef QUANTUM_LIB_HPP
#define QUANTUM_LIB_HPP

// For M_PI on Windows/MSVC - must be before cmath
#define _USE_MATH_DEFINES
#include <cmath>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>

#ifdef _WIN32
#define NOMINMAX // Prevent Windows min/max macro conflicts
#include <intrin.h>
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

// RDTSC wrapper
inline uint64_t getCycleCount() {
#ifdef _WIN32
  return __rdtsc();
#elif defined(__x86_64__) || defined(_M_X64)
  uint32_t lo, hi;
  __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
  return ((uint64_t)hi << 32) | lo;
#else
  // ARM64 (Apple Silicon): Use system timer
  uint64_t val;
  __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(val));
  return val;
#endif
}

// NOP wrapper
inline void nop() {
#ifdef _WIN32
  __nop();
#else
  __asm__ __volatile__("nop");
#endif
}

// Set thread priority
inline void setHighPriority() {
#ifdef _WIN32
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#else
  // macOS/Linux
  pthread_t thread = pthread_self();
  int policy = SCHED_FIFO;
  struct sched_param param;
  param.sched_priority = sched_get_priority_max(policy);
  pthread_setschedparam(thread, policy, &param);
#endif
}

// Complex number
struct Complex {
  double real;
  double imag;

  Complex(double r = 0.0, double i = 0.0) : real(r), imag(i) {}

  Complex operator+(const Complex &c) const {
    return Complex(real + c.real, imag + c.imag);
  }

  Complex operator-(const Complex &c) const {
    return Complex(real - c.real, imag - c.imag);
  }

  Complex operator*(const Complex &c) const {
    return Complex(real * c.real - imag * c.imag,
                   real * c.imag + imag * c.real);
  }

  double squaredModulus() const { return real * real + imag * imag; }
};

// Qubit
class Qubit {
  Complex alpha;
  Complex beta;
  static std::mt19937 rng;
  static std::uniform_real_distribution<double> dist;

public:
  Qubit() : alpha(1.0, 0.0), beta(0.0, 0.0) {}

  void applyHadamard() {
    const double invSqrt2 = 0.70710678118;
    Complex newAlpha((alpha.real + beta.real) * invSqrt2,
                     (alpha.imag + beta.imag) * invSqrt2);
    Complex newBeta((alpha.real - beta.real) * invSqrt2,
                    (alpha.imag - beta.imag) * invSqrt2);
    alpha = newAlpha;
    beta = newBeta;
  }

  void applyX() { std::swap(alpha, beta); }

  void applyZ() {
    beta.real = -beta.real;
    beta.imag = -beta.imag;
  }

  void applyS() { beta = Complex(-beta.imag, beta.real); }

  void applyT() {
    const double cos45 = 0.70710678118;
    const double sin45 = 0.70710678118;
    beta = Complex(beta.real * cos45 - beta.imag * sin45,
                   beta.real * sin45 + beta.imag * cos45);
  }

  void applyRY(double theta) {
    double cosHalf = cos(theta / 2);
    double sinHalf = sin(theta / 2);
    Complex newAlpha(cosHalf * alpha.real - sinHalf * beta.real,
                     cosHalf * alpha.imag - sinHalf * beta.imag);
    Complex newBeta(sinHalf * alpha.real + cosHalf * beta.real,
                    sinHalf * alpha.imag + cosHalf * beta.imag);
    alpha = newAlpha;
    beta = newBeta;
  }

  int measure() {
    double p0 = alpha.squaredModulus();
    return dist(rng) < p0 ? 0 : 1;
  }

  // For testing purposes
  Complex getAlpha() const { return alpha; }
  Complex getBeta() const { return beta; }
};

// Static member initialization
inline std::mt19937 Qubit::rng(std::random_device{}());
inline std::uniform_real_distribution<double> Qubit::dist(0.0, 1.0);

// ========== CPU Calibration Functions ==========

// CPU周波数を測定（Hz単位）
inline uint64_t measureCPUFrequency() {
  // 100msの間にカウントされるサイクル数を測定
  auto start_time = std::chrono::high_resolution_clock::now();
  uint64_t start_cycles = getCycleCount();

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  auto end_time = std::chrono::high_resolution_clock::now();
  uint64_t end_cycles = getCycleCount();

  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                      end_time - start_time)
                      .count();

  uint64_t cycles = end_cycles - start_cycles;
  // Hz = cycles / seconds
  return (cycles * 1000000) / duration;
}

// 時間（マイクロ秒）からtick値を計算
inline uint64_t calculateTicksFromMicroseconds(uint64_t cpu_freq_hz,
                                               double microseconds) {
  // tick = CPU周波数 × 時間（秒）
  double seconds = microseconds / 1000000.0;
  return static_cast<uint64_t>(cpu_freq_hz * seconds);
}

// 周波数（Hz）からtick値を計算
inline uint64_t calculateTicksFromFrequency(uint64_t cpu_freq_hz,
                                            double target_freq_hz) {
  // 周期（マイクロ秒） = 1 / 周波数
  double period_us = 1000000.0 / target_freq_hz;
  return calculateTicksFromMicroseconds(cpu_freq_hz, period_us);
}

// キャリブレーション情報を保持する構造体
struct CalibrationData {
  uint64_t cpu_freq_hz;
  uint64_t tick_center; // 277.3 kHz (3.606 μs)
  uint64_t tick_minus1; // 276.3 kHz (3.619 μs)
  uint64_t tick_plus1;  // 278.3 kHz (3.593 μs)
};

// CPU周波数を測定してキャリブレーション
// Default: 2.4 GHz (for Intel Core i5-6200U and similar CPUs)
inline CalibrationData calibrateCPU(uint64_t override_freq_hz = 0) {
  CalibrationData cal;

  if (override_freq_hz > 0) {
    // Use override frequency if provided
    cal.cpu_freq_hz = override_freq_hz;
  } else {
    // Try to measure CPU frequency
    uint64_t measured = measureCPUFrequency();

    // Use measured value if reasonable (> 1 GHz)
    // Otherwise use 2.4 GHz default for Intel i5-6200U
    if (measured > 1000000000ULL) {
      cal.cpu_freq_hz = measured;
    } else {
      cal.cpu_freq_hz = 2400000000ULL; // 2.4 GHz default
    }
  }

  // 277.3 kHz +/- 1 kHz
  cal.tick_center =
      calculateTicksFromFrequency(cal.cpu_freq_hz, 277300.0); // 277.3 kHz
  cal.tick_minus1 =
      calculateTicksFromFrequency(cal.cpu_freq_hz, 276300.0); // 276.3 kHz
  cal.tick_plus1 =
      calculateTicksFromFrequency(cal.cpu_freq_hz, 278300.0); // 278.3 kHz

  return cal;
}

// キャリブレーション情報を表示
inline void printCalibrationInfo(const CalibrationData &cal) {
  std::cout << "CPU Frequency: " << std::fixed << std::setprecision(2)
            << (cal.cpu_freq_hz / 1e9) << " GHz\n";
  std::cout << "Calibrated for 3.6μs base period (277.3 kHz region):\n";
  std::cout << "  Tick " << cal.tick_minus1 << " → 276.3 kHz (3.619 μs)\n";
  std::cout << "  Tick " << cal.tick_center
            << " → 277.3 kHz (3.606 μs, center)\n";
  std::cout << "  Tick " << cal.tick_plus1 << " → 278.3 kHz (3.593 μs)\n";
}

// ========== End of Calibration Functions ==========

// ========== Real FFT Implementation (Cooley-Tukey) ==========

// Complex number for FFT (separate from quantum Complex to avoid confusion)
struct FFTComplex {
  double re, im;
  FFTComplex(double r = 0, double i = 0) : re(r), im(i) {}
  FFTComplex operator+(const FFTComplex &o) const {
    return FFTComplex(re + o.re, im + o.im);
  }
  FFTComplex operator-(const FFTComplex &o) const {
    return FFTComplex(re - o.re, im - o.im);
  }
  FFTComplex operator*(const FFTComplex &o) const {
    return FFTComplex(re * o.re - im * o.im, re * o.im + im * o.re);
  }
};

// In-place Cooley-Tukey FFT (size must be power of 2)
inline void fft(FFTComplex *data, int n, bool inverse = false) {
  // Bit-reversal permutation
  for (int i = 1, j = 0; i < n; i++) {
    int bit = n >> 1;
    for (; j & bit; bit >>= 1)
      j ^= bit;
    j ^= bit;
    if (i < j)
      std::swap(data[i], data[j]);
  }

  // Cooley-Tukey iterative FFT
  for (int len = 2; len <= n; len <<= 1) {
    double angle = 2 * M_PI / len * (inverse ? -1 : 1);
    FFTComplex wlen(cos(angle), sin(angle));
    for (int i = 0; i < n; i += len) {
      FFTComplex w(1, 0);
      for (int j = 0; j < len / 2; j++) {
        FFTComplex u = data[i + j];
        FFTComplex v = data[i + j + len / 2] * w;
        data[i + j] = u + v;
        data[i + j + len / 2] = u - v;
        w = w * wlen;
      }
    }
  }

  if (inverse) {
    for (int i = 0; i < n; i++) {
      data[i].re /= n;
      data[i].im /= n;
    }
  }
}

// FFT size options for different load levels
// Larger size = more computation time
constexpr int FFT_SIZE_64 = 64;   // For 60% load
constexpr int FFT_SIZE_128 = 128; // For 75%/90% load

// Perform FFT with specified size and repeat count
inline void performFFTWithSize(int fft_size, int repeats) {
  FFTComplex data[256]; // Max size buffer

  // Initialize with test data
  for (int i = 0; i < fft_size; i++) {
    data[i] = FFTComplex(sin(2 * M_PI * i / fft_size), 0);
  }

  // Perform FFT multiple times
  for (int r = 0; r < repeats; r++) {
    fft(data, fft_size, false); // Forward FFT
    fft(data, fft_size, true);  // Inverse FFT
  }
}

// FFT Load patterns: 75%, 80%, 85%, 90% of 3.6us base period
// 75% = 2.70us, 80% = 2.88us, 85% = 3.06us, 90% = 3.24us
// Calibrated for ~5.25 GHz CPU
enum class FFTLoadLevel {
  LOAD_75_PERCENT, // 2.70 us (75% of 3.6us)
  LOAD_80_PERCENT, // 2.88 us (80% of 3.6us)
  LOAD_85_PERCENT, // 3.06 us (85% of 3.6us)
  LOAD_90_PERCENT  // 3.24 us (90% of 3.6us)
};

inline void performFFTLoad(FFTLoadLevel level = FFTLoadLevel::LOAD_75_PERCENT) {
  switch (level) {
  case FFTLoadLevel::LOAD_75_PERCENT:
    performFFTWithSize(FFT_SIZE_64, 4); // 75% load
    break;
  case FFTLoadLevel::LOAD_80_PERCENT:
    performFFTWithSize(FFT_SIZE_64, 5); // 80% load
    break;
  case FFTLoadLevel::LOAD_85_PERCENT:
    performFFTWithSize(FFT_SIZE_128, 3); // 85% load
    break;
  case FFTLoadLevel::LOAD_90_PERCENT:
    performFFTWithSize(FFT_SIZE_128, 4); // 90% load
    break;
  }
}

// Legacy compatibility (default 75% load)
inline void performFFTLoadLegacy() {
  performFFTLoad(FFTLoadLevel::LOAD_75_PERCENT);
}

// Quantum Load
inline void performQuantumLoad() {
  Qubit q;

  // Combined: H+G+QFT
  q.applyHadamard();
  for (int i = 0; i < 150; i++) {
    q.applyX();
    q.applyHadamard();
    nop();
  }

  for (int i = 0; i < 75; i++) {
    q.applyZ();
    q.applyHadamard();
    q.applyX();
    q.applyZ();
    q.applyX();
    q.applyHadamard();
    nop();
  }

  for (int i = 0; i < 150; i++) {
    double angle = M_PI / (1 << (i % 8 + 1));
    q.applyRY(angle);
    q.applyS();
    q.applyT();
    nop();
  }

  q.measure();
}

#endif // QUANTUM_LIB_HPP
