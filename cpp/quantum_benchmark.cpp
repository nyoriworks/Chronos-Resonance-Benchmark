#include "QuantumLib.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
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

// Quick stats for scheduled mode
struct QuickStats {
  double avg;
  double stdDev;
  int peakBin;
  double peakPercent;
};

QuickStats quickAnalyze(const std::vector<int> &data) {
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

  // Find peak bin
  std::map<int, int> histogram;
  for (int value : data) {
    int bin = (value / 20) * 20;
    histogram[bin]++;
  }

  std::vector<std::pair<int, int>> sorted(histogram.begin(), histogram.end());
  std::sort(sorted.begin(), sorted.end(),
            [](const auto &a, const auto &b) { return a.second > b.second; });

  int peakBin = sorted.empty() ? 0 : sorted[0].first;
  int peakCount = sorted.empty() ? 0 : sorted[0].second;
  double peakPercent = (double)peakCount / data.size() * 100.0;

  return {avg, stdDev, peakBin, peakPercent};
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

// Get current time as formatted string
std::string getCurrentTimeStr() {
  auto now = std::chrono::system_clock::now();
  std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  std::tm *tm = std::localtime(&now_time);

  std::ostringstream oss;
  oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
  return oss.str();
}

// Get current date as formatted string
std::string getCurrentDateStr() {
  auto now = std::chrono::system_clock::now();
  std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  std::tm *tm = std::localtime(&now_time);

  std::ostringstream oss;
  oss << std::put_time(tm, "%Y-%m-%d");
  return oss.str();
}

// Get current hour and minute
void getCurrentHourMinute(int &hour, int &minute, int &second) {
  auto now = std::chrono::system_clock::now();
  std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  std::tm *tm = std::localtime(&now_time);
  hour = tm->tm_hour;
  minute = tm->tm_min;
  second = tm->tm_sec;
}

// Scheduled mode: measurements every 30 minutes
void runScheduledMode(const CalibrationData &cal) {
  std::cout << "=== Scheduled Mode: 30-Minute Interval Measurements ===\n";
  std::cout << "Measuring at: 00:00, 00:30, 01:00, ... 23:00, 23:30\n";
  std::cout << "Total: 48 measurements x 32 patterns = 1,536 data points/day\n";
  std::cout << "  - 12 Static (4 FFT x 3 Ticks)\n";
  std::cout << "  - 20 Dynamic (4 FFT x 5 Patterns)\n";
  std::cout << "========================================================\n\n";

  // Create log file
  std::string logFileName = "time_surface_" + getCurrentDateStr() + ".csv";
  std::cout << "Log file: " << logFileName << "\n\n";

  // Write CSV header
  std::ofstream logFile(logFileName, std::ios::app);
  if (logFile.tellp() == 0) {
    logFile << "timestamp,hour,minute,type,fft_level,pattern,avg,std_dev,peak_"
               "bin,peak_percent\n";
  }
  logFile.close();

  const int iterations =
      30000; // 30K per pattern (32 patterns x 30K = 960K total)

  // FFT load levels
  struct FFTConfig {
    const char *name;
    FFTLoadLevel level;
  };
  FFTConfig fftConfigs[] = {{"75%", FFTLoadLevel::LOAD_75_PERCENT},
                            {"80%", FFTLoadLevel::LOAD_80_PERCENT},
                            {"85%", FFTLoadLevel::LOAD_85_PERCENT},
                            {"90%", FFTLoadLevel::LOAD_90_PERCENT}};

  // Static tick configurations
  struct TickConfig {
    const char *name;
    uint64_t tick;
  };
  TickConfig tickConfigs[] = {
      {"-1", cal.tick_minus1}, {"0", cal.tick_center}, {"+1", cal.tick_plus1}};

  // Dynamic patterns
  struct DynamicPattern {
    const char *name;
    uint64_t ticks[6];
  };
  DynamicPattern dynamicPatterns[] = {
      {"Original",
       {cal.tick_minus1, cal.tick_minus1, cal.tick_center, cal.tick_plus1,
        cal.tick_plus1, cal.tick_center}},
      {"Alternating",
       {cal.tick_plus1, cal.tick_minus1, cal.tick_plus1, cal.tick_minus1,
        cal.tick_plus1, cal.tick_minus1}},
      {"Block",
       {cal.tick_plus1, cal.tick_plus1, cal.tick_plus1, cal.tick_minus1,
        cal.tick_minus1, cal.tick_minus1}},
      {"Mixed",
       {cal.tick_center, cal.tick_minus1, cal.tick_plus1, cal.tick_center,
        cal.tick_plus1, cal.tick_minus1}},
      {"Sweep",
       {cal.tick_minus1, cal.tick_center, cal.tick_plus1, cal.tick_plus1,
        cal.tick_center, cal.tick_minus1}}};

  std::string lastRunTime = "";
  int lastPrintedMinute = -1;

  while (true) {
    int hour, minute, second;
    getCurrentHourMinute(hour, minute, second);

    char timeStrBuf[10];
    snprintf(timeStrBuf, sizeof(timeStrBuf), "%02d:%02d", minute, second);
    std::string timeStr = timeStrBuf;

    // Trigger at XX:29:00 or XX:59:00 for 2-minute boundary scan
    bool isBoundaryScan = (second == 0) && (minute == 29 || minute == 59);

    if (isBoundaryScan && lastRunTime != timeStr) {
      lastRunTime = timeStr;

      int boundaryMinute = (minute == 29) ? 30 : 0;
      std::string timestamp = getCurrentTimeStr();

      std::cout << "\n[[" << timestamp
                << "] Starting 2-minute Boundary Scan...\n";
      std::cout << "  Scanning across " << hour << ":" << std::setfill('0')
                << std::setw(2) << boundaryMinute << ":00 boundary\n";

      auto scanStart = std::chrono::steady_clock::now();
      auto scanEnd = scanStart + std::chrono::seconds(120); // 2 minutes
      int patternIndex = 0;

      // Build pattern list: 12 static + 20 dynamic = 32 patterns
      // We'll cycle through them continuously

      while (std::chrono::steady_clock::now() < scanEnd) {
        int pIdx = patternIndex % 32;
        std::string measureTimestamp = getCurrentTimeStr();

        // Get current time for CSV
        int mHour, mMinute, mSecond;
        getCurrentHourMinute(mHour, mMinute, mSecond);

        std::vector<int> data;
        data.reserve(iterations);

        std::string patternType;
        std::string fftName;
        std::string patternName;

        if (pIdx < 12) {
          // Static pattern (index 0-11)
          int fftIdx = pIdx / 3;
          int tickIdx = pIdx % 3;
          patternType = "Static";
          fftName = fftConfigs[fftIdx].name;
          patternName = std::string("Tick") + tickConfigs[tickIdx].name;

          for (int i = 0; i < iterations; i++) {
            data.push_back(measureSingle(tickConfigs[tickIdx].tick,
                                         fftConfigs[fftIdx].level));
          }
        } else {
          // Dynamic pattern (index 12-31)
          int dynIdx = pIdx - 12;
          int fftIdx = dynIdx / 5;
          int patIdx = dynIdx % 5;
          patternType = "Dynamic";
          fftName = fftConfigs[fftIdx].name;
          patternName = dynamicPatterns[patIdx].name;

          for (int i = 0; i < iterations; i++) {
            uint64_t tick = dynamicPatterns[patIdx].ticks[i % 6];
            data.push_back(measureSingle(tick, fftConfigs[fftIdx].level));
          }
        }

        QuickStats stats = quickAnalyze(data);

        // Append to CSV with precise timestamp
        std::ofstream logFile(logFileName, std::ios::app);
        logFile << measureTimestamp << "," << mHour << "," << mMinute << ","
                << patternType << "," << fftName << "," << patternName << ","
                << std::fixed << std::setprecision(2) << stats.avg << ","
                << stats.stdDev << "," << stats.peakBin << ","
                << stats.peakPercent << "\n";
        logFile.close();

        // Progress indicator every 32 patterns (1 full cycle)
        if (patternIndex % 32 == 0) {
          std::cout << "  [" << std::setfill('0') << std::setw(2) << mMinute
                    << ":" << std::setw(2) << mSecond << "] Cycle "
                    << (patternIndex / 32 + 1) << "...\n";
        }

        patternIndex++;
      }

      std::cout << "Boundary scan complete. " << patternIndex
                << " patterns recorded.\n";
      std::cout << "Waiting for next boundary...\n";
    }

    if (minute != lastPrintedMinute) {
      std::cout << "Waiting... (Current: " << std::setfill('0') << std::setw(2)
                << minute << ":" << std::setw(2) << second << ")\n";
      lastPrintedMinute = minute;
    }

    // Sleep for 0.1s
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

// Full benchmark mode
void runFullBenchmark(const CalibrationData &cal) {
  std::cout << "\nTarget: 277.3 kHz region (+/-1 kHz)\n";
  std::cout << "Base Period: 3.6 microseconds\n";
  std::cout << "Iterations: 1,000,000 per measurement\n";
  std::cout << "Patterns: 32 (12 Static + 20 Dynamic)\n";
  std::cout << "========================================================\n\n";

  // Warmup
  std::cout << "Warming up...\n";
  uint64_t warmupStart = getCycleCount();
  while ((getCycleCount() - warmupStart) < 72000000) {
    nop();
  }
  std::cout << "Done.\n\n";

  const int iterations = 1000000;

  // FFT load levels
  struct FFTConfig {
    const char *name;
    FFTLoadLevel level;
  };
  FFTConfig fftConfigs[] = {{"75%", FFTLoadLevel::LOAD_75_PERCENT},
                            {"80%", FFTLoadLevel::LOAD_80_PERCENT},
                            {"85%", FFTLoadLevel::LOAD_85_PERCENT},
                            {"90%", FFTLoadLevel::LOAD_90_PERCENT}};

  // Tick configurations
  struct TickConfig {
    const char *name;
    uint64_t tick;
  };
  TickConfig tickConfigs[] = {
      {"-1", cal.tick_minus1}, {"0", cal.tick_center}, {"+1", cal.tick_plus1}};

  // Dynamic patterns
  struct DynamicPattern {
    const char *name;
    uint64_t ticks[6];
  };
  DynamicPattern dynamicPatterns[] = {
      {"Original",
       {cal.tick_minus1, cal.tick_minus1, cal.tick_center, cal.tick_plus1,
        cal.tick_plus1, cal.tick_center}},
      {"Alternating",
       {cal.tick_plus1, cal.tick_minus1, cal.tick_plus1, cal.tick_minus1,
        cal.tick_plus1, cal.tick_minus1}},
      {"Block",
       {cal.tick_plus1, cal.tick_plus1, cal.tick_plus1, cal.tick_minus1,
        cal.tick_minus1, cal.tick_minus1}},
      {"Mixed",
       {cal.tick_center, cal.tick_minus1, cal.tick_plus1, cal.tick_center,
        cal.tick_plus1, cal.tick_minus1}},
      {"Sweep",
       {cal.tick_minus1, cal.tick_center, cal.tick_plus1, cal.tick_plus1,
        cal.tick_center, cal.tick_minus1}}};

  // Storage for results
  std::map<std::string, std::vector<int>> results;

  // Part 1: Static Patterns (12 = 4 FFT x 3 Ticks)
  std::cout << "Part 1: Static Patterns (12 = 4 FFT x 3 Ticks)\n";
  std::cout << "--------------------------------------------\n";
  std::cout
      << "FFT Load: 75% (2.70us), 80% (2.88us), 85% (3.06us), 90% (3.24us)\n";
  std::cout << "Tick Variation: -1 (276.3kHz), 0 (277.3kHz), +1 (278.3kHz)\n";
  std::cout << "12 combinations x 1M samples each\n\n";

  for (const auto &fft : fftConfigs) {
    for (const auto &tick : tickConfigs) {
      std::string key = std::string("FFT") + fft.name + " Tick" + tick.name;
      std::cout << key << " (1M)...";
      std::cout.flush();

      std::vector<int> data;
      data.reserve(iterations);
      for (int i = 0; i < iterations; i++) {
        data.push_back(measureSingle(tick.tick, fft.level));
      }
      results[key] = data;
      std::cout << " done\n";
    }
  }

  // Part 2: Dynamic Transition (20 = 4 FFT x 5 Patterns)
  std::cout << "\nPart 2: Dynamic Transition (20 = 4 FFT x 5 Patterns)\n";
  std::cout << "--------------------------------------------\n";
  std::cout << "Patterns:\n";
  std::cout << "  Original: -1->-1->0->+1->+1->0\n";
  std::cout << "  Alternating: +1->-1->+1->-1->+1->-1\n";
  std::cout << "  Block: +1->+1->+1->-1->-1->-1\n";
  std::cout << "  Mixed: 0->-1->+1->0->+1->-1\n";
  std::cout << "  Sweep: -1->0->+1->+1->0->-1\n";
  std::cout << "20 combinations x 1M samples each\n\n";

  for (const auto &fft : fftConfigs) {
    for (const auto &pattern : dynamicPatterns) {
      std::string key =
          std::string("Dynamic FFT") + fft.name + " " + pattern.name;
      std::cout << key << " (1M)...";
      std::cout.flush();

      std::vector<int> data;
      data.reserve(iterations);
      for (int i = 0; i < iterations; i++) {
        uint64_t tick = pattern.ticks[i % 6];
        data.push_back(measureSingle(tick, fft.level));
      }
      results[key] = data;
      std::cout << " done\n";
    }
  }

  // Analysis
  std::cout << "\n========================================================\n";
  std::cout << "Statistical Analysis\n";
  std::cout << "========================================================\n\n";

  // Static analysis
  for (const auto &fft : fftConfigs) {
    std::cout << "--- FFT " << fft.name << " + Tick Variations ---\n\n";
    for (const auto &tick : tickConfigs) {
      std::string key = std::string("FFT") + fft.name + " Tick" + tick.name;
      analyze(key, results[key]);
    }
  }

  // Dynamic analysis
  std::cout << "--- Dynamic Transition (5 patterns x 4 FFT) ---\n\n";
  for (const auto &fft : fftConfigs) {
    for (const auto &pattern : dynamicPatterns) {
      std::string key =
          std::string("Dynamic FFT") + fft.name + " " + pattern.name;
      analyze(key, results[key]);
    }
  }

  std::cout << "========================================================\n";
  std::cout << "Done.\n";
}

int main(int argc, char *argv[]) {
  setHighPriority();

  // Check for scheduled mode
  bool scheduledMode = false;
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--scheduled" || arg == "-s") {
      scheduledMode = true;
    }
  }

  std::cout << "=== Quantum Transition Measurement (Cross-Platform) ===\n";
  std::cout << "Auto-calibrating for your CPU...\n\n";

  CalibrationData cal = calibrateCPU();
  printCalibrationInfo(cal);

  if (scheduledMode) {
    runScheduledMode(cal);
  } else {
    runFullBenchmark(cal);
  }

  return 0;
}
