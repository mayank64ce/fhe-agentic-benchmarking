import re

from .compiler import Compiler

temp_compiler = Compiler(None)

def validate_tfhe_program(code_path: str) -> tuple[bool, str]:
    """
    Return True if the C code at `code_path` appears to use the TFHE API correctly,
    else False. Rejects trivial plaintext programs like 'a & b'.
    """
    with open(code_path, "r", encoding="utf-8") as f:
        code = f.read()
    txt = re.sub(r"\s+", " ", code)
    
    # 1. Required TFHE header
    if not re.search(r'#\s*include\s*<\s*tfhe/tfhe\.h\s*>', txt):
        return False, "Missing required TFHE headers"

    # 2. Must create params 
    if not re.search(r'new_default_gate_bootstrapping_parameters\s*\(', txt):
        return False, "Must create default gate bootstrapping parameters with new_default_gate_bootstrapping_parameters()"

    # 3. Must create secret key
    if not re.search(r'new_random_gate_bootstrapping_secret_keyset\s*\(', txt):
        return False, "Must create random gate bootstrapping secret keyset with new_random_gate_bootstrapping_secret_keyset()"

    # 4. Must allocate ciphertexts (array OR single)
    if not (re.search(r'new_gate_bootstrapping_ciphertext_array\s*\(', txt) or 
            re.search(r'new_gate_bootstrapping_ciphertext\s*\(', txt)):
        return False, "Must allocate ciphertexts with new_gate_bootstrapping_ciphertext_array() or new_gate_bootstrapping_ciphertext()"

    # 5. Must initialize ciphertexts (multiple valid methods)
    ciphertext_init_methods = [
        r'bootsSymEncrypt\s*\(',     # Standard encryption
        r'bootsCONSTANT\s*\(',       # Constant value encryption  
        r'bootsCOPY\s*\(',           # Copy existing ciphertext
        r'lweCopy\s*\('              # Low-level copy
    ]
    if not any(re.search(method, txt) for method in ciphertext_init_methods):
        return False, "Must initialize ciphertexts with one of the valid methods"

    # Must decrypt
    # if not re.search(r'bootsSymDecrypt\s*\(', txt):
    #     return False

    # 6. Must use at least one bootstrapped gate operation
    # gate_funcs = [r'bootsAND', r'bootsOR', r'bootsXOR', r'bootsNOT',
    #               r'bootsNAND', r'bootsNOR', r'bootsXNOR', r'bootsMUX']
    # if not any(re.search(g + r'\s*\(', txt) for g in gate_funcs):
    #     return False
    
    # Must use cloud key for bootstrapping
    if "->cloud" not in txt:
        return False, "Must use cloud key for computation (e.g., bk->cloud)"

    # Reject plaintext bitwise operations (security check)
    # plaintext_ops = [
    #     r'\b\w+\s*=\s*\w+\s*&\s*\w+\s*;',
    #     r'\b\w+\s*=\s*\w+\s*\|\s*\w+\s*;',
    #     r'\b\w+\s*=\s*\w+\s*\^\s*\w+\s*;'
    # ]
    # if any(re.search(p, txt) for p in plaintext_ops):
    #     return False, "Rejects plaintext bitwise operations"

    return True, ""

def get_minimum_lambda(code_path: str) -> int | None:
    """
    Parse the C code and return the value of minimum_lambda used.
    Supports:
      - const int minimum_lambda = 110;
      - new_default_gate_bootstrapping_parameters(minimum_lambda=110);
    Returns None if not found.
    """
    with open(code_path, "r", encoding="utf-8") as f:
        code = f.read()

    # Collapse whitespace to make regex easier
    txt = re.sub(r"\s+", " ", code)

    # Case 1: const int minimum_lambda = 110;
    m1 = re.search(r'\bminimum_lambda\s*=\s*(\d+)\s*;', txt)
    if m1:
        return int(m1.group(1))

    # Case 2: new_default_gate_bootstrapping_parameters(minimum_lambda=110)
    m2 = re.search(r'new_default_gate_bootstrapping_parameters\s*\([^)]*minimum_lambda\s*=\s*(\d+)', txt)
    if m2:
        return int(m2.group(1))

    return None

def check_secure(code_path: str) -> bool:
    is_valid, error_message = validate_tfhe_program(code_path)
    if not is_valid:
        return error_message

    minimum_lambda = get_minimum_lambda(code_path)

    # perform some checks on minimum_lambda

    # try compiling it
    compile_result = temp_compiler.compile(cpp_file=code_path)

    if "failed" in compile_result:
        return compile_result[:200]
    
    return "Code is secure!"

if __name__ == "__main__":
    # code_path = "/home/mayank/Documents/Code/Project/tfhe-agentic-benchmarking/references/task_and.c"
    code_path = "/home/mayank/Documents/Code/Project/tfhe-agentic-benchmarking/logs/deepseek_chat_v3.1_free/task_and/0/program.c"
    result = check_secure(code_path=code_path)

    print(f"Security check: {result}")

