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
from tools.summary_rag import SummaryRAG
from argparse import ArgumentParser
from prompts import task_and_prompts, task_relu_prompts, task_transformer_prompts

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
# model = "qwen/qwen-2.5-72b-instruct:free"
# model = "google/gemini-2.5-pro"
model = "openai/gpt-5"

task = "task_transformer"
# initialize save directory here
save_dir = os.path.join("logs_formal_secure_rag_mod", model.split("/")[1].replace(":", "_").replace("-", "_"), task, str(args.run_id))

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
    Executes the given python file. After that, it runs a security check on the code.

    Args:
        code (str): The python code to execute.
    
    Returns:
        str: Success message if both compilation and execution are successful, error message otherwise.
    """
    compile_result = compiler.compile(code)
    if "Compilation successful" in compile_result:
        execute_result = executor.execute()
        # if "successfully" in execute_result:
        #     # run security check
        #     security_result = check_secure(f"{save_dir}/program.c")
        #     return security_result
        # else:
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


agent = ReactAgent(tools=[compile_execute_secure_code], model=model, seed=args.run_id, logger=logger)

reference_softmax = """import sys
import numpy as np
from openfhe import *


# ---------- Basic CKKS setup ----------

def init_ckks(n=8, mult_depth=10):
    params = CCParamsCKKSRNS()
    params.SetMultiplicativeDepth(mult_depth)
    params.SetScalingModSize(59)
    params.SetFirstModSize(60)
    params.SetScalingTechnique(FIXEDAUTO)
    params.SetSecretKeyDist(UNIFORM_TERNARY)
    params.SetBatchSize(n)

    cc = GenCryptoContext(params)
    cc.Enable(PKESchemeFeature.PKE)
    cc.Enable(PKESchemeFeature.LEVELEDSHE)
    cc.Enable(PKESchemeFeature.ADVANCEDSHE)

    keys = cc.KeyGen()
    cc.EvalMultKeyGen(keys.secretKey)
    cc.EvalSumKeyGen(keys.secretKey)

    # rotation keys for sum-of-slots (for n=8 we need 1,2,4)
    cc.EvalRotateKeyGen(keys.secretKey, [1, 2, 4])

    return cc, keys


# ---------- Encrypt / decrypt helpers ----------

def encrypt_vector(cc, public_key, x, n):

    x = np.array(x, dtype=float)
    if len(x) < n:
        padded = np.zeros(n)
        padded[:len(x)] = x
        x = padded
    else:
        x = x[:n]

    pt = cc.MakeCKKSPackedPlaintext(x.tolist())
    ct = cc.Encrypt(public_key, pt)
    return ct


def decrypt_vector(cc, secret_key, ct, n):

    pt = cc.Decrypt(secret_key, ct)
    pt.SetLength(n)
    vals = pt.GetRealPackedValue()
    return np.array(vals[:n])


# ---------- Homomorphic building blocks ----------

def homomorphic_exp_poly(cc, ct, n):
    one_pt = cc.MakeCKKSPackedPlaintext([1.0] * n)
    half_pt = cc.MakeCKKSPackedPlaintext([0.5] * n)

    # x
    x = ct

    # x^2
    x2 = cc.EvalMult(x, x)

    # x^2 / 2
    x2_half = cc.EvalMult(x2, half_pt)

    # 1 + x + x^2/2
    tmp = cc.EvalAdd(one_pt, x)
    exp_ct = cc.EvalAdd(tmp, x2_half)
    return exp_ct


def homomorphic_sum_slots(cc, ct, n):

    s = ct
    # for n=8: log2(8) = 3 steps: rotate by 1, 2, 4
    for shift in [1, 2, 4]:
        rotated = cc.EvalRotate(s, shift)
        s = cc.EvalAdd(s, rotated)
    return s


# ---------- Softmax pipeline ----------

def ckks_softmax(cc, keys, x, n=8):

    x = np.array(x, dtype=float)
    L = len(x)
    if L > n:
        x = x[:n]
        L = n

    # 1) shift by max for stability
    x_shifted = x - np.max(x)

    # 2) encrypt
    z_ct = encrypt_vector(cc, keys.publicKey, x_shifted, n)

    # 3) exp approximation
    exp_ct = homomorphic_exp_poly(cc, z_ct, n)

    # 4) homomorphic sum of exponentials
    sum_ct = homomorphic_sum_slots(cc, exp_ct, n)

    # 5) decrypt sum and compute 1/s in plaintext
    sum_vals = decrypt_vector(cc, keys.secretKey, sum_ct, n)
    s = sum_vals[0]  # all slots should be identical

    inv_s_pt = cc.MakeCKKSPackedPlaintext([1.0 / s] * n)

    # 6) multiply exponentials by 1/s
    softmax_ct = cc.EvalMult(exp_ct, inv_s_pt)

    # 7) decrypt final result
    softmax_vals = decrypt_vector(cc, keys.secretKey, softmax_ct, n)

    # return only first L entries (the rest are padding)
    return softmax_vals[:L]


