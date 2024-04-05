import glob, re
import pandas as pd

BENCHMARK_POS_REGEX = r"--- BENCHMARK POSITION (\d+) ---"
PT_DATA_REGEX = r"RNGGen:\s+(\d+\.\d+) ms, PathTracing:\s+(\s+\d+\.\d+) ms, Reproject:\s+(\d+\.\d+) ms, AS Build\s+(\d+.\d+) ms"
HR_DATA_REGEX = r"RNGGen:\s+(\d+\.\d+) ms, GBufLayout:\s+(\d+\.\d+) ms, GBufSample:\s+(\d+\.\d+) ms, DI:\s+(\d+\.\d+) ms, DS:\s+(\d+\.\d+) ms, Reproject:\s+(\d+\.\d+) ms, AS Build\s+(\d+.\d+) ms"

FILES = glob.glob("test_*_*")

def write_pt_csv(f, t_interval, outPath):
    lines = f.readlines()
    data = {
        "T Interval": [],
        "Camera Position": [],
        "RNG Generation time (ms)": [],
        "Path Tracing time (ms)": [],
        "Reprojection time (ms)": [],
        "AS Build time (ms)": [],
    }
    benchmarkPos = 0

    for line in lines:
        line = line.strip()

        bp = re.match(BENCHMARK_POS_REGEX, line, re.MULTILINE)
        if bp is not None:
            benchmarkPos = bp.groups()[0]

        m = re.match(PT_DATA_REGEX, line, re.MULTILINE)
        if m is None:
            continue

        (rng, pt, reproj, as_build) = m.groups()
        data["T Interval"].append(t_interval)
        data["Camera Position"].append(benchmarkPos)
        data["RNG Generation time (ms)"].append(rng)
        data["Path Tracing time (ms)"].append(pt)
        data["Reprojection time (ms)"].append(reproj)
        data["AS Build time (ms)"].append(as_build)

    df = pd.DataFrame(data=data)
    df.to_csv(outPath, index=False)

def write_hr_csv(f, t_interval, outPath):
    lines = f.readlines()
    data = {
        "T Interval": [],
        "Camera Position": [],
        "RNG Generation time (ms)": [],
        "GBuffer Layout time (ms)": [],
        "GBuffer Sample time (ms)": [],
        "Direct Illumination time (ms)": [],
        "Deferred Shading time (ms)": [],
        "Reprojection time (ms)": [],
        "AS Build time (ms)": [],
    }

    for line in lines:
        line = line.strip()

        bp = re.match(BENCHMARK_POS_REGEX, line, re.MULTILINE)
        if bp is not None:
            benchmarkPos = bp.groups()[0]

        m = re.match(HR_DATA_REGEX, line, re.MULTILINE)
        if m is None:
            continue

        (rng, gbl, gbs, di, ds, reproj, as_build) = m.groups()
        data["T Interval"].append(t_interval)
        data["Camera Position"].append(benchmarkPos)
        data["RNG Generation time (ms)"].append(rng)
        data["GBuffer Layout time (ms)"].append(gbl)
        data["GBuffer Sample time (ms)"].append(gbs)
        data["Direct Illumination time (ms)"].append(di)
        data["Deferred Shading time (ms)"].append(ds)
        data["Reprojection time (ms)"].append(reproj)
        data["AS Build time (ms)"].append(as_build)

    df = pd.DataFrame(data=data)
    df.to_csv(outPath, index=False)

for file in FILES:
    (_, method, t) = file.split('_')
    method = "PathTracing" if method == "pt" else "HybridRendering"

    print(method, t)
    t_interval = float(t[1:]) / 100.0

    with open(file, "r", encoding="UTF16") as f:
        if method == "PathTracing":
            write_pt_csv(f, t_interval, f"dataset/{method}_{t}.csv")
        elif method == "HybridRendering":
            write_hr_csv(f, t_interval, f"dataset/{method}_{t}.csv")
