import argparse
import ast
import collections
import concurrent.futures
import hashlib
import io
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import threading
import time
import tokenize
import _winapi
import urllib.request
import zipfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOCK_PATH = os.path.join(REPO_ROOT, "toolchain.lock.json")
LOCK = json.load(open(LOCK_PATH, encoding="utf-8"))
ARTIFACTS = os.path.join(REPO_ROOT, "artifacts")
VERSION_HEADER = os.path.join(REPO_ROOT, "Firmware", "include", "GlobalDefinition.h")
READ_ME = os.path.join(REPO_ROOT, "README.md")
PROTOCOL_DOCUMENT = os.path.join(REPO_ROOT, "Protocol Specification.md")
CMAKE_LISTS = os.path.join(REPO_ROOT, "CMakeLists.txt")
SIMULATION_SOURCE = os.path.join(REPO_ROOT, "Tests", "Simulation", "FadSimulation.c")
SIMULATION_EXPECTED = os.path.join(REPO_ROOT, "Tests", "Simulation", "Expected.txt")
HOST_TESTS = os.path.join(REPO_ROOT, "Tests", "HostTests.cpp")

SOURCE_DIRECTORIES = ("Firmware", "Tests")
SOURCE_SUFFIXES = (".c", ".cpp", ".h")
TEXT_SUFFIXES = SOURCE_SUFFIXES + (".py", ".cmake", ".txt", ".json", ".md")
SKIP_DIRECTORIES = {".git", ".vs", ".vscode", "artifacts", "build", "__pycache__"}
MAX_COLUMNS = 170
FLASH_BYTES = 262144
RAM_BYTES = 8192
MCU = "atmega2560"
BUILD_TIMEOUT_SECONDS = 300
TEST_TIMEOUT_SECONDS = 120
SIMULATION_TIMEOUT_SECONDS = 900
DOWNLOAD_TIMEOUT_SECONDS = 60
CONFIGURATIONS = {"Release": "release", "Debug": "debug"}
WSL_HARNESS = "/root/.fad/FadSimulation"
SHORT_ROOT_LIMIT = 120
CYCLES_PER_MICROSECOND = 16


LOG_CAPTURE = threading.local()


def log(message):
    lines = getattr(LOG_CAPTURE, "lines", None)
    if lines is None:
        print(message, flush=True)
    else:
        lines.append(message)


def captured(function, *arguments):
    LOG_CAPTURE.lines = []
    try:
        return function(*arguments), LOG_CAPTURE.lines
    finally:
        LOG_CAPTURE.lines = None


def replay(result):
    value, lines = result
    for line in lines:
        log(line)
    return value


def tools_root():
    configured = os.environ.get("FAD_TOOLS_ROOT")
    if configured:
        return configured
    return os.path.join(os.environ.get("LOCALAPPDATA", os.path.expanduser("~")), *LOCK["tools-root"].split("/"))


def tool_directory(name):
    return os.path.join(tools_root(), LOCK[name]["directory"])


def avr_tool(name):
    return os.path.join(tool_directory("avr-gcc"), "bin", name + ".exe")


def effective_root(source_root):
    if len(source_root) <= SHORT_ROOT_LIMIT and source_root.isascii():
        return source_root

    link_parent = os.path.join(os.environ.get("LOCALAPPDATA", tempfile.gettempdir()), "BioSight_FAD", "roots")
    os.makedirs(link_parent, exist_ok=True)
    link = os.path.join(link_parent, hashlib.sha256(os.path.normcase(source_root).encode("utf-8")).hexdigest()[:12])

    if os.path.isdir(link) and os.path.samefile(link, source_root):
        return link
    if os.path.lexists(link):
        os.rmdir(link)
    _winapi.CreateJunction(source_root, link)
    return link


def run(command, timeout, cwd=REPO_ROOT, env=None):
    started = time.time()
    try:
        completed = subprocess.run(command, cwd=cwd, env=env, capture_output=True, text=True, encoding="utf-8",
                                   errors="replace", timeout=timeout, check=False)
    except FileNotFoundError:
        return 127, "실행 파일을 찾지 못했다: " + str(command[0]), time.time() - started
    except subprocess.TimeoutExpired:
        return 124, "{0}초 제한을 넘겨 중단했다: {1}".format(timeout, " ".join(command)), time.time() - started
    return completed.returncode, completed.stdout + completed.stderr, time.time() - started


def tail(text, lines=30):
    return "\n".join(text.strip().splitlines()[-lines:])


