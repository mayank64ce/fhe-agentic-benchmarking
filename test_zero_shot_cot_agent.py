import math
from random import random
from agents.react_agent import ReactAgent
from agents.tool import tool
import subprocess
import os
import logging
from tools.compiler import Compiler
from tools.executor import Executor
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
save_dir = os.path.join("logs_zscot", model.split("/")[1].replace(":", "_").replace("-", "_"), 'task_and', str(args.run_id))

os.makedirs(save_dir, exist_ok=True)
# print(save_dir)
logger = get_logger(save_dir)



test_dir = "unit_tests/task_and"

compiler = Compiler(save_dir=save_dir)
executor = Executor(save_dir=save_dir, test_dir=test_dir)

@tool
def compile_execute_code(code: str) -> str:
    """
    Compiles the given C file and executes it on the test cases.

    Args:
        code (str): The C code to compile and execute.
    
    Returns:
        str: Success message if both compilation and execution are successful, error message otherwise.
    """
    compile_result = compiler.compile(code)
    if "Compilation successful" in compile_result:
        execute_result = executor.execute()
        return execute_result
    else:
        return compile_result


agent = ReactAgent(tools=[compile_execute_code], model=model, seed=args.run_id, logger=logger)

user_prompt = task_and_prompts["informal"]

user_prompt += "Do not use extra logging in the program, just the computation. Make sure that the compilation and execution is successful. Do not give extra information."
user_prompt += "\nLet's think step by step."
output = agent.run(user_prompt, max_rounds=10)
# print(output)
logger.info(f"{output}")