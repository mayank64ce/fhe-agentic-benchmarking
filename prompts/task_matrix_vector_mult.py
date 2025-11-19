path_to_vector_addition = ""
path_to_vector_dot_product = ""

with open(path_to_vector_addition, "r") as f:
    code_vector_addition = f.read()
with open(path_to_vector_dot_product, "r") as f:
    code_vector_dot_product = f.read()

task_prompt_matrix_vector_mult = f"Write a TFHE code in C that homomorphically computes Y = A·x where A is an M×K matrix and x is a K-vector of 32-bit integers. Use 32×32→64 multiplies and 64-bit accumulators for each output entry, then decrypt and print the M-vector. Make sure that the code compiles and executes successfully. Use the following code for vector addition:\n{code_vector_addition}\n and Use the following code for dot product:\n {code_vector_dot_product}".strip()

task_prompt_matrix_vector_mult_formal = 