def write_report(name, report):
    directory = os.path.join(ARTIFACTS, name)
    os.makedirs(directory, exist_ok=True)
    with open(os.path.join(directory, "report.json"), "w", encoding="utf-8") as handle:
        json.dump(report, handle, ensure_ascii=False, indent=2)


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def install_archive(name):
    entry = LOCK[name]
    target = tool_directory(name)
    marker = os.path.join(target, ".sha256")
    if os.path.isfile(marker) and open(marker, encoding="utf-8").read().strip() == entry["sha256"]:
        log("  {0} {1}: 이미 설치됨".format(name, entry["version"]))
        return True

    downloads = os.path.join(tools_root(), "downloads")
    os.makedirs(downloads, exist_ok=True)
    archive = os.path.join(downloads, entry["url"].rsplit("/", 1)[1])

    if not os.path.isfile(archive) or sha256(archive) != entry["sha256"]:
        log("  {0} {1}: 내려받는 중".format(name, entry["version"]))
        try:
            with urllib.request.urlopen(entry["url"], timeout=DOWNLOAD_TIMEOUT_SECONDS) as response, open(archive, "wb") as handle:
                shutil.copyfileobj(response, handle, 1 << 20)
        except OSError as error:
            log("  {0}: 내려받지 못했다 ({1}). 인터넷 연결을 확인한 뒤 다시 실행한다.".format(name, error))
            return False

    actual = sha256(archive)
    if actual != entry["sha256"]:
        log("  {0}: 내려받은 파일의 SHA-256 이 잠금 값과 다르다. 기대 {1}, 실제 {2}".format(name, entry["sha256"], actual))
        return False

    extract_root = os.path.join(tools_root(), "." + name + "-extract")
    shutil.rmtree(extract_root, ignore_errors=True)
    with zipfile.ZipFile(archive) as bundle:
        bundle.extractall(extract_root)

    entries = os.listdir(extract_root)
    source = os.path.join(extract_root, entries[0]) if len(entries) == 1 and os.path.isdir(os.path.join(extract_root, entries[0])) else extract_root
    shutil.rmtree(target, ignore_errors=True)
    shutil.move(source, target)
    shutil.rmtree(extract_root, ignore_errors=True)

    with open(marker, "w", encoding="utf-8") as handle:
        handle.write(entry["sha256"])
    log("  {0} {1}: 설치 완료 ({2})".format(name, entry["version"], target))
    return True


def find_clang():
    for directory in (os.environ.get("FAD_LLVM_BIN", ""), os.path.join(os.environ.get("ProgramFiles", "C:/Program Files"), "LLVM", "bin")):
        if directory and os.path.isfile(os.path.join(directory, "clang++.exe")):
            return os.path.join(directory, "clang++.exe"), os.path.join(directory, "llvm-rc.exe")

    compiler = shutil.which("clang++")
    if compiler:
        return compiler, shutil.which("llvm-rc") or os.path.join(os.path.dirname(compiler), "llvm-rc.exe")
    return None, None


def wsl(arguments, timeout):
    distribution = LOCK["host"]["wsl-distribution"]
    return run(["wsl.exe", "-d", distribution, "-u", "root", "--"] + arguments, timeout)


def to_wsl_path(path):
    absolute = os.path.abspath(path).replace("\\", "/")
    return "/mnt/" + absolute[0].lower() + absolute[2:]


def setup_archives():
    return install_archive("avr-gcc") & install_archive("avrdude")


def setup_python():
    packages = ["{0}=={1}".format(name, version) for name, version in LOCK["python"].items()]
    code, output, _ = run([sys.executable, "-m", "pip", "install", "--disable-pip-version-check", "-q"] + packages, 600)
    log("  Python 도구 {0}: {1}".format(", ".join(packages), "설치 완료" if code == 0 else "실패"))
    if code != 0:
        log(tail(output))
    return code == 0


def setup_llvm():
    compiler, _ = find_clang()
    if compiler is None:
        run(["winget", "install", "--id", "LLVM.LLVM", "--exact", "--version", LOCK["host"]["llvm"], "--silent",
             "--accept-package-agreements", "--accept-source-agreements"], 1800)
        compiler, _ = find_clang()
    log("  LLVM(호스트 시험): {0}".format(compiler or "설치하지 못했다. winget 으로 LLVM.LLVM 을 설치한다."))
    return compiler is not None


def setup_simulator():
    code, output, _ = wsl(["bash", "-c", "command -v gcc && test -f /usr/include/simavr/sim_avr.h"], 60)
    if code != 0:
        log("  시뮬레이터: WSL 에 simavr 를 설치하는 중")
        code, output, _ = wsl(["bash", "-c", "DEBIAN_FRONTEND=noninteractive apt-get update -qq && DEBIAN_FRONTEND=noninteractive "
                                             "apt-get install -y -qq gcc libsimavr-dev libelf-dev"], 1800)
    log("  시뮬레이터(WSL {0}, simavr): {1}".format(LOCK["host"]["wsl-distribution"], "준비됨" if code == 0 else "준비하지 못했다"))
    if code != 0:
        log("  wsl --install -d {0} 로 배포판을 설치한 뒤 다시 실행한다.".format(LOCK["host"]["wsl-distribution"]))
    return code == 0


WINDOWS_SETUP = (setup_archives, setup_python, setup_llvm)


def run_setup(parts, pool):
    futures = [pool.submit(captured, part) for part in parts]
    return all([replay(future.result()) for future in futures])


