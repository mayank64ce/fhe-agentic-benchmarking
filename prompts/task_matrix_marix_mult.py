path_to_vector_addition = ""
path_to_vector_dot_product = ""

with open(path_to_vector_addition, "r") as f:
    code_vector_addition = f.read()
with open(path_to_vector_dot_product, "r") as f:
    code_vector_dot_product = f.read()

task_prompt_matrix_matrix_mult = f"Write a TFHE code in C that homomorphically computes C = A·B for 32-bit signed matrices A(M×K) and B(K×N). Implement the triple loop with 32×32→64 multiplies and 64-bit accumulation per output cell, then decrypt and print the M×N result. Make sure that the code compiles and executes successfully. Use the following code for vector addition:\n{code_vector_addition}\n and Use the following code for dot product:\n {code_vector_dot_product}".strip()