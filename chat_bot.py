import math
from random import random
from agents.react_agent import ReactAgent
from agents.tool import tool
import subprocess
import os
import streamlit as st
import io
import logging
from contextlib import redirect_stdout
from tools.compiler import Compiler
from tools.executor import Executor
from tools.security_check import check_secure
from tools.summary_rag import SummaryRAG
from argparse import ArgumentParser
from prompts import task_and_prompts, task_relu_prompts

from multiprocessing import get_context

# Top-level worker (must be top-level so it can be pickled by multiprocessing)
def _formalize_worker(args):
    user_prompt, model, seed = args
    # Import INSIDE the child process so Sage/cysignals installs signal handlers safely
    from tools.formal_prompt import formalize_user_prompt
    return formalize_user_prompt(user_prompt, model, seed)

@st.cache_resource
def _proc_pool():
    # "spawn" gives a clean child process with its own main thread
    return get_context("spawn").Pool(1)

def formalize_in_subprocess(user_prompt: str, model: str, seed: int = 0) -> str:
    pool = _proc_pool()
    return pool.apply(_formalize_worker, ((user_prompt, model, seed),))

st.title("TFHE-Coder interface")

task = st.radio(
    "Select a task:",
    ["AND", "ReLU", "Adder", "Multiplier", "Vector Addition", "Dot Product"]
)

task_map = {
    "AND": "task_and",
    "ReLU": "task_relu",
    "Adder": "task_adder",
    "Multiplier": "task_mult",
    "Vector Addition": "task_vector_addition",
    "Dot Product": "task_dot_product",
}

task_key = task_map[task]

user_query = st.text_input("Enter your query:")

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
model = "deepseek/deepseek-chat-v3.1:free"
# model = "qwen/qwen-2.5-72b-instruct:free"
# model = "google/gemini-2.5-pro"

task = task_key
# initialize save directory here
# save_dir = os.path.join("logs_formal_secure_rag_mod", model.split("/")[1].replace(":", "_").replace("-", "_"), task, str(args.run_id))
save_dir = f"chatting_output/{task}"

os.makedirs(save_dir, exist_ok=True)
# print(save_dir)
logger = get_logger(save_dir)

test_dir = f"unit_tests/{task}"

compiler = Compiler(save_dir=save_dir)
executor = Executor(save_dir=save_dir, test_dir=test_dir)
retriever = SummaryRAG(
    summary_db_path="./tfhe_documentation/summaries_db.json",
    embedding_model="text-embedding-3-small",
    persist_directory="./chroma_tfhe_summaries",
)

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
    
@tool
def rag(query: str) -> str:
    """
    This tool will retrieve the most relevant docstrings and function signatures for the query.

    Args:
        query (str): The query string to search for.
    Returns:
        str: Retrieved function signatures and summaries.
    """
    sig, summary, doxygen = retriever.retrieve(query, k=1)[0]
    return "\n".join([doxygen, sig])


agent = ReactAgent(tools=[compile_execute_secure_code, rag], model=model, seed=args.run_id, logger=logger)

def get_output_from_agent(query: str) -> str:
    output_buffer = io.StringIO()
    with redirect_stdout(output_buffer):
        print("Formalizing prompt .....")
        formal_query = formalize_in_subprocess(query, model)  # ✅ runs in a separate process
        print("Prompt formalized.....")

    captured_output = output_buffer.getvalue()
    response = agent.run(formal_query, max_rounds=7)
    try:
        with open(f"{save_dir}/program.c", "r") as f:
            code = f.read()
    except:
        code = "No code generated"
    return captured_output, code


# Process button
if st.button("Process Query"):
    if user_query:
        # Show processing message
        with st.spinner("Processing..."):
            captured_logs, final_result = get_output_from_agent(user_query)
        
        # Display captured print statements
        if captured_logs:
            st.subheader("Process Log:")
            st.text_area("Console Output:", value=captured_logs, height=100)
        
        # Display final result
        st.subheader("Final Output:")
        st.text_area("Result:", value=final_result, height=100)
    else:
        st.warning("Please enter a query!")

st.sidebar.write("Current selections:")
st.sidebar.write(f"Task: {task}")
st.sidebar.write(f"Query: {user_query}")