def command_setup(_arguments):
    with concurrent.futures.ThreadPoolExecutor(max_workers=len(WINDOWS_SETUP) + 1) as pool:
        return 0 if run_setup(WINDOWS_SETUP + (setup_simulator,), pool) else 1


def repository_files():
    for current, directories, files in os.walk(REPO_ROOT):
        directories[:] = [name for name in directories if name not in SKIP_DIRECTORIES]
        for name in files:
            yield os.path.join(current, name)


def c_comment_column(line):
    in_text = False
    in_char = False
    index = 0
    while index < len(line):
        current = line[index]
        if current == "\\" and (in_text or in_char):
            index += 2
            continue
        if current == '"' and not in_char:
            in_text = not in_text
        elif current == "'" and not in_text:
            in_char = not in_char
        elif not in_text and not in_char and current == "/" and index + 1 < len(line) and line[index + 1] in "/*":
            return index
        index += 1
    return -1


def python_comment_lines(text):
    lines = [token.start[0] for token in tokenize.generate_tokens(io.StringIO(text).readline) if token.type == tokenize.COMMENT]
    tree = ast.parse(text)
    for node in ast.walk(tree):
        if isinstance(node, (ast.Module, ast.FunctionDef, ast.ClassDef, ast.AsyncFunctionDef)) and ast.get_docstring(node) is not None:
            lines.append(node.body[0].lineno)
    return lines


def check_text_files(failures):
    checked = 0
    for path in repository_files():
        relative = os.path.relpath(path, REPO_ROOT).replace(os.sep, "/")
        if not relative.endswith(TEXT_SUFFIXES):
            continue

        raw = open(path, "rb").read()
        checked += 1
        if raw.startswith(b"\xef\xbb\xbf"):
            failures.append("UTF-8 BOM 을 빼야 한다 - " + relative)
        if b"\r" in raw:
            failures.append("줄 끝은 LF 여야 한다 - " + relative)

        try:
            text = raw.decode("utf-8-sig")
        except UnicodeDecodeError:
            failures.append("UTF-8 로 읽을 수 없다 - " + relative)
            continue

        is_code = relative.endswith(SOURCE_SUFFIXES + (".py", ".cmake")) or relative == "CMakeLists.txt"
        if not is_code:
            continue

        for number, line in enumerate(text.splitlines(), start=1):
            if len(line) > MAX_COLUMNS:
                failures.append("{0}열을 넘는다 - {1}:{2}".format(MAX_COLUMNS, relative, number))
            if relative.endswith(SOURCE_SUFFIXES) and c_comment_column(line) >= 0:
                failures.append("주석이 남아 있다 - {0}:{1}".format(relative, number))
            if (relative.endswith(".cmake") or relative == "CMakeLists.txt") and line.lstrip().startswith("#"):
                failures.append("주석이 남아 있다 - {0}:{1}".format(relative, number))

        if relative.endswith(".py"):
            for number in python_comment_lines(text):
                failures.append("주석이나 설명 문자열이 남아 있다 - {0}:{1}".format(relative, number))

    log("  텍스트 파일 {0}개: UTF-8(BOM 없음)·LF·170열·주석 검사".format(checked))


def check_build_lists(failures):
    listed = set(re.findall(r"^\s+((?:Firmware|Tests)/[\w/]+\.(?:cpp|c))\s*$", open(CMAKE_LISTS, encoding="utf-8").read(), re.MULTILINE))
    on_disk = set()
    for directory in SOURCE_DIRECTORIES:
        for current, _, files in os.walk(os.path.join(REPO_ROOT, directory)):
            for name in files:
                if name.endswith(".cpp"):
                    on_disk.add(os.path.relpath(os.path.join(current, name), REPO_ROOT).replace(os.sep, "/"))

    for path in sorted(on_disk - listed):
        failures.append("CMakeLists.txt 에 없는 소스 - " + path)
    for path in sorted(listed - on_disk):
        failures.append("CMakeLists.txt 에만 있고 파일이 없다 - " + path)
    log("  빌드 목록: 소스 {0}개, 등록 {1}개".format(len(on_disk), len(listed)))


def firmware_version():
    match = re.search(r'#define\s+VERSION\s+\("([^"]+)"\)', open(VERSION_HEADER, encoding="utf-8").read())
    return match.group(1) if match else None


def check_version(failures):
    version = firmware_version()
    if version is None:
        failures.append("GlobalDefinition.h 에서 VERSION 을 찾지 못했다")
        return

    anchors = [
        (READ_ME, "- 펌웨어 버전: `{0}`", "README 머리말"),
        (PROTOCOL_DOCUMENT, "수신: <STX>GVER {0}<ETX>", "프로토콜 기술서 GVER 예제"),
    ]
    for path, pattern, where in anchors:
        if pattern.format(version) not in open(path, encoding="utf-8").read():
            failures.append("{0} 의 버전이 {1} 과 다르다".format(where, version))

    lock_text = open(LOCK_PATH, encoding="utf-8").read()
    log("  펌웨어 버전 {0}, 툴체인 잠금 {1}바이트".format(version, len(lock_text)))


