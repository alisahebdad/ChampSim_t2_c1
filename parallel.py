import os
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
import shutil

# ===== CONFIG =====
BINARIES = [
    "champsim",
]



WORKLOAD_DIRS = [
    "/home/ali/Documents/workload/600.perlbench_s-1273B.champsimtrace.xz",
    "/home/ali/Documents/workload/602.gcc_s-734B.champsimtrace.xz" ,
    "/home/ali/Documents/workload/603.bwaves_s-891B.champsimtrace.xz" ,
    "/home/ali/Documents/workload/605.mcf_s-484B.champsimtrace.xz",
    "/home/ali/Documents/workload/607.cactuBSSN_s-2421B.champsimtrace.xz",
    "/home/ali/Documents/workload/619.lbm_s-2676B.champsimtrace.xz" ,
    "/home/ali/Documents/workload/621.wrf_s-575B.champsimtrace.xz",
    "/home/ali/Documents/workload/623.xalancbmk_s-165B.champsimtrace.xz",
    "/home/ali/Documents/workload/625.x264_s-12B.champsimtrace.xz" ,
    "/home/ali/Documents/workload/627.cam4_s-490B.champsimtrace.xz" ,    
    "/home/ali/Documents/workload/628.pop2_s-17B.champsimtrace.xz",
    "/home/ali/Documents/workload/631.deepsjeng_s-928B.champsimtrace.xz",
    "/home/ali/Documents/workload/638.imagick_s-824B.champsimtrace.xz",
    "/home/ali/Documents/workload/641.leela_s-149B.champsimtrace.xz" ,
    "/home/ali/Documents/workload/644.nab_s-5853B.champsimtrace.xz",
    "/home/ali/Documents/workload/648.exchange2_s-72B.champsimtrace.xz",
    "/home/ali/Documents/workload/649.fotonik3d_s-1B.champsimtrace.xz",
    "/home/ali/Documents/workload/654.roms_s-293B.champsimtrace.xz" ,
    "/home/ali/Documents/workload/657.xz_s-56B.champsimtrace.xz" 
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
WARMUP = 1*1000*1000
SIM    = 1*1000*1000

# ==================

os.makedirs(OUTPUT_DIR, exist_ok=True)


def run_job(binary, workload_dir):
    binary_name = Path(binary).name
    workload_name = Path(workload_dir).name

    print(f"[INFO] workload = {workload_name}", flush=True)

    out_dir = os.path.join(OUTPUT_DIR, workload_name)

    # create directory
    os.makedirs(out_dir, exist_ok=True)

    # copy binary
    shutil.copy(f"bin/{binary}", out_dir)

    # write info
    with open(os.path.join(out_dir, "info.txt"), "w") as info:
        info.write(f"{binary} {out_dir}\n")

    os.sync()

    log_file = os.path.join(
        OUTPUT_DIR,
        f"{binary_name}__{workload_name}.log"
    )

    cmd = [
        f"{out_dir}/{binary}",
        "--warmup-instructions", str(WARMUP),
        "--simulation-instructions", str(SIM),
        workload_dir
    ]

    print(f"[START] {binary_name} on {workload_name}", flush=True)
    print(f"[CMD] {' '.join(cmd)}", flush=True)

    with open(log_file, "w") as f:

        # stdbuf forces line-buffered stdout/stderr
        full_cmd = [
            "stdbuf",
            "-oL",
            "-eL",
            *cmd
        ]

        process = subprocess.Popen(
            full_cmd,
            cwd=out_dir,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            universal_newlines=True
        )

        # stream output live
        for line in iter(process.stdout.readline, ''):

            if not line:
                break

            line = line.rstrip()

            # print immediately
            print(
                f"[{binary_name}|{workload_name}] {line}",
                flush=True
            )

            # save immediately
            f.write(line + "\n")
            f.flush()

        process.stdout.close()

        return_code = process.wait()

    print(
        f"[DONE] {binary_name} on {workload_name} (rc={return_code})",
        flush=True
    )
    if os.path.exists(f"{out_dir}/access_trace.txt"):
        os.rename(f"{out_dir}/access_trace.txt",f"{out_dir}/access_trace_{workload_name}.txt")

    return log_file

def run_job__(binary, workload_dir):
    binary_name = Path(binary).name
    workload_name = Path(workload_dir).name

    print(workload_name)

    out_dir = os.path.join(OUTPUT_DIR, workload_name)

    os.makedirs(out_dir, exist_ok=True)

    shutil.copy(f"bin/{binary}", out_dir)

    with open(os.path.join(out_dir, "info.txt"), "w") as info:
        info.write(f"{binary} {out_dir}\n")

    os.sync()

    log_file = os.path.join(
        OUTPUT_DIR,
        f"{binary_name}__{workload_name}.log"
    )

    cmd = [
        f"{out_dir}/{binary}",
        "--warmup-instructions", str(WARMUP),
        "--simulation-instructions", str(SIM),
        workload_dir
    ]

    print(f"[START] {binary_name} on {workload_name}")
    print(cmd)

    with open(log_file, "w") as f:

        process = subprocess.Popen(
            cmd,
            cwd=out_dir,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
            universal_newlines=True
        )

        # Read stdout live
        while True:
            stdout_line = process.stdout.readline()
            stderr_line = process.stderr.readline()

            if stdout_line:
                stdout_line = stdout_line.rstrip()
                print(f"[STDOUT][{binary_name}|{workload_name}] {stdout_line}")
                f.write(f"[STDOUT] {stdout_line}\n")
                f.flush()

            if stderr_line:
                stderr_line = stderr_line.rstrip()
                print(f"[STDERR][{binary_name}|{workload_name}] {stderr_line}")
                f.write(f"[STDERR] {stderr_line}\n")
                f.flush()

            # Exit when process finished
            if (
                stdout_line == ""
                and stderr_line == ""
                and process.poll() is not None
            ):
                break

        return_code = process.wait()

    print(f"[DONE] {binary_name} on {workload_name} (rc={return_code})")

    return log_file
def run_job_(binary, workload_dir):
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