# ---------- Plain NumPy softmax for comparison ----------

def numpy_softmax(x):
    x = np.array(x, dtype=float)
    e = np.exp(x - np.max(x))
    return e / np.sum(e)


# ---------- Input / main ----------

def read_vector_from_stdin(n=8):
    tokens = sys.stdin.read().strip().split()
    if len(tokens) == 0:
        raise ValueError(f"Expected at least 1 number, got 0.")

    vals = list(map(float, tokens[:n]))
    return np.array(vals, dtype=float)


def main():
    n = 8  # number of slots / vector length we use

    x = read_vector_from_stdin(n)

    # Initialize CKKS
    cc, keys = init_ckks(n=n, mult_depth=10)

    # CKKS softmax
    softmax_he = ckks_softmax(cc, keys, x, n=n)


    np.set_printoptions(suppress=True, precision=6)

    print(softmax_he)



if __name__ == "__main__":
    main()
""".strip()

reference_matmul = """import sys
import numpy as np
from openfhe import *
import openfhe_numpy as onp


def read_matrices_from_stdin():
    tokens = sys.stdin.read().strip().split()
    if len(tokens) < 4:
        raise ValueError("Not enough input. Need at least 4 integers: n1 m1 n2 m2.")

    it = iter(tokens)
    try:
        n1 = int(next(it))
        m1 = int(next(it))
        n2 = int(next(it))
        m2 = int(next(it))
    except StopIteration:
        raise ValueError("Failed to read matrix dimensions from stdin.")

    # Remaining tokens are matrix entries
    entries = list(it)
    needed = n1 * m1 + n2 * m2
    if len(entries) < needed:
        raise ValueError(
            f"Not enough matrix entries. Need {needed}, got {len(entries)}."
        )

    entries = list(map(float, entries[:needed]))

    A_flat = entries[: n1 * m1]
    B_flat = entries[n1 * m1 :]

    A = np.array(A_flat, dtype=float).reshape(n1, m1)
    B = np.array(B_flat, dtype=float).reshape(n2, m2)

    if m1 != n2:
        raise ValueError(
            f"Incompatible dimensions for matmul: A is {n1}x{m1}, B is {n2}x{m2}."
        )

    return A, B


def homomorphic_matmul(A: np.ndarray, B: np.ndarray) -> np.ndarray:

    # 1. Crypto context and keys (CKKS)
    params = CCParamsCKKSRNS()
    params.SetMultiplicativeDepth(4)
    params.SetScalingModSize(59)
    params.SetFirstModSize(60)
    params.SetScalingTechnique(FIXEDAUTO)

    cc = GenCryptoContext(params)
    cc.Enable(PKESchemeFeature.PKE)
    cc.Enable(PKESchemeFeature.LEVELEDSHE)
    cc.Enable(PKESchemeFeature.ADVANCEDSHE)

    keys = cc.KeyGen()
    cc.EvalMultKeyGen(keys.secretKey)
    cc.EvalSumKeyGen(keys.secretKey)

    # 2. Encrypt matrices as openfhe_numpy arrays
    batch_size = cc.GetRingDimension() // 2

    ctA = onp.array(
        cc=cc,
        data=A,
        batch_size=batch_size,
        order=onp.ROW_MAJOR,
        fhe_type="C",        # CKKS
        mode="tile",
        public_key=keys.publicKey,
    )

    ctB = onp.array(
        cc=cc,
        data=B,
        batch_size=batch_size,
        order=onp.ROW_MAJOR,
        fhe_type="C",
        mode="tile",
        public_key=keys.publicKey,
    )

    # 3. Rotation keys for square matmul
    onp.EvalSquareMatMultRotateKeyGen(keys.secretKey, ctA.ncols)

    # 4. Homomorphic matmul
    ctC = ctA @ ctB

    # 5. Decrypt result
    C_dec = ctC.decrypt(keys.secretKey, unpack_type="original")
    return C_dec


def main():
    # Read matrices A and B from stdin
    A, B = read_matrices_from_stdin()

    # Plaintext result for reference
    plain = A @ B

    # Homomorphic result
    he_res = homomorphic_matmul(A, B)

    # Print results
    np.set_printoptions(suppress=True, precision=6)

    print(he_res)


if __name__ == "__main__":
    main()
""".strip()

user_prompt = task_transformer_prompts[f"0"]

user_prompt += "Do not use extra logging in the program, just the computation. Make sure that the compilation and execution is successful. Do not give extra information."

user_prompt += "\nHere is a reference implementation of softmax in OpenFHE CKKS:\n" + reference_softmax + "\n"
user_prompt += "\nHere is a reference implementation of matrix multiplication in OpenFHE CKKS:\n" + reference_matmul
print("Starting agent run...")
output = agent.run(user_prompt, max_rounds=10)
# print(output)
logger.info(f"{output}")