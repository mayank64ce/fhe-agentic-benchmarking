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
from prompts import task_and_prompts, task_relu_prompts

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
task = "task_relu"

# define agent
model = "deepseek/deepseek-chat-v3.1:free"
# model = "qwen/qwen3-coder"
# model = "qwen/qwen-2.5-72b-instruct:free"
# model = "qwen/qwen3-235b-a22b:free"
# model = "openai/gpt-4o-2024-11-20"
# model = "meta-llama/llama-4-maverick:free"
# model = "openai/gpt-3.5-turbo"
# model = "google/gemini-2.5-pro"

# initialize save directory here
save_dir = os.path.join("logs_formal_mod", model.split("/")[1].replace(":", "_").replace("-", "_"), task, str(args.run_id))

os.makedirs(save_dir, exist_ok=True)

# print(save_dir)
logger = get_logger(save_dir)


test_dir = f"unit_tests/{task}"

compiler = Compiler(save_dir=save_dir)
executor = Executor(save_dir=save_dir, test_dir=test_dir)

@tool
def compile_execute_code(code: str) -> str:
    """
    Compiles the given C file and executes it on the test cases.

    Args:
        code (str): The C++ code to compile and execute.
    
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

user_prompt = task_relu_prompts[f"{0}"]

user_prompt += "Do not use extra logging in the program, just the computation. Stop the compilation and execution is successful, all tests have passed. Do not give extra information."
output = agent.run(user_prompt, max_rounds=10)
# print(output)
logger.info(f"{output}")