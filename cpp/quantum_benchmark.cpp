#include "QuantumLib.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <thread>
#include <vector>

// Single measurement with configurable FFT load
int measureSingle(uint64_t tick,
                  FFTLoadLevel fftLevel = FFTLoadLevel::LOAD_75_PERCENT) {
  // Baseline
  uint64_t baseStart = getCycleCount();
  uint64_t baseOps = 0;
  while ((getCycleCount() - baseStart) < tick) {
    baseOps++;
    nop();
  }
  uint64_t idleStart1 = getCycleCount();
  while ((getCycleCount() - idleStart1) < tick) {
    nop();
  }

  // Loaded with FFT
  uint64_t loadStart = getCycleCount();
  performFFTLoad(fftLevel);
  performQuantumLoad();
  uint64_t loadOps = 0;
  while ((getCycleCount() - loadStart) < tick) {
    loadOps++;
    nop();
  }
  uint64_t idleStart2 = getCycleCount();
  while ((getCycleCount() - idleStart2) < tick) {
    nop();
  }

  return static_cast<int>(baseOps) - static_cast<int>(loadOps);
}

void analyze(const std::string &name, const std::vector<int> &data) {
  double sum = 0;
  for (int v : data)
    sum += v;
  double avg = sum / data.size();

  double variance = 0;
  for (int v : data) {
    variance += (v - avg) * (v - avg);
  }
  variance /= data.size();
  double stdDev = sqrt(variance);

  int minVal = *std::min_element(data.begin(), data.end());
  int maxVal = *std::max_element(data.begin(), data.end());

  std::cout << name << ":\n";
  std::cout << "  Average: " << std::fixed << std::setprecision(2) << avg
            << "\n";
  std::cout << "  Std Dev: " << stdDev << "\n";
  std::cout << "  Range: [" << minVal << ", " << maxVal << "]\n";

  // Histogram (bins of 20)
  std::map<int, int> histogram;
  for (int value : data) {
    int bin = (value / 20) * 20;
    histogram[bin]++;
  }

  std::cout << "  Histogram (Top 10 bins):\n";
  std::vector<std::pair<int, int>> sorted(histogram.begin(), histogram.end());
  std::sort(sorted.begin(), sorted.end(),
            [](const auto &a, const auto &b) { return a.second > b.second; });

  for (size_t i = 0; i < std::min(size_t(10), sorted.size()); i++) {
    int bin = sorted[i].first;
    int count = sorted[i].second;
    double percentage = (double)count / data.size() * 100.0;
    std::cout << "    [" << bin << "-" << (bin + 19) << "]: " << count << " ("
              << std::fixed << std::setprecision(2) << percentage << "%)\n";
  }
  std::cout << "\n";
}

int main() {
  setHighPriority();

  std::cout << "=== Quantum Transition Measurement (Cross-Platform) ===\n";
  std::cout << "Auto-calibrating for your CPU...\n\n";

  // CPU周波数を測定してキャリブレーション
  CalibrationData cal = calibrateCPU();
  printCalibrationInfo(cal);

  std::cout << "\nTarget: 277.3 kHz region (±1 kHz)\n";
  std::cout << "Base Period: 3.6 microseconds\n";
  std::cout << "Iterations: 1,000,000 per measurement\n";
  std::cout << "========================================================\n\n";

  // Warmup
  std::cout << "Warming up...\n";
  uint64_t warmupStart = getCycleCount();
  while ((getCycleCount() - warmupStart) < 72000000) {
    nop();
  }
  std::cout << "Done.\n\n";

  const int iterations = 1000000;

  // Part 1: Combined FFT + Tick Variation Tests
  // 12 patterns: 4 FFT loads (75%, 80%, 85%, 90%) x 3 Ticks (-1, 0, +1)
  std::cout << "Part 1: Combined FFT + Tick Variation Tests\n";
  std::cout << "--------------------------------------------\n";
  std::cout
      << "FFT Load: 75% (2.70us), 80% (2.88us), 85% (3.06us), 90% (3.24us)\n";
  std::cout << "Tick Variation: -1 (276.3kHz), 0 (277.3kHz), +1 (278.3kHz)\n";
  std::cout << "12 combinations x 1M samples each\n\n";

  // Storage for 12 patterns
  std::vector<int> fft75_tick_m1, fft75_tick_0, fft75_tick_p1;
  std::vector<int> fft80_tick_m1, fft80_tick_0, fft80_tick_p1;
  std::vector<int> fft85_tick_m1, fft85_tick_0, fft85_tick_p1;
  std::vector<int> fft90_tick_m1, fft90_tick_0, fft90_tick_p1;

  // FFT 75% with all tick variations
  std::cout << "FFT 75% + Tick -1 (1M)...";
  std::cout.flush();
  for (int i = 0; i < iterations; i++) {
    fft75_tick_m1.push_back(
        measureSingle(cal.tick_minus1, FFTLoadLevel::LOAD_75_PERCENT));
  }
  std::cout << " done\n";
  std::cout << "FFT 75% + Tick 0 (1M)...";
  std::cout.flush();
  for (int i = 0; i < iterations; i++) {
    fft75_tick_0.push_back(
        measureSingle(cal.tick_center, FFTLoadLevel::LOAD_75_PERCENT));
  }
  std::cout << " done\n";
  std::cout << "FFT 75% + Tick +1 (1M)...";
  std::cout.flush();
  for (int i = 0; i < iterations; i++) {
    fft75_tick_p1.push_back(
        measureSingle(cal.tick_plus1, FFTLoadLevel::LOAD_75_PERCENT));
  }
  std::cout << " done\n";

  // FFT 80% with all tick variations
  std::cout << "FFT 80% + Tick -1 (1M)...";
  std::cout.flush();
  for (int i = 0; i < iterations; i++) {
    fft80_tick_m1.push_back(
        measureSingle(cal.tick_minus1, FFTLoadLevel::LOAD_80_PERCENT));
  }
  std::cout << " done\n";
  std::cout << "FFT 80% + Tick 0 (1M)...";
  std::cout.flush();
  for (int i = 0; i < iterations; i++) {
    fft80_tick_0.push_back(
        measureSingle(cal.tick_center, FFTLoadLevel::LOAD_80_PERCENT));
  }
  std::cout << " done\n";
  std::cout << "FFT 80% + Tick +1 (1M)...";
  std::cout.flush();
  for (int i = 0; i < iterations; i++) {
    fft80_tick_p1.push_back(
        measureSingle(cal.tick_plus1, FFTLoadLevel::LOAD_80_PERCENT));
  }
  std::cout << " done\n";

  // FFT 85% with all tick variations
  std::cout << "FFT 85% + Tick -1 (1M)...";
  std::cout.flush();
  for (int i = 0; i < iterations; i++) {
    fft85_tick_m1.push_back(
        measureSingle(cal.tick_minus1, FFTLoadLevel::LOAD_85_PERCENT));
  }
  std::cout << " done\n";
  std::cout << "FFT 85% + Tick 0 (1M)...";
  std::cout.flush();
  for (int i = 0; i < iterations; i++) {
    fft85_tick_0.push_back(
        measureSingle(cal.tick_center, FFTLoadLevel::LOAD_85_PERCENT));
  }
  std::cout << " done\n";
  std::cout << "FFT 85% + Tick +1 (1M)...";
  std::cout.flush();
  for (int i = 0; i < iterations; i++) {
    fft85_tick_p1.push_back(
        measureSingle(cal.tick_plus1, FFTLoadLevel::LOAD_85_PERCENT));
  }
  std::cout << " done\n";

  // FFT 90% with all tick variations
  std::cout << "FFT 90% + Tick -1 (1M)...";
  std::cout.flush();
  for (int i = 0; i < iterations; i++) {
    fft90_tick_m1.push_back(
        measureSingle(cal.tick_minus1, FFTLoadLevel::LOAD_90_PERCENT));
  }
  std::cout << " done\n";
  std::cout << "FFT 90% + Tick 0 (1M)...";
  std::cout.flush();
  for (int i = 0; i < iterations; i++) {
    fft90_tick_0.push_back(
        measureSingle(cal.tick_center, FFTLoadLevel::LOAD_90_PERCENT));
  }
  std::cout << " done\n";
  std::cout << "FFT 90% + Tick +1 (1M)...";
  std::cout.flush();
  for (int i = 0; i < iterations; i++) {
    fft90_tick_p1.push_back(
        measureSingle(cal.tick_plus1, FFTLoadLevel::LOAD_90_PERCENT));
  }
  std::cout << " done\n";

  // Part 2: Dynamic Transition with all 4 FFT loads
  std::cout << "\nPart 2: Dynamic Transition (4 FFT patterns)\n";
  std::cout << "--------------------------------------------\n";
  std::cout << "Pattern: -1->-1->0->+1->+1->0 (1M iterations each)\n\n";

  const uint64_t pattern[] = {cal.tick_minus1, cal.tick_minus1,
                              cal.tick_center, cal.tick_plus1,
                              cal.tick_plus1,  cal.tick_center};

  std::vector<int> dyn_75, dyn_80, dyn_85, dyn_90;

  std::cout << "Dynamic FFT 75% (1M)...";
  std::cout.flush();
  for (int i = 0; i < iterations; i++) {
    dyn_75.push_back(
        measureSingle(pattern[i % 6], FFTLoadLevel::LOAD_75_PERCENT));
  }
  std::cout << " done\n";

  std::cout << "Dynamic FFT 80% (1M)...";
  std::cout.flush();
  for (int i = 0; i < iterations; i++) {
    dyn_80.push_back(
        measureSingle(pattern[i % 6], FFTLoadLevel::LOAD_80_PERCENT));
  }
  std::cout << " done\n";

  std::cout << "Dynamic FFT 85% (1M)...";
  std::cout.flush();
  for (int i = 0; i < iterations; i++) {
    dyn_85.push_back(
        measureSingle(pattern[i % 6], FFTLoadLevel::LOAD_85_PERCENT));
  }
  std::cout << " done\n";

  std::cout << "Dynamic FFT 90% (1M)...";
  std::cout.flush();
  for (int i = 0; i < iterations; i++) {
    dyn_90.push_back(
        measureSingle(pattern[i % 6], FFTLoadLevel::LOAD_90_PERCENT));
  }
  std::cout << " done\n";

  // Analysis
  std::cout << "\n========================================================\n";
  std::cout << "Statistical Analysis\n";
  std::cout << "========================================================\n\n";

  std::cout << "--- FFT 75% (2.70us) + Tick Variations ---\n\n";
  analyze("FFT75% Tick-1", fft75_tick_m1);
  analyze("FFT75% Tick 0", fft75_tick_0);
  analyze("FFT75% Tick+1", fft75_tick_p1);

  std::cout << "--- FFT 80% (2.88us) + Tick Variations ---\n\n";
  analyze("FFT80% Tick-1", fft80_tick_m1);
  analyze("FFT80% Tick 0", fft80_tick_0);
  analyze("FFT80% Tick+1", fft80_tick_p1);

  std::cout << "--- FFT 85% (3.06us) + Tick Variations ---\n\n";
  analyze("FFT85% Tick-1", fft85_tick_m1);
  analyze("FFT85% Tick 0", fft85_tick_0);
  analyze("FFT85% Tick+1", fft85_tick_p1);

  std::cout << "--- FFT 90% (3.24us) + Tick Variations ---\n\n";
  analyze("FFT90% Tick-1", fft90_tick_m1);
  analyze("FFT90% Tick 0", fft90_tick_0);
  analyze("FFT90% Tick+1", fft90_tick_p1);

  std::cout << "--- Dynamic Transition (4 FFT patterns) ---\n\n";
  analyze("Dynamic FFT75%", dyn_75);
  analyze("Dynamic FFT80%", dyn_80);
  analyze("Dynamic FFT85%", dyn_85);
  analyze("Dynamic FFT90%", dyn_90);

  std::cout << "========================================================\n";
  std::cout << "Done.\n";

  return 0;
}
