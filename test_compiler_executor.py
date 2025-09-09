import os

from tools.compiler import Compiler
from tools.executor import Executor

task = "task_and"
reference_path = f"references/{task}.c"

with open(reference_path, "r") as f:
    reference_code = f.read()

compiler = Compiler(save_dir=None)

compiler_status = compiler.compile(code=reference_code, cpp_file=reference_path)
print("Compiler status:", compiler_status)

executor = Executor(save_dir="references", test_dir=f"unit_tests/{task}", task=task)

executor_status = executor.execute()
print("Executor status:", executor_status)