def check_protocol_examples(failures):
    document = open(PROTOCOL_DOCUMENT, encoding="utf-8").read()
    literals = set(re.findall(r'"((?:[^"\\]|\\.)*)"', open(HOST_TESTS, encoding="utf-8").read()))

    host_examples = re.findall(r"^(?:송신|수신): <STX>(.*?)<ETX>", document, re.MULTILINE)
    kisan_examples = re.findall(r"^송신: (#[^<\s]*)<CR>", document, re.MULTILINE)

    for body in host_examples:
        if not body.startswith("GVER ") and body not in literals:
            failures.append("프로토콜 기술서의 상위 PC 예제가 시험에 없다 - " + body)

    for frame in kisan_examples:
        if frame + "\\r" not in literals:
            failures.append("프로토콜 기술서의 KiSAN 예제가 시험에 없다 - " + frame)

    log("  프로토콜 예제: 상위 PC {0}건, KiSAN 송신 {1}건".format(len(host_examples), len(kisan_examples)))


def command_check(_arguments):
    failures = []
    log("[check] 텍스트 파일 규칙")
    check_text_files(failures)
    log("[check] 빌드 목록")
    check_build_lists(failures)
    log("[check] 버전 표기")
    check_version(failures)
    log("[check] 프로토콜 기술서 예제와 시험 대조")
    check_protocol_examples(failures)

    if failures:
        log("[check] 실패 {0}건".format(len(failures)))
        for item in failures:
            log("  - " + item)
        return 1

    log("[check] 통과")
    return 0


def section_sizes(elf):
    code, output, _ = run([avr_tool("avr-size"), "-A", elf], 60)
    sizes = {}
    if code != 0:
        return sizes
    for line in output.splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[0].startswith(".") and parts[1].isdigit():
            sizes[parts[0]] = int(parts[1])
    return sizes


def build_configuration(configuration, source_root=REPO_ROOT):
    source_root = effective_root(source_root)
    preset = CONFIGURATIONS[configuration]
    build_directory = os.path.join(source_root, "build", preset)
    steps = []
    if not os.path.isfile(os.path.join(build_directory, "build.ninja")):
        steps.append([sys.executable, "-m", "cmake", "--preset", preset])
    steps.append([sys.executable, "-m", "cmake", "--build", "--preset", preset])

    started = time.time()
    output = ""
    for step in steps:
        code, text, _ = run(step, BUILD_TIMEOUT_SECONDS, cwd=source_root)
        output += text
        if code != 0:
            return {"configuration": configuration, "succeeded": False, "seconds": round(time.time() - started, 2), "log": tail(output)}

    elf = os.path.join(build_directory, "FAD.elf")
    sizes = section_sizes(elf)
    program = sizes.get(".text", 0) + sizes.get(".data", 0)
    data = sizes.get(".data", 0) + sizes.get(".bss", 0) + sizes.get(".noinit", 0)
    warnings = [line for line in output.splitlines() if "warning:" in line]
    return {
        "configuration": configuration,
        "succeeded": not warnings,
        "seconds": round(time.time() - started, 2),
        "warnings": warnings,
        "program_bytes": program,
        "program_percent": round(program * 100.0 / FLASH_BYTES, 1),
        "data_bytes": data,
        "data_percent": round(data * 100.0 / RAM_BYTES, 1),
        "version": firmware_version(),
        "elf": os.path.relpath(elf, source_root).replace(os.sep, "/"),
    }


def report_build(report):
    status = "성공" if report["succeeded"] else "실패"
    log("[build] {0}: {1}, {2}초".format(report["configuration"], status, report["seconds"]))
    if "program_bytes" in report:
        log("  프로그램 {0}바이트 ({1} %), 데이터 {2}바이트 ({3} %)".format(
            report["program_bytes"], report["program_percent"], report["data_bytes"], report["data_percent"]))
    for line in report.get("warnings", [])[:10]:
        log("  경고: " + line)
    if "log" in report:
        log(report["log"])
    write_report("build-" + report["configuration"].lower(), report)


def command_build(arguments):
    configurations = list(CONFIGURATIONS) if arguments.configuration == "all" else [arguments.configuration]
    with concurrent.futures.ThreadPoolExecutor(max_workers=len(configurations)) as pool:
        reports = list(pool.map(build_configuration, configurations))
    for report in reports:
        report_build(report)
    return 0 if all(report["succeeded"] for report in reports) else 1


