from pathlib import Path, PurePosixPath
from enum import Enum
import subprocess
import os
import uuid

class COMPILE_FOR(Enum):
    WINDOWS_64 = 0
    LINUX_64 = 1
    LNX_WIN_64 = 2

    ALL = 2147483648
    INVALID = -2147483648

    @classmethod
    def QueryInput(cls, Input):
        Input = Input.upper()
        if cls in cls.__members__:
            return cls[Input]

        if Input.isdigit():
            Input = int(Input)
            if Input in [i.value for i in cls]:
                return cls(Input)

        return cls['INVALID']

class BuildOptimizationLevels(Enum):
    Debug = 0
    ReleaseFast = 1
    ReleaseSafe = 2
    ReleaseSmall = 3

    @classmethod
    def QueryInput(cls, Input):
        Input = Input.upper()
        if cls in cls.__members__:
            return cls[Input]

        if Input.isdigit():
            Input = int(Input)
            if Input in [i.value for i in cls]:
                return cls(Input)

        return cls['INVALID']

class WSLSession:
    def __init__(self, Distro):
        self.distro = Distro
        self.process = None
        self.echo_code = f"__WSL_COMPUTE_FINISHED_{uuid.uuid4().hex}__"
        self._boot()

    def _boot(self):
        self.process = subprocess.Popen(
            ['wsl', '-d', self.distro, '--', 'bash', '-noprofile', '--norc'],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1
        )

        self.Execute("echo 'bootcheck'")

    def Execute(self, Command: str) -> str:
        if not self.process or self.process.poll() is not None:
            raise RuntimeError(f"WSLSession execute failed. Perhaps the boot failed?")

        Command = Command.strip()
        Command = f"{Command} && echo '{self.echo_code}' || echo '{self.echo_code}'\n"

        self.process.stdin.write(Command)
        self.process.stdin.flush()

        Output = []
        while True:
            line = self.process.stdout.readline()
            if not line:
                break
            if self.echo_code in line:
                break
            Output.append(line.strip())

        return "".join(Output)

    def Shutdown(self):
        if self.process:
            try:
                self.process.stdin.write("exit\n")
                self.process.stdin.flush()
                self.process.communicate(timeout=2)
            except Exception:
                self.process.kill()

def ReadFile(RelativeFilePath):
    file_path = Path(RelativeFilePath).resolve()

    with open(file_path, 'r') as file:
        Contents = file.read()
        return Contents

def WinToWSLPath(WindowsPath):
    drive = WindowsPath.drive.replace(":", '').lower()
    WindowsPath = PurePosixPath('/mnt', drive, *WindowsPath.parts[1:])
    return WindowsPath

def Compile_SCons(Case, File, Target):
    TemplateCommand = f'scons --target=\"{Target}\" --targetFolder=\"{File}\" --productionBuild={int(ProductionBuild)}'
    TemplateCommandLinux = f'scons --target="{Target}" --targetFolder="{File}" --productionBuild={int(ProductionBuild)} --compileFor="Linux"'
    SourceFolder = Path("../../").resolve()

    def win_sub_process_comp():
        print(f"Compiling with command : '{TemplateCommand}' \n In source folder : '{SourceFolder}'")
        Result = subprocess.run(TemplateCommand, cwd=SourceFolder, shell=True, capture_output=True, text=True)
        print(Result.stdout) if Result.returncode == 0 else print(Result.stderr)

    def linux_sub_process_comp(LinuxPath):
        print(f"Compiling for Linux with command : '{TemplateCommandLinux}'\n In source folder : '{LinuxPath}'")
        print(Session.Execute(f"cd {LinuxPath}"))
        print(Session.Execute(TemplateCommandLinux))

    match Case:
        case COMPILE_FOR.WINDOWS_64.value:
            win_sub_process_comp()
        case COMPILE_FOR.LINUX_64.value:
            linux_sub_process_comp(WinToWSLPath(SourceFolder))

def Compile_Zig(Case, File, Target):
    BaseCommand = f'zig build -DLibraryName=\"{Target}\" -DCompileFromDirectory=\"{File}\"'
    TemplateCommandWindows = BaseCommand + '-Dtarget=x86_64-windows'
    TemplateCommandLinux = BaseCommand + '-Dtarget=aarch64-linux'

src = '../../src'

MetaData = ReadFile(src + '/meta')

print(MetaData)

BuildOptimization = -1
BuildSystem = input("Build system to use (0 for Zig, 1 for SCons): ")

if BuildSystem == 0:
    BuildOptimization = input("Build optimization for Zig build system (type !help for help): ")

Platform = 0
ProductionBuild = 0

if BuildSystem != "!help":
    Platform = input("Target platform to compile for (type !help for help): ")
    ProductionBuild = input("Production (non-debug) build? (y/n): ")
    ProductionBuild = True if ProductionBuild.lower() == "y" else False
else:
    Platform = BuildOptimization


def main():
    match Platform:
        case "!help":
            print("Platform enums:")
            for i in COMPILE_FOR:
                print(f"{i.name}: {i.value}")
            print("Zig build system enums:")
            for i in BuildOptimizationLevels:
                print(f"{i.name}: {i.value}")
        case _:
            CompileForPlatform = COMPILE_FOR.QueryInput(Platform).value
            if CompileForPlatform == COMPILE_FOR.INVALID.value:
                print("Selected target platform is not valid.")
                return

            print(CompileForPlatform)

            for line in MetaData.splitlines():
                if line.isspace() or ":" not in line: continue
                file, target = line.split(":", 1)
                if BuildSystem == 1:
                    Compile_SCons(CompileForPlatform, file, target)
                else:
                    Compile_Zig(CompileForPlatform, file, target)

Session: WSLSession
if BuildSystem == 1:
    Session = WSLSession(Distro='Ubuntu')
    main()
    Session.Shutdown()
else:
    main()