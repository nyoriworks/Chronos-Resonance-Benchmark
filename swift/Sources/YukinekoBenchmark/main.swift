import CShim
import Foundation

// MARK: - 量子負荷方式
enum QuantumMode: String {
    case combined = "H+G+QFT"
}

// MARK: - FFT負荷レベル (3.6μs基準)
enum FFTLoadLevel: String, CaseIterable {
    case load75 = "75%"  // 2.70μs
    case load80 = "80%"  // 2.88μs
    case load85 = "85%"  // 3.06μs
    case load90 = "90%"  // 3.24μs

    var repeats: Int {
        switch self {
        case .load75: return 4
        case .load80: return 5
        case .load85: return 6
        case .load90: return 8
        }
    }

    var fftSize: Int {
        switch self {
        case .load75, .load80: return 64
        case .load85, .load90: return 128
        }
    }

    var label: String {
        switch self {
        case .load75: return "75% (2.70μs)"
        case .load80: return "80% (2.88μs)"
        case .load85: return "85% (3.06μs)"
        case .load90: return "90% (3.24μs)"
        }
    }
}

// MARK: - CPU Calibration
struct CalibrationData {
    let cpuFreqHz: UInt64
    let tickCenter: UInt64  // 277.3 kHz
    let tickMinus1: UInt64  // 276.3 kHz
    let tickPlus1: UInt64  // 278.3 kHz

    var cpuFreqGHz: Double {
        return Double(cpuFreqHz) / 1e9
    }
}

@inline(__always)
func getCycleCount() -> UInt64 {
    return shim_rdtsc()
}

// CPU周波数を100ms間測定
func measureCPUFrequency() -> UInt64 {
    let startTime = Date()
    let startCycles = getCycleCount()

    Thread.sleep(forTimeInterval: 0.1)  // 100ms

    let endTime = Date()
    let endCycles = getCycleCount()

    let durationUs = endTime.timeIntervalSince(startTime) * 1_000_000
    let cycles = endCycles - startCycles

    return UInt64(Double(cycles) * 1_000_000 / durationUs)
}

// 周波数からTick値を計算
func calculateTicksFromFrequency(_ cpuFreqHz: UInt64, targetFreqHz: Double) -> UInt64 {
    let periodUs = 1_000_000.0 / targetFreqHz
    let seconds = periodUs / 1_000_000.0
    return UInt64(Double(cpuFreqHz) * seconds)
}

// CPUキャリブレーション
func calibrateCPU() -> CalibrationData {
    let measured = measureCPUFrequency()

    // Apple Siliconは24MHzベースクロック
    let cpuFreqHz: UInt64 = (measured < 20_000_000) ? 24_000_000 : measured

    return CalibrationData(
        cpuFreqHz: cpuFreqHz,
        tickCenter: calculateTicksFromFrequency(cpuFreqHz, targetFreqHz: 277_300.0),
        tickMinus1: calculateTicksFromFrequency(cpuFreqHz, targetFreqHz: 276_300.0),
        tickPlus1: calculateTicksFromFrequency(cpuFreqHz, targetFreqHz: 278_300.0)
    )
}

// MARK: - Complex Number (for FFT)
struct FFTComplex {
    var re: Double
    var im: Double

    init(_ r: Double = 0, _ i: Double = 0) {
        self.re = r
        self.im = i
    }

    static func + (lhs: FFTComplex, rhs: FFTComplex) -> FFTComplex {
        return FFTComplex(lhs.re + rhs.re, lhs.im + rhs.im)
    }

    static func - (lhs: FFTComplex, rhs: FFTComplex) -> FFTComplex {
        return FFTComplex(lhs.re - rhs.re, lhs.im - rhs.im)
    }

    static func * (lhs: FFTComplex, rhs: FFTComplex) -> FFTComplex {
        return FFTComplex(
            lhs.re * rhs.re - lhs.im * rhs.im,
            lhs.re * rhs.im + lhs.im * rhs.re)
    }
}

// MARK: - Real Cooley-Tukey FFT
func fft(_ data: inout [FFTComplex], inverse: Bool = false) {
    let n = data.count
    guard n > 1 else { return }

    // Bit-reversal permutation
    var j = 0
    for i in 1..<n {
        var bit = n >> 1
        while (j & bit) != 0 {
            j ^= bit
            bit >>= 1
        }
        j ^= bit
        if i < j {
            data.swapAt(i, j)
        }
    }

    // Cooley-Tukey iterative FFT
    var len = 2
    while len <= n {
        let angle = 2.0 * Double.pi / Double(len) * (inverse ? -1.0 : 1.0)
        let wlen = FFTComplex(cos(angle), sin(angle))

        var i = 0
        while i < n {
            var w = FFTComplex(1.0, 0.0)
            for k in 0..<(len / 2) {
                let u = data[i + k]
                let v = data[i + k + len / 2] * w
                data[i + k] = u + v
                data[i + k + len / 2] = u - v
                w = w * wlen
            }
            i += len
        }
        len <<= 1
    }

    if inverse {
        for i in 0..<n {
            data[i].re /= Double(n)
            data[i].im /= Double(n)
        }
    }
}