def run_host_tests(source_root=REPO_ROOT):
    source_root = effective_root(source_root)
    compiler, resource_compiler = find_clang()
    if compiler is None:
        return {"succeeded": False, "log": "clang++ 를 찾지 못했다. python Scripts/verify.py setup 을 먼저 실행한다."}

    build_directory = os.path.join(source_root, "build", "host-tests")
    started = time.time()
    output = ""
    if not os.path.isfile(os.path.join(build_directory, "build.ninja")):
        code, text, _ = run([sys.executable, "-m", "cmake", "--preset", "host-tests", "-DCMAKE_CXX_COMPILER=" + compiler.replace(os.sep, "/"),
                             "-DCMAKE_RC_COMPILER=" + resource_compiler.replace(os.sep, "/")], BUILD_TIMEOUT_SECONDS, cwd=source_root)
        output += text
        if code != 0:
            return {"succeeded": False, "seconds": round(time.time() - started, 2), "log": tail(output)}

    code, text, _ = run([sys.executable, "-m", "cmake", "--build", "--preset", "host-tests"], BUILD_TIMEOUT_SECONDS, cwd=source_root)
    output += text
    if code != 0 or "warning:" in text:
        return {"succeeded": False, "seconds": round(time.time() - started, 2), "log": tail(output)}

    code, text, _ = run([os.path.join(build_directory, "HostTests.exe")], TEST_TIMEOUT_SECONDS, cwd=source_root)
    summary = re.search(r"tests (\d+), passed (\d+), failed (\d+), checks (\d+)", text)
    return {
        "succeeded": code == 0 and summary is not None,
        "seconds": round(time.time() - started, 2),
        "tests": int(summary.group(1)) if summary else 0,
        "passed": int(summary.group(2)) if summary else 0,
        "checks": int(summary.group(4)) if summary else 0,
        "failures": [line for line in text.splitlines() if line.startswith("FAIL") or line.startswith("  ")],
    }


def report_tests(report):
    status = "통과" if report["succeeded"] else "실패"
    log("[test] 호스트 시험 {0}: {1}/{2}개 통과, 검사 {3}건, {4}초".format(
        status, report.get("passed", 0), report.get("tests", 0), report.get("checks", 0), report.get("seconds", 0)))
    for line in report.get("failures", [])[:20]:
        log("  " + line)
    if "log" in report:
        log(report["log"])
    write_report("host-tests", report)


def command_test(_arguments):
    report = run_host_tests()
    report_tests(report)
    return 0 if report["succeeded"] else 1


def transcript_lines(output, version):
    lines = []
    for line in output.splitlines():
        if line.startswith("RESP ") or line.startswith("PORT "):
            lines.append(line.replace(version, "<VERSION>") if version else line)
    return lines


def loop_entry_address(elf):
    code, output, _ = run([avr_tool("avr-nm"), "-C", elf], 60)
    for line in output.splitlines():
        if line.rstrip().endswith("Application::RunCycle()"):
            return line.split()[0]
    return None


def build_harness():
    code, output, _ = wsl(["mkdir", "-p", os.path.dirname(WSL_HARNESS)], 60)
    if code != 0:
        return "WSL 을 실행하지 못했다. python Scripts/verify.py setup 을 먼저 실행한다.\n" + tail(output)

    code, output, _ = wsl(["gcc", "-O2", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-I/usr/include/simavr", "-o", WSL_HARNESS,
                           to_wsl_path(SIMULATION_SOURCE), "-lsimavr", "-lelf"], 300)
    if code != 0:
        return "시뮬레이션 도구를 컴파일하지 못했다.\n" + tail(output)
    return None


def simulate_elf(elf, variables=()):
    address = loop_entry_address(elf)
    if address is None:
        return {"succeeded": False, "log": "{0} 에서 Application::RunCycle 주소를 찾지 못했다.".format(elf)}

    started = time.time()
    prefix = ["env"] + list(variables) if variables else []
    code, output, _ = wsl(prefix + [WSL_HARNESS, to_wsl_path(elf), address], SIMULATION_TIMEOUT_SECONDS)
    metrics = {}
    windows = []
    samples = {}
    for line in output.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[0] == "METRIC":
            metrics[parts[1]] = float(parts[2])
        elif len(parts) == 5 and parts[0] == "WINDOW":
            windows.append((parts[1], parts[2], int(parts[3]), int(parts[4])))
        elif len(parts) == 3 and parts[0] == "PROFILE":
            samples[int(parts[1], 16)] = int(parts[2])
    return {"succeeded": code == 0, "seconds": round(time.time() - started, 1), "output": output, "metrics": metrics, "windows": windows,
            "samples": samples, "log": "" if code == 0 else tail(output)}


def run_simulation(elf):
    error = build_harness()
    if error is not None:
        return {"succeeded": False, "log": error}
    return simulate_elf(elf)


def command_simulate(arguments):
    elf = os.path.join(REPO_ROOT, "build", "release", "FAD.elf")
    if not os.path.isfile(elf):
        log("[simulate] build/release/FAD.elf 가 없다. python Scripts/verify.py build 를 먼저 실행한다.")
        return 1

    result = run_simulation(elf)
    report = {"succeeded": result["succeeded"], "seconds": result.get("seconds", 0), "metrics": result.get("metrics", {})}
    if not result["succeeded"]:
        log("[simulate] 실패")
        log(result["log"])
        write_report("simulation", report)
        return 1

    actual = transcript_lines(result["output"], firmware_version())
    if arguments.update:
        with open(SIMULATION_EXPECTED, "w", encoding="utf-8", newline="\n") as handle:
            handle.write("\n".join(actual) + "\n")
        log("[simulate] 기대 기록 {0}줄을 새로 썼다".format(len(actual)))

    expected = open(SIMULATION_EXPECTED, encoding="utf-8").read().splitlines()
    differences = [(index + 1, want, got) for index, (want, got) in enumerate(zip(expected, actual)) if want != got]
    if len(expected) != len(actual):
        differences.append((min(len(expected), len(actual)) + 1, "{0}줄".format(len(expected)), "{0}줄".format(len(actual))))

    report["lines"] = len(actual)
    report["differences"] = len(differences)
    write_report("simulation", report)

    log("[simulate] 끝단 시나리오 {0}줄, 기대 기록과 다른 줄 {1}개, {2}초".format(len(actual), len(differences), report["seconds"]))
    for line, want, got in differences[:10]:
        log("  {0}행 기대: {1}".format(line, want))
        log("  {0}행 실제: {1}".format(line, got))
    for name, value in sorted(report["metrics"].items()):
        log("  {0} = {1}".format(name, value))
    return 0 if not differences else 1


def prepare_copy(root):
    for name in ("Firmware", "Tests", "cmake"):
        shutil.copytree(os.path.join(REPO_ROOT, name), os.path.join(root, name))
    for name in ("CMakeLists.txt", "CMakePresets.json", "toolchain.lock.json"):
        shutil.copy2(os.path.join(REPO_ROOT, name), os.path.join(root, name))
    return max(len(os.path.join(current, name)) for current, _, files in os.walk(root) for name in files)


def build_and_test_copy(case):
    label, root = case
    longest = prepare_copy(root)
    build = build_configuration("Release", root)
    tests = run_host_tests(root)
    return {"case": label, "longest_path": longest, "build": build["succeeded"], "tests": tests["succeeded"],
            "log": "" if build["succeeded"] and tests["succeeded"] else build.get("log", "") + "\n".join(tests.get("failures", [])) + tests.get("log", "")}


def run_paths():
    base = os.path.join(tempfile.gettempdir(), "BioSight FAD path test")
    shutil.rmtree(base, ignore_errors=True)
    deep = os.path.join(base, "긴 경로 " + "가" * 60, "긴 경로 " + "나" * 60, "긴 경로 " + "다" * 60, "저장소")
    cases = [
        ("공백이 든 영문 경로", os.path.join(base, "space path", "repository copy")),
        ("공백과 한글이 든 경로", os.path.join(base, "한글 경로", "저장소 복사본")),
        ("260자를 넘는 한글 경로", deep),
    ]

    with concurrent.futures.ThreadPoolExecutor(max_workers=len(cases)) as pool:
        results = list(pool.map(build_and_test_copy, cases))

    for _, root in cases:
        link = effective_root(root)
        if link != root:
            os.rmdir(link)
    shutil.rmtree(base, ignore_errors=True)
    return results


def report_paths(results):
    status = 0
    for result in results:
        passed = result["build"] and result["tests"]
        status |= 0 if passed else 1
        log("[paths] {0} (가장 긴 파일 경로 {1}자): Release 빌드 {2}, 호스트 시험 {3}".format(
            result["case"], result["longest_path"], "성공" if result["build"] else "실패", "통과" if result["tests"] else "실패"))
        if not passed:
            log(result["log"])

    write_report("paths", {"cases": results})
    return status


def command_paths(_arguments):
    return report_paths(run_paths())


def command_all(arguments):
    arguments.update = False
    with concurrent.futures.ThreadPoolExecutor(max_workers=len(WINDOWS_SETUP) + len(CONFIGURATIONS) + 3) as pool:
        simulator = pool.submit(captured, setup_simulator)
        status = 0 if run_setup(WINDOWS_SETUP, pool) else 1
        status |= command_check(arguments)
        paths = pool.submit(captured, run_paths)
        builds = [pool.submit(build_configuration, configuration) for configuration in CONFIGURATIONS]
        tests = pool.submit(run_host_tests)
        for future in builds:
            report_build(future.result())
            status |= 0 if future.result()["succeeded"] else 1
        report_tests(tests.result())
        status |= 0 if tests.result()["succeeded"] else 1

        status |= 0 if replay(simulator.result()) else 1
        status |= command_simulate(arguments)
        status |= report_paths(replay(paths.result()))
    log("[all] {0}".format("모두 통과" if status == 0 else "실패한 단계가 있다"))
    return status


def export_revision(revision):
    environment = dict(os.environ, GIT_OPTIONAL_LOCKS="0")
    code, text, _ = run(["git", "-C", REPO_ROOT, "rev-parse", "--short", revision], 60, env=environment)
    commit = text.strip().splitlines()[-1] if text.strip() else ""
    if code != 0 or not re.fullmatch(r"[0-9a-f]+", commit):
        return None, "기준 판 {0} 을(를) 저장소에서 찾지 못했다. git log 로 판 이름을 확인한 뒤 다시 실행한다.".format(revision)

    root = os.path.join(REPO_ROOT, "build", "compare", commit)
    if not os.path.isdir(root):
        archive = subprocess.run(["git", "-C", REPO_ROOT, "archive", "--format=tar", commit], capture_output=True, env=environment, check=False)
        if archive.returncode != 0:
            return None, "기준 판 {0} 을(를) 꺼내지 못했다: {1}".format(commit, archive.stderr.decode("utf-8", "replace").strip())
        partial = root + ".partial"
        shutil.rmtree(partial, ignore_errors=True)
        os.makedirs(partial)
        with tarfile.open(fileobj=io.BytesIO(archive.stdout)) as package:
            package.extractall(partial, filter="data")
        os.replace(partial, root)
    return root, commit


def summarize_windows(windows):
    groups = collections.defaultdict(list)
    for _, command, loop, latency in windows:
        groups[command].append((loop, latency))
    return {command: {"count": len(values),
                      "loop_avg_us": round(sum(value[0] for value in values) / len(values) / CYCLES_PER_MICROSECOND, 2),
                      "loop_max_us": round(max(value[0] for value in values) / CYCLES_PER_MICROSECOND, 2),
                      "latency_avg_us": round(sum(value[1] for value in values) / len(values) / CYCLES_PER_MICROSECOND, 1)}
            for command, values in groups.items()}


def comparison_rows(builds, results):
    rows = [("프로그램 메모리 (바이트)", builds[0]["program_bytes"], builds[1]["program_bytes"]),
            ("데이터 메모리 (바이트)", builds[0]["data_bytes"], builds[1]["data_bytes"])]
    before, after = (summarize_windows(result["windows"]) for result in results)
    for command in sorted(set(before) & set(after)):
        for key, name in (("loop_avg_us", "바퀴 평균"), ("loop_max_us", "바퀴 최악"), ("latency_avg_us", "첫 응답 바이트까지 평균")):
            rows.append(("{0} {1} (µs, {2}건)".format(command, name, after[command]["count"]), before[command][key], after[command][key]))
    for key, name in (("loop-overall-avg-us", "루프 평균"), ("loop-idle-avg-us", "한가할 때 루프 평균"), ("loop-max-us", "루프 최악"),
                      ("latency-avg-us", "GVER 반복 측정의 첫 응답 바이트까지 평균")):
        rows.append((name + " (µs)", results[0]["metrics"].get(key), results[1]["metrics"].get(key)))
    return rows


def command_compare(arguments):
    root, commit = export_revision(arguments.base)
    if root is None:
        log("[compare] " + commit)
        return 1

    labels = (commit, "작업본")
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        builds = list(pool.map(lambda source: build_configuration("Release", source), [root, REPO_ROOT]))
    for label, build in zip(labels, builds):
        if not build["succeeded"]:
            log("[compare] {0} 빌드 실패\n{1}".format(label, build.get("log") or "\n".join(build.get("warnings", []))))
            return 1

    error = build_harness()
    if error is not None:
        log("[compare] " + error)
        return 1

    elves = [os.path.join(effective_root(source), "build", "release", "FAD.elf") for source in (root, REPO_ROOT)]
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        results = list(pool.map(lambda elf: simulate_elf(elf, ["FAD_WINDOWS=1"]), elves))
    for label, result in zip(labels, results):
        if not result["succeeded"]:
            log("[compare] {0} 시뮬레이션 실패\n{1}".format(label, result["log"]))
            return 1

    rows = comparison_rows(builds, results)
    worse = [name for name, before, after in rows if before is not None and after is not None and after > before]
    log("[compare] 기준 {0} → 작업본. 같은 시나리오와 같은 측정 도구로 16 MHz 사이클을 쟀다".format(commit))
    for name, before, after in rows:
        change = " ({0:+.1f} %)".format((after - before) * 100.0 / before) if before and after is not None else ""
        log("  {0}: {1} → {2}{3}".format(name, before, after, change))
    log("[compare] 늘어난 항목 {0}개{1}".format(len(worse), ": " + ", ".join(worse) if worse else ""))
    write_report("compare", {"base": commit, "rows": [{"name": name, "base": before, "current": after} for name, before, after in rows],
                             "worse": worse})
    return 0


def symbolize(elf, addresses):
    frames = {}
    for start in range(0, len(addresses), 300):
        chunk = ["0x{0:x}".format(address) for address in addresses[start:start + 300]]
        code, output, _ = run([avr_tool("avr-addr2line"), "-e", elf, "-f", "-i", "-C", "-a"] + chunk, 120)
        lines = output.splitlines()
        current = None
        index = 0
        while index < len(lines):
            if lines[index].startswith("0x"):
                current = int(lines[index], 16)
                frames[current] = []
                index += 1
                continue
            if current is not None and index + 1 < len(lines):
                frames[current].append((lines[index].split("(")[0], os.path.basename(lines[index + 1].split(" ")[0])))
            index += 2
    return frames


def command_profile(arguments):
    elf = os.path.join(REPO_ROOT, "build", "release", "FAD.elf")
    if not os.path.isfile(elf):
        log("[profile] build/release/FAD.elf 가 없다. python Scripts/verify.py build 를 먼저 실행한다.")
        return 1

    error = build_harness()
    if error is not None:
        log("[profile] " + error)
        return 1

    result = simulate_elf(elf, ["FAD_PROFILE={0}".format(arguments.min_cycles)])
    total = sum(result["samples"].values())
    if not result["succeeded"] or total == 0:
        log("[profile] 표본을 모으지 못했다. {0}".format(result["log"] or "--min-cycles 값을 줄여 다시 실행한다."))
        return 1

    frames = symbolize(elf, sorted(result["samples"]))
    functions = collections.Counter()
    lines = collections.Counter()
    for address, count in result["samples"].items():
        chain = frames.get(address) or [("?", "?")]
        functions[next((frame for frame in chain if not re.match(r"\.?L", frame[0])), chain[0])[0]] += count
        lines[chain[0][1]] += count

    log("[profile] {0} 사이클 이상 이어진 반복에서 모은 표본 {1}개".format(arguments.min_cycles, total))
    for title, counter in (("함수", functions), ("소스 줄", lines)):
        log("  -- {0} --".format(title))
        for name, count in counter.most_common(arguments.top):
            log("  {0:6.2f} % {1}".format(count * 100.0 / total, name))
    write_report("profile", {"min_cycles": arguments.min_cycles, "samples": total, "functions": functions.most_common(50),
                             "lines": lines.most_common(50)})
    return 0


def command_flash(arguments):
    hex_file = os.path.join(REPO_ROOT, "build", "release", "FAD.hex")
    if not os.path.isfile(hex_file):
        log("[flash] build/release/FAD.hex 가 없다. python Scripts/verify.py build --configuration Release 를 먼저 실행한다.")
        return 1

    avrdude = os.path.join(tool_directory("avrdude"), "avrdude.exe")
    command = [avrdude, "-p", "m2560", "-c", "wiring", "-P", arguments.port, "-b", "115200", "-D", "-U", "flash:w:" + hex_file + ":i"]
    log("[flash] " + " ".join(command))
    code, output, _ = run(command, 300)
    log(tail(output, 40))
    if code != 0:
        log("[flash] 기록하지 못했다. 보드가 {0} 에 연결되어 있는지, 다른 프로그램이 그 포트를 쓰고 있지 않은지 확인한 뒤 다시 실행한다.".format(arguments.port))
    return code


def main():
    parser = argparse.ArgumentParser(description="BioSight FAD 펌웨어 검증")
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("setup", help="툴체인·빌드 도구·시험 도구 설치")
    commands.add_parser("check", help="텍스트 파일 규칙·빌드 목록·버전 표기 대조")
    build = commands.add_parser("build", help="펌웨어 빌드")
    build.add_argument("--configuration", choices=list(CONFIGURATIONS) + ["all"], default="all")
    commands.add_parser("test", help="PC 에서 로직 단위 시험")
    simulate = commands.add_parser("simulate", help="simavr 로 Release 펌웨어 끝단 시험")
    simulate.add_argument("--update", action="store_true", help="현재 결과를 기대 기록으로 저장")
    commands.add_parser("paths", help="공백·한글·긴 경로에서 빌드와 시험")
    commands.add_parser("all", help="setup, check, build, test, simulate, paths 를 모두 실행")
    compare = commands.add_parser("compare", help="다른 판과 같은 시나리오로 크기·시간 비교")
    compare.add_argument("--base", default="HEAD", help="기준 판(커밋, 태그, 브랜치). 기본은 HEAD")
    profile = commands.add_parser("profile", help="simavr 명령 주소 표본으로 시간이 드는 곳 찾기")
    profile.add_argument("--min-cycles", type=int, default=0, help="이 사이클 수 이상 이어진 반복에서만 표본을 모은다")
    profile.add_argument("--top", type=int, default=15, help="보여 줄 항목 수")
    flash = commands.add_parser("flash", help="Release 펌웨어를 보드에 기록")
    flash.add_argument("--port", required=True, help="보드가 연결된 직렬 포트, 예: COM5")

    arguments = parser.parse_args()
    handlers = {"setup": command_setup, "check": command_check, "build": command_build, "test": command_test,
                "simulate": command_simulate, "paths": command_paths, "all": command_all, "compare": command_compare,
                "profile": command_profile, "flash": command_flash}
    return handlers[arguments.command](arguments)


if __name__ == "__main__":
    sys.exit(main())
