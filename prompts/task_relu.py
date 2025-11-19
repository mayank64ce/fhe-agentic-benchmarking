task_prompt_relu = "Write a TFHE code in C that performs a bitwise ReLU operation on one input integer. Make sure that the code compiles and executes successfully."

task_prompt_relu_formal = """You are an expert coding agent familiar with cryptographic concepts.

Make sure to follow the below instructions:

    - The input will always come from the stdin.

    - Code should not have any extra logging or print statements.

    - Code should not be a basic C program, it should use TFHE library functions.

    - The TFHE header files to include are:
                #include <tfhe/tfhe.h>
                #include <tfhe/tfhe_io.h>

    - Code should have `assert` statements to implement the `ensure` and `requires` statements in the Dafny code.

    - Follow the following Dafny-like code as a pseudo-code.
            // Global security parameter specification
const minimum_lambda: int := 128;

method Main(x: bv32)
  returns (result: bv32)
  // Postcondition: Result is always non-negative
  ensures result >= 0bv32
  // Postcondition: For non-negative inputs, result equals input
  ensures x >= 0bv32 ==> result == x
  // Postcondition: For negative inputs, result equals zero
  ensures x < 0bv32 ==> result == 0bv32
{
  // Bitwise ReLU implementation using sign bit manipulation
  // The sign bit (most significant bit) determines if x is negative
  var sign_bit_mask: bv32 := 0x80000000; // Mask for the sign bit
  
  // Create a mask that is all 1s if x is negative, all 0s otherwise
  var is_negative_mask: bv32 := (x & sign_bit_mask) >> 31;
  
  // Bitwise AND with the inverse of the negative mask to zero out negative values
  // If x is negative, is_negative_mask is all 1s, so ~is_negative_mask is all 0s
  // If x is non-negative, is_negative_mask is all 0s, so ~is_negative_mask is all 1s
  result := x & ~is_negative_mask;
  
  // Assertions to prove functional correctness
  assert result >= 0bv32;
  assert x >= 0bv32 ==> result == x;
  assert x < 0bv32 ==> result == 0bv32;
}

    - Follow the following informal code requirements strictly.
            ### Bitwise ReLU Operation Implementation

This program operates within a defined security context and proves the correctness of a bitwise ReLU (Rectified Linear Unit) operation.

**Security Context:**
* A global constant, `minimum_lambda`, is set to 128 to define the program's required security level for cryptographic operations.

**Functional Correctness:**
* The `Main` method takes a single 32-bit integer `x` as user input.
* It implements a bitwise ReLU operation that returns 0 for negative values and the original value for non-negative values.
* The method proves functional correctness by asserting that:
    1. The result is always non-negative (≥ 0)
    2. For non-negative inputs, the result equals the input
    3. For negative inputs, the result equals 0
* The implementation uses bitwise operations to achieve this without conditional branching.

    - Code should follow the following structure:
            1. Setup Parameters
            2. Key Generation
            3. Encryption of inputs
            4. Homomorphic Operations
            5. Decryption of the result
            6. Print the result


TASK: Write TFHE code in C that performs a bitwise ReLU operation on one input integer
""".strip()

if __name__ == "__main__":
    print(task_prompt_relu_formal)