// MARK: - FFT Load (Real Implementation)
@inline(__always)
func performFFTLoad(level: FFTLoadLevel) {
    let size = level.fftSize
    let repeats = level.repeats

    var data = [FFTComplex](repeating: FFTComplex(), count: size)

    // Initialize with sine wave
    for i in 0..<size {
        data[i] = FFTComplex(sin(2.0 * Double.pi * Double(i) / Double(size)), 0)
    }

    // Perform FFT multiple times
    for _ in 0..<repeats {
        fft(&data, inverse: false)
        fft(&data, inverse: true)
    }
}

// MARK: - Qubit (Same as before)
struct Complex {
    var real: Double
    var imag: Double

    static func + (lhs: Complex, rhs: Complex) -> Complex {
        return Complex(real: lhs.real + rhs.real, imag: lhs.imag + rhs.imag)
    }

    static func - (lhs: Complex, rhs: Complex) -> Complex {
        return Complex(real: lhs.real - rhs.real, imag: lhs.imag - rhs.imag)
    }

    static func * (lhs: Complex, rhs: Complex) -> Complex {
        return Complex(
            real: lhs.real * rhs.real - lhs.imag * rhs.imag,
            imag: lhs.real * rhs.imag + lhs.imag * rhs.real
        )
    }

    var squaredModulus: Double {
        return real * real + imag * imag
    }
}

struct Qubit {
    var alpha: Complex
    var beta: Complex

    init() {
        self.alpha = Complex(real: 1.0, imag: 0.0)
        self.beta = Complex(real: 0.0, imag: 0.0)
    }

    mutating func applyHadamard() {
        let invSqrt2 = 0.70710678118
        let newAlpha = Complex(
            real: (alpha.real + beta.real) * invSqrt2,
            imag: (alpha.imag + beta.imag) * invSqrt2
        )
        let newBeta = Complex(
            real: (alpha.real - beta.real) * invSqrt2,
            imag: (alpha.imag - beta.imag) * invSqrt2
        )
        alpha = newAlpha
        beta = newBeta
    }

    mutating func applyX() {
        let temp = alpha
        alpha = beta
        beta = temp
    }

    mutating func applyZ() {
        beta = Complex(real: -beta.real, imag: -beta.imag)
    }

    mutating func applyS() {
        beta = Complex(real: -beta.imag, imag: beta.real)
    }

    mutating func applyT() {
        let cos45 = 0.70710678118
        let sin45 = 0.70710678118
        let newBeta = Complex(
            real: beta.real * cos45 - beta.imag * sin45,
            imag: beta.real * sin45 + beta.imag * cos45
        )
        beta = newBeta
    }

    mutating func applyRY(theta: Double) {
        let cosHalf = cos(theta / 2)
        let sinHalf = sin(theta / 2)
        let newAlpha = Complex(
            real: cosHalf * alpha.real - sinHalf * beta.real,
            imag: cosHalf * alpha.imag - sinHalf * beta.imag
        )
        let newBeta = Complex(
            real: sinHalf * alpha.real + cosHalf * beta.real,
            imag: sinHalf * alpha.imag + cosHalf * beta.imag
        )
        alpha = newAlpha
        beta = newBeta
    }

    func measure() -> Int {
        let p0 = alpha.squaredModulus
        return Double.random(in: 0...1) < p0 ? 0 : 1
    }
}

// MARK: - Quantum Load
@inline(__always)
func performQuantumLoad() {
    var q = Qubit()

    // Hadamard Walk (150 iterations)
    q.applyHadamard()
    for _ in 0..<150 {
        q.applyX()
        q.applyHadamard()
        shim_nop()
    }

    // Grover (75 iterations)
    for _ in 0..<75 {
        q.applyZ()
        q.applyHadamard()
        q.applyX()
        q.applyZ()
        q.applyX()
        q.applyHadamard()
        shim_nop()
    }

    // QFT (150 iterations)
    for i in 0..<150 {
        let angle = Double.pi / Double(1 << (i % 8 + 1))
        q.applyRY(theta: angle)
        q.applyS()
        q.applyT()
        shim_nop()
    }

    let _ = q.measure()
}

// MARK: - Single Measurement
func measureSingle(tick: UInt64, fftLevel: FFTLoadLevel) -> Int {
    // Baseline
    let baseStart = getCycleCount()
    var baseOps: UInt64 = 0
    while (getCycleCount() - baseStart) < tick {
        baseOps += 1
        shim_nop()
    }
    let idleStart1 = getCycleCount()
    while (getCycleCount() - idleStart1) < tick {
        shim_nop()
    }

    // Loaded
    let loadStart = getCycleCount()
    performFFTLoad(level: fftLevel)
    performQuantumLoad()
    var loadOps: UInt64 = 0
    while (getCycleCount() - loadStart) < tick {
        loadOps += 1
        shim_nop()
    }
    let idleStart2 = getCycleCount()
    while (getCycleCount() - idleStart2) < tick {
        shim_nop()
    }

    return Int(baseOps) - Int(loadOps)
}

// MARK: - Analysis
func analyze(name: String, data: [Int]) {
    let sum = data.reduce(0, +)
    let avg = Double(sum) / Double(data.count)

    let variance = data.map { pow(Double($0) - avg, 2) }.reduce(0, +) / Double(data.count)
    let stdDev = sqrt(variance)

    let minVal = data.min() ?? 0
    let maxVal = data.max() ?? 0

    print("\(name):")
    print("  Average: \(String(format: "%.2f", avg))")
    print("  Std Dev: \(String(format: "%.2f", stdDev))")
    print("  Range: [\(minVal), \(maxVal)]")

    // Histogram (bins of 20)
    var histogram = [Int: Int]()
    for value in data {
        let bin = (value / 20) * 20
        histogram[bin, default: 0] += 1
    }

    print("  Histogram (Top 10 bins):")
    let sorted = histogram.sorted { $0.value > $1.value }.prefix(10)
    for (bin, count) in sorted {
        let percentage = Double(count) / Double(data.count) * 100.0
        print("    [\(bin)-\(bin+19)]: \(count) (\(String(format: "%.2f", percentage))%)")
    }
    print("")
}

// MARK: - Main
@main
struct QuantumTransitionBenchmark {
    static func main() {
        let _ = pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0)

        print("=== Quantum Transition Measurement (Swift - Imported from C++) ===")
        print("Auto-calibrating for your CPU...\n")

        let cal = calibrateCPU()
        print("CPU Frequency: \(String(format: "%.2f", cal.cpuFreqGHz)) GHz")
        print("Calibrated for 3.6μs base period (277.3 kHz region):")
        print("  Tick \(cal.tickMinus1) → 276.3 kHz (3.619 μs)")
        print("  Tick \(cal.tickCenter) → 277.3 kHz (3.606 μs, center)")
        print("  Tick \(cal.tickPlus1) → 278.3 kHz (3.593 μs)")

        print("\nTarget: 277.3 kHz region (±1 kHz)")
        print("Base Period: 3.6 microseconds")
        print("Iterations: 1,000,000 per measurement")
        print("============================================================\n")

        // Warmup
        print("Warming up...")
        let warmupStart = getCycleCount()
        while (getCycleCount() - warmupStart) < 72_000_000 { shim_nop() }
        print("Done.\n")

        let iterations = 1_000_000

        // Part 1: 12 Patterns (4 FFT x 3 Ticks)
        print("Part 1: Combined FFT + Tick Variation Tests")
        print("--------------------------------------------")
        print("FFT Load: 75% (2.70μs), 80% (2.88μs), 85% (3.06μs), 90% (3.24μs)")
        print("Tick Variation: -1 (276.3kHz), 0 (277.3kHz), +1 (278.3kHz)")
        print("12 combinations x 1M samples each\n")

        var results = [String: [Int]]()

        for fftLevel in FFTLoadLevel.allCases {
            for (tickName, tick) in [
                ("-1", cal.tickMinus1), ("0", cal.tickCenter), ("+1", cal.tickPlus1),
            ] {
                let key = "FFT\(fftLevel.rawValue) Tick\(tickName)"
                print("\(key) (1M)...", terminator: "")
                fflush(stdout)

                var data = [Int]()
                for _ in 0..<iterations {
                    data.append(measureSingle(tick: tick, fftLevel: fftLevel))
                }
                results[key] = data
                print(" done")
            }
        }

        // Part 2: Dynamic Transition
        print("\nPart 2: Dynamic Transition (4 FFT patterns)")
        print("--------------------------------------------")
        print("Pattern: -1→-1→0→+1→+1→0 (1M iterations each)\n")

        let pattern = [
            cal.tickMinus1, cal.tickMinus1, cal.tickCenter, cal.tickPlus1, cal.tickPlus1,
            cal.tickCenter,
        ]

        for fftLevel in FFTLoadLevel.allCases {
            let key = "Dynamic FFT\(fftLevel.rawValue)"
            print("\(key) (1M)...", terminator: "")
            fflush(stdout)

            var data = [Int]()
            for i in 0..<iterations {
                let tick = pattern[i % 6]
                data.append(measureSingle(tick: tick, fftLevel: fftLevel))
            }
            results[key] = data
            print(" done")
        }

        // Analysis
        print("\n============================================================")
        print("Statistical Analysis")
        print("============================================================\n")

        for fftLevel in FFTLoadLevel.allCases {
            print("--- FFT \(fftLevel.label) + Tick Variations ---\n")
            for tickName in ["-1", "0", "+1"] {
                let key = "FFT\(fftLevel.rawValue) Tick\(tickName)"
                if let data = results[key] {
                    analyze(name: key, data: data)
                }
            }
        }

        print("--- Dynamic Transition (4 FFT patterns) ---\n")
        for fftLevel in FFTLoadLevel.allCases {
            let key = "Dynamic FFT\(fftLevel.rawValue)"
            if let data = results[key] {
                analyze(name: key, data: data)
            }
        }

        print("============================================================")
        print("Done.")
    }
}
