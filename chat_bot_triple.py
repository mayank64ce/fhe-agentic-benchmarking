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
from baseline_agent import setup_baseline_agent
from tfhe_coder_agent import setup_tfhe_agent

from multiprocessing import get_context

chat_id_to_name = {
    "chat1": "TFHECoder",
    "chat2": "Chain of Thought Agent",
    "chat3": "Baseline Agent",
}

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

# st.title("TFHE-Coder interface")
st.set_page_config(page_title="Triple Chat • Final", layout="wide")
st.title("Secure Code Generation Interface")

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

# parsing

parser = ArgumentParser()
parser.add_argument("--run_id", type=int, default=0, help="Run ID for the experiment")

args = parser.parse_args()

# define agent
model = "deepseek/deepseek-chat-v3.1"
# model = "qwen/qwen-2.5-72b-instruct:free"
# model = "google/gemini-2.5-pro"

task = task_key
# initialize save directory here

tfhe_agent, tfhe_save_dir = setup_tfhe_agent(model, task)
baseline_agent, baseline_save_dir = setup_baseline_agent(model, task)
cot_agent, cot_save_dir = setup_baseline_agent(model, task, cot=True)


def get_output_from_agent1(query: str) -> str:
    output_buffer = io.StringIO()
    with redirect_stdout(output_buffer):
        print("Formalizing prompt .....")
        formal_query = formalize_in_subprocess(query, model)  # ✅ runs in a separate process
        print("Prompt formalized.....")

    captured_output = output_buffer.getvalue()
    response = tfhe_agent.run(formal_query, max_rounds=7)
    try:
        with open(f"{tfhe_save_dir}/program.c", "r") as f:
            code = f.read()
    except:
        code = "No code generated"
    return captured_output, code

def get_output_from_agent2(query: str) -> str:
    output_buffer = io.StringIO()

    query += "\nLet's think step by step."

    captured_output = output_buffer.getvalue()
    response = cot_agent.run(query, max_rounds=7)
    try:
        with open(f"{cot_save_dir}/program.c", "r") as f:
            code = f.read()
    except:
        code = "No code generated"
    return captured_output, code

def get_output_from_agent3(query: str) -> str:
    output_buffer = io.StringIO()

    captured_output = output_buffer.getvalue()
    response = baseline_agent.run(query, max_rounds=7)
    try:
        with open(f"{baseline_save_dir}/program.c", "r") as f:
            code = f.read()
    except:
        code = "No code generated"
    return captured_output, code



# ---- state helpers ----
def ensure_state(chat_id: str):
    st.session_state.setdefault(f"q_{chat_id}", "")
    st.session_state.setdefault(f"logs_{chat_id}", "")
    st.session_state.setdefault(f"code_{chat_id}", "")


def render_chat_panel(chat_id: str, run_fn):
    # ensure per-chat state
    st.session_state.setdefault(f"q_{chat_id}", "")
    st.session_state.setdefault(f"logs_display_{chat_id}", "")
    st.session_state.setdefault(f"code_display_{chat_id}", "")

    # st.subheader(f"Chat {chat_id.upper()}")
    st.subheader(f"{chat_id_to_name[chat_id]}")

    # INPUT (stateful)
    st.text_input("Enter your query:", key=f"q_{chat_id}",
                  placeholder="Type and click Process Query")

    # BUTTON → update state
    if st.button("Process Query", key=f"btn_{chat_id}", use_container_width=True):
        q = st.session_state[f"q_{chat_id}"].strip()
        if not q:
            st.session_state[f"logs_display_{chat_id}"] = f"[{chat_id}] Please enter a query."
            st.session_state[f"code_display_{chat_id}"] = ""
        else:
            logs, code = run_fn(q)
            # write results into the DISPLAY state keys
            st.session_state[f"logs_display_{chat_id}"] = logs
            st.session_state[f"code_display_{chat_id}"] = code

    # DISPLAY (unique keys; no value=)
    st.text_area("Process Log:",
                 key=f"logs_display_{chat_id}",
                 height=140,
                 disabled=True)
    st.text_area("Final Output:",
                 key=f"code_display_{chat_id}",
                 height=180,
                 disabled=True)
# ---- layout ----
c1, c2, c3 = st.columns(3)
with c1: render_chat_panel("chat1", get_output_from_agent1)
with c2: render_chat_panel("chat2", get_output_from_agent2)
with c3: render_chat_panel("chat3", get_output_from_agent3)
