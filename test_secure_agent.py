import math
from random import random
from agents.react_agent import ReactAgent
from agents.tool import tool
import subprocess
import os
import logging
from tools.compiler import Compiler
from tools.executor import Executor
from tools.security_check import check_secure
from argparse import ArgumentParser
from prompts import task_and_prompts

def get_logger(save_dir: str):
    log_file = os.path.join(save_dir, "run.log")

    logger = logging.getLogger("experiment")
    logger.setLevel(logging.INFO)

    # Clear existing handlers if you re-run in the same process
    if logger.hasHandlers():
        logger.handlers.clear()

    # File handler
    fh = logging.FileHandler(log_file)
    fh.setLevel(logging.INFO)

    # Console handler
    ch = logging.StreamHandler()
    ch.setLevel(logging.INFO)

    # Format
    formatter = logging.Formatter("%(asctime)s - %(levelname)s - %(message)s")
    fh.setFormatter(formatter)
    ch.setFormatter(formatter)

    logger.addHandler(fh)
    logger.addHandler(ch)

    return logger

# parsing

parser = ArgumentParser()
parser.add_argument("--run_id", type=int, default=0, help="Run ID for the experiment")

args = parser.parse_args()

# define agent
# model = "deepseek/deepseek-chat-v3.1:free"
model = "qwen/qwen-2.5-72b-instruct:free"

# initialize save directory here
save_dir = os.path.join("logs_secure", model.split("/")[1].replace(":", "_").replace("-", "_"), 'task_and', str(args.run_id))

os.makedirs(save_dir, exist_ok=True)
# print(save_dir)
logger = get_logger(save_dir)



test_dir = "unit_tests/task_and"

compiler = Compiler(save_dir=save_dir)
executor = Executor(save_dir=save_dir, test_dir=test_dir)

@tool
def compile_execute_secure_code(code: str) -> str:
    """
    Compiles the given C file and executes it on the test cases. After that, it runs a security check on the code.

    Args:
        code (str): The C code to compile and execute.
    
    Returns:
        str: Success message if both compilation and execution are successful, error message otherwise.
    """
    compile_result = compiler.compile(code)
    if "Compilation successful" in compile_result:
        execute_result = executor.execute()
        if "successfully" in execute_result:
            # run security check
            security_result = check_secure(f"{save_dir}/program.c")
            return security_result
        else:
            return execute_result
    else:
        return compile_result


agent = ReactAgent(tools=[compile_execute_secure_code], model=model, seed=args.run_id, logger=logger)

user_prompt = task_and_prompts["informal"]

user_prompt += "Do not use extra logging in the program, just the computation. Make sure that the compilation, execution and security check is successful. Do not give extra information."
user_prompt += "Do not create Simple-C programs, create C programs. Use the functions from the TFHE library."
# user_prompt += """
# Here is the header to import:
# #include <tfhe/tfhe.h>
# #include <tfhe/tfhe_io.h>
# """
output = agent.run(user_prompt, max_rounds=10)
# print(output)
logger.info(f"{output}")