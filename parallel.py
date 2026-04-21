import os
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

# ===== CONFIG =====
BINARIES = [
    "bin/champsim_deltartlru",
    "bin/champsim_lru",
    "bin/champsim_modertlru",
    "bin/champsim_rtlru"
]

WORKLOAD_DIRS = [
    "/home/ali/Documents/workload/401.bzip2-7B.champsimtrace.xz",
    "/home/ali/Documents/workload/403.gcc-16B.champsimtrace.xz",
    "/home/ali/Documents/workload/435.gromacs-228B.champsimtrace.xz",
    "/home/ali/Documents/workload/473.astar-42B.champsimtrace.xz",
    "/home/ali/Documents/workload/605.mcf_s-484B.champsimtrace.xz",
    "/home/ali/Documents/workload/462.libquantum-714B.champsimtrace.xz"
]

OUTPUT_DIR = "/home/ali/Documents/logs/"
MAX_PARALLEL = 12  # adjust based on CPU
WARMUP = 20*1000*1000
SIM    = 180*1000*1000

# ==================

os.makedirs(OUTPUT_DIR, exist_ok=True)


def run_job(binary, workload_dir):
    binary_name = Path(binary).name
    workload_name = Path(workload_dir).name

    log_file = os.path.join(
        OUTPUT_DIR,
        f"{binary_name}__{workload_name}.log"
    )

    cmd = [
		binary, 
        "--warmup-instructions", str(WARMUP),
        "--simulation-instructions", str(SIM),
        workload_dir]

    print(f"[START] {binary_name} on {workload_name}")

    with open(log_file, "w") as f:
        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1
        )

        for line in process.stdout:
            line = line.rstrip()
            
            # print to screen (tagged)
            print(f"[{binary_name}|{workload_name}] {line}")
            
            # write to file
            f.write(line + "\n")

        process.wait()

    print(f"[DONE] {binary_name} on {workload_name}")
    return log_file


def main():
    jobs = []

    with ThreadPoolExecutor(max_workers=MAX_PARALLEL) as executor:
        for binary in BINARIES:
            for workload in WORKLOAD_DIRS:
                jobs.append(executor.submit(run_job, binary, workload))

        for future in as_completed(jobs):
            try:
                log = future.result()
                print(f"[LOG SAVED] {log}")
            except Exception as e:
                print(f"[ERROR] {e}")


if __name__ == "__main__":
    main()
