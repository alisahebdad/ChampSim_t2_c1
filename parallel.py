import os
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

# ===== CONFIG =====
BINARIES = [
    "champsim",
]

WORKLOAD_DIRS = [
    "/home/ownergive/Documents/workload/600.perlbench_s-1273B.champsimtrace.xz",
    "/home/ownergive/Documents/workload/605.mcf_s-484B.champsimtrace.xz",
    "/home/ownergive/Documents/workload/607.cactuBSSN_s-2421B.champsimtrace.xz",
    "/home/ownergive/Documents/workload/620.omnetpp_s-141B.champsimtrace.xz",
    "/home/ownergive/Documents/workload/623.xalancbmk_s-165B.champsimtrace.xz",
    "/home/ownergive/Documents/workload/628.pop2_s-17B.champsimtrace.xz",
    "/home/ownergive/Documents/workload/631.deepsjeng_s-928B.champsimtrace.xz",
    "/home/ownergive/Documents/workload/638.imagick_s-824B.champsimtrace.xz",
    "/home/ownergive/Documents/workload/644.nab_s-5853B.champsimtrace.xz",
    "/home/ownergive/Documents/workload/648.exchange2_s-72B.champsimtrace.xz",
    "/home/ownergive/Documents/workload/649.fotonik3d_s-1B.champsimtrace.xz",
    "/home/ownergive/Documents/workload/657.xz_s-56B.champsimtrace.xz"
]

OUTPUT_DIR = "/home/ownergive/Documents/logs/"
MAX_PARALLEL = 4 # adjust based on CPU
WARMUP = 2*1000*1000
SIM    = 180*1000*1000

# ==================

os.makedirs(OUTPUT_DIR, exist_ok=True)


def run_job(binary, workload_dir):
    binary_name = Path(binary).name
    workload_name = Path(workload_dir).name

    print(workload_name)
    os.system(f"mkdir -p {os.path.join(OUTPUT_DIR,workload_name)}")
    os.system(f"cp bin/{binary}  {os.path.join(OUTPUT_DIR,workload_name)}/")

    os.system(f"echo {binary} {os.path.join(OUTPUT_DIR,workload_name)} > {os.path.join(OUTPUT_DIR,workload_name)}/info.txt")
    os.sync() 
    
    log_file = os.path.join(
        OUTPUT_DIR,
        f"{binary_name}__{workload_name}.log"
    )
    cmd = [
		f"{os.path.join(OUTPUT_DIR,workload_name)}/{binary}",
        "--warmup-instructions", str(WARMUP),
        "--simulation-instructions", str(SIM),
        workload_dir]
    print(f"[START] {binary_name} on {workload_name}")
    with open(log_file, "w") as f:
        print (cmd)
        process = subprocess.Popen(
            cmd,
            cwd=os.path.join(OUTPUT_DIR,workload_name),
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
