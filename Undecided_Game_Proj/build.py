import argparse
import os
import subprocess

Parser = argparse.ArgumentParser(description="A build script that allows for cross compilation using SCons, Zig, and MSVC (Windows).")

Parser.add_argument("--target", type=str, help="The platform to build for. e.g., --target='windows'")
Parser.add_argument("--LibraryName", type=str, default="ALL", help="The library to build for. e.g., --LibraryName='PCG_Environment'\nLeave empty to compile everything.")
Parser.add_argument("--CompileFromDirectory", type=str, default="ALL", help="The directory where the library is located in. e.g., --CompileFromDirectory='Procedural Environment Generator'\nOr leave empty to compile everything")
Parser.add_argument("--ProductionBuild", type=str, default=True, help="Whether to build for a debug -O2 build or an optimized -O3 build.")

Arguments = Parser.parse_args()

def _compile_cpp_zig(Target: str, LibraryName: str, CompileFromDirectory: str, Debug: bool):
    Target = "x86_64-" + Target.lower().strip()
    try:
        os.chdir("src")
    except os.error as err:
        print("Failed to change directory to src with err: ", err)
        return

    try:
        result = subprocess.run(["zig", "build", f"-Dtarget='{Target}'", f"-DLibraryName='{LibraryName}'",
                                 f"-DCompileFromDirectory='{CompileFromDirectory}'", f"-DDebugBuild={Debug}"],
                                check=True, capture_output=True, text=True)
    except subprocess.CalledProcessError as err:
        print("Failed to build with Zig with error:", err)

def _compile_cpp_win_scons(LibraryName: str, CompileFromDirectory: str, Debug: bool):
    DebugBuild = 0 if Debug else 1
    try:
        result = subprocess.run(["scons", f"--target={LibraryName}", f"--targetFolder={CompileFromDirectory}", f"--productionBuild={DebugBuild}"], check=True, capture_output=True, text=True)
    except subprocess.CalledProcessError as err:
        print("Failed to compile with SCons with error: ", err)

if(Arguments.target != 'windows'):
    _compile_cpp_zig(Arguments.target, Arguments.LibraryName, Arguments.CompileFromDirectory, Arguments.ProductionBuild)
else:
    _compile_cpp_win_scons(Arguments.LibraryName, Arguments.CompileFromDirectory, Arguments.ProductionBuild)