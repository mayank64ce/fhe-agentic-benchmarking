task_prompt = "Write a TFHE code in C that performs a bitwise AND operation on two input integers. Make sure that the code compiles and executes successfully."

task_prompt_formal_0 = """ You are an expert coding agent familiar with cryptographic concepts.

Make sure to follow the below instructions:

    - The input will always come from the user.

    - Code should not have any extra logging or print statements.

    - Code should not be a basic C program, it should use TFHE library functions.

    - The TFHE header files to include are:
        #include <tfhe/tfhe.h>
        #include <tfhe/tfhe_io.h>

    - Code should have `assert` statements to implement the `ensure` and `requires` statements in the Dafny code.

    - Follow the following Dafny-like code as a pseudo-code.
            // Global security parameter for TFHE operations
const minimum_lambda: int := 128;

method Main(a: int, b: int) 
  returns (result: int)
  // Ensure inputs are non-negative for bitwise operations
  requires a >= 0 && b >= 0
  // Postcondition: Result is the correct bitwise AND
  ensures result == a & b
{
  // Perform the bitwise AND operation
  result := a & b;
  
  // Prove correctness by verifying the AND property holds
  assert result == a & b;
  
  // Additional verification: Each bit follows AND truth table
  assert forall i: int :: 0 <= i < 32 ==> 
    ((result >> i) & 1) == (((a >> i) & 1) & ((b >> i) & 1));
}

    - Follow the following informal code requirements strictly.
            ### TFHE Bitwise AND Implementation in Dafny

This program models the security properties of a TFHE (Fully Homomorphic Encryption) bitwise AND operation while proving functional correctness.

**Security Context:**
* A global constant, `minimum_lambda`, is set to 128 to define the required security level for cryptographic operations.
* This security parameter ensures the encryption scheme provides adequate protection against attacks.

**Functional Correctness:**
* The `Main` method takes two integers `a` and `b` as inputs (representing plaintext values).
* It proves the correctness of the bitwise AND operation by verifying that each bit position follows the AND truth table.
* The implementation ensures that for all possible bit positions, the result matches the expected AND behavior.

    - Code should follow the following structure:
            1. Setup Parameters
            2. Key Generation
            3. Encryption of inputs
            4. Homomorphic Operations
            5. Decryption of the result
            6. Print the result


TASK: Write TFHE code in C that performs a bitwise AND operation on two input integers""".strip()

task_prompt_formal_1 = """ You are an expert coding agent familiar with cryptographic concepts.

Make sure to follow the below instructions:

    - The input will always come from the user.

    - Code should not have any extra logging or print statements.

    - Code should not be a basic C program, it should use TFHE library functions.

    - The TFHE header files to include are:
        #include <tfhe/tfhe.h>
        #include <tfhe/tfhe_io.h>

    - Code should have `assert` statements to implement the `ensure` and `requires` statements in the Dafny code.

    - Follow the following Dafny-like code as a pseudo-code.
            // Global security parameter specification for the TFHE context
const minimum_lambda: int := 128;

method Main(a: int, b: int) returns (result: int)
  // Ensures the result is the correct bitwise AND of the inputs
  ensures result == a & b
{
  result := a & b;

  // Assert a key property to help prove correctness:
  // The result, when OR'd with either input, returns the original input.
  // This is true because the result contains a subset of the bits in 'a' and 'b'.
  assert (result | a) == a;
  assert (result | b) == b;
}

    - Follow the following informal code requirements strictly.
            ### TFHE Bitwise AND Implementation (Dafny Model)

This program models the core functional correctness of a bitwise AND operation, which would be executed within a TFHE (Fully Homomorphic Encryption over the Torus) context in C. The Dafny code focuses on proving the logic's correctness, independent of the cryptographic operations.

**Security Context:**
* A global constant, `minimum_lambda`, is set to 128. This represents the required security parameter (bit strength) for the underlying TFHE cryptographic operations, ensuring the homomorphic encryption provides adequate protection.

**Functional Correctness:**
* The `Main` method takes two integers, `a` and `b`, as inputs.
* It calculates the result of the bitwise AND operation (`a & b`).
* It proves the correctness of this operation by asserting a fundamental property of the AND result: for every single bit position, the result's bit is 1 **if and only if** both input bits at that position are 1. This is verified by checking that `(result | a) == a` and `(result | b) == b`, which are necessary conditions for a correct AND.

    - Code should follow the following structure:
            1. Setup Parameters
            2. Key Generation
            3. Encryption of inputs
            4. Homomorphic Operations
            5. Decryption of the result
            6. Print the result


TASK: Write TFHE code in C that performs a bitwise AND operation on two input integers""".strip()

task_prompt_formal_2 = """ You are an expert coding agent familiar with cryptographic concepts.

Make sure to follow the below instructions:

    - The input will always come from the user.

    - Code should not have any extra logging or print statements.

    - Code should not be a basic C program, it should use TFHE library functions.

    - The TFHE header files to include are:
        #include <tfhe/tfhe.h>
        #include <tfhe/tfhe_io.h>

    - Code should have `assert` statements to implement the `ensure` and `requires` statements in the Dafny code.

    - Follow the following Dafny-like code as a pseudo-code.
            // Global security parameter specification
const minimum_lambda: int := 128;

method Main(a: int, b: int) returns (result: int)
  // Precondition: Inputs must be valid 32-bit integers
  requires 0 <= a < (1 << 32)
  requires 0 <= b < (1 << 32)
  // Postcondition: Result is also a 32-bit integer
  ensures 0 <= result < (1 << 32)
{
  // Perform bitwise AND operation
  result := a & b;
  
  // Assert functional correctness properties
  assert result <= a && result <= b;           // AND result is always less than or equal to both inputs
  assert (a & b) == result;                    // Operation is consistent
  assert (result & a) == result;               // Idempotent property
  assert (result & b) == result;               // Idempotent property
  assert (a & a) == a;                         // Identity property
  assert (b & b) == b;                         // Identity property
  assert (a & 0xFFFFFFFF) == a;                // AND with all ones returns original
  assert (b & 0xFFFFFFFF) == b;                // AND with all ones returns original
  assert (a & 0) == 0;                         // AND with zero returns zero
  assert (b & 0) == 0;                         // AND with zero returns zero
}

    - Follow the following informal code requirements strictly.
            ### TFHE Bitwise AND Implementation in Dafny

This program models TFHE (Fully Homomorphic Encryption) bitwise operations within a defined security context.

**Security Context:**
* A global constant, `minimum_lambda`, is set to 128 to define the required security level for cryptographic operations.
* This security parameter ensures the implementation meets the necessary cryptographic strength requirements.

**Functional Correctness:**
* The `Main` method takes two integers, `a` and `b`, as user inputs.
* It proves the functional correctness of the bitwise AND operation by demonstrating:
  - The result preserves the logical AND property for all bits
  - The operation is idempotent (applying AND with the same value twice yields the same result)
  - The operation follows standard bitwise AND algebraic properties

    - Code should follow the following structure:
            1. Setup Parameters
            2. Key Generation
            3. Encryption of inputs
            4. Homomorphic Operations
            5. Decryption of the result
            6. Print the result


TASK: Write TFHE code in C that performs a bitwise AND operation on two input integers""".strip()

task_prompt_formal_3 = """ You are an expert coding agent familiar with cryptographic concepts.

Make sure to follow the below instructions:

    - The input will always come from the user.

    - Code should not have any extra logging or print statements.

    - Code should not be a basic C program, it should use TFHE library functions.

    - The TFHE header files to include are:
        #include <tfhe/tfhe.h>
        #include <tfhe/tfhe_io.h>

    - Code should have `assert` statements to implement the `ensure` and `requires` statements in the Dafny code.

    - Follow the following Dafny-like code as a pseudo-code.
            // Global security parameter specification
const minimum_lambda: int := 128;

method Main(a: int, b: int)
  // Precondition: Inputs must be non-negative to represent valid bit patterns
  requires a >= 0 && b >= 0
{
  var result: int := 0;
  var bit_position: int := 1;
  
  // Process each bit position (modeling TFHE's bit-level operations)
  while bit_position <= a || bit_position <= b
    invariant result >= 0
    invariant bit_position >= 1
  {
    // Extract current bit from both inputs
    var a_bit: bool := (a & bit_position) != 0;
    var b_bit: bool := (b & bit_position) != 0;
    
    // Perform bitwise AND and set result bit
    if a_bit && b_bit {
      result := result | bit_position;
    }
    
    bit_position := bit_position * 2;
  }
  
  // Assert functional correctness: result equals standard AND operation
  assert result == a & b;
  
  // Additional verification: each bit follows AND truth table
  var verification_bit: int := 1;
  while verification_bit <= result || verification_bit <= a || verification_bit <= b
    invariant verification_bit >= 1
  {
    var result_bit: bool := (result & verification_bit) != 0;
    var expected_bit: bool := ((a & verification_bit) != 0) && ((b & verification_bit) != 0);
    assert result_bit == expected_bit;
    
    verification_bit := verification_bit * 2;
  }
}

    - Follow the following informal code requirements strictly.
            ### TFHE Bitwise AND Implementation

This program models a cryptographic bitwise AND operation suitable for integration with TFHE (Fully Homomorphic Encryption) libraries.

**Security Context:**
* A global constant, `minimum_lambda`, is set to 128 to define the required security level for cryptographic operations.
* This security parameter ensures the implementation meets the necessary cryptographic strength requirements.

**Functional Correctness:**
* The `Main` method takes two integers, `a` and `b`, as inputs (representing plaintext values that would be encrypted in a real TFHE implementation).
* It proves the correctness of the bitwise AND operation by verifying that each bit in the result correctly follows the AND truth table.
* The implementation uses bit-level operations to model how TFHE would perform this operation on encrypted data.

    - Code should follow the following structure:
            1. Setup Parameters
            2. Key Generation
            3. Encryption of inputs
            4. Homomorphic Operations
            5. Decryption of the result
            6. Print the result


TASK: Write TFHE code in C that performs a bitwise AND operation on two input integers""".strip()

task_prompt_formal_4 = """ You are an expert coding agent familiar with cryptographic concepts.

Make sure to follow the below instructions:

    - The input will always come from the user.

    - Code should not have any extra logging or print statements.

    - Code should not be a basic C program, it should use TFHE library functions.

    - The TFHE header files to include are:
        #include <tfhe/tfhe.h>
        #include <tfhe/tfhe_io.h>

    - Code should have `assert` statements to implement the `ensure` and `requires` statements in the Dafny code.

    - Follow the following Dafny-like code as a pseudo-code.
            // Global security parameter for the TFHE cryptographic scheme
const minimum_lambda: int := 128;

method Main(a: int, b: int) returns (result: int)
  // Ensures the result is the correct bitwise AND of the inputs
  ensures result == (a & b)
{
  // Perform the simulated homomorphic AND operation
  result := a & b;

  // This assertion is logically redundant due to the postcondition
  // but serves to explicitly demonstrate the property being proven.
  assert result == (a & b);
}

    - Follow the following informal code requirements strictly.
            ### TFHE Bitwise AND Simulation in Dafny

This program simulates the core logical correctness of a bitwise AND operation, which would be performed on encrypted data in a TFHE (Fully Homomorphic Encryption over the Torus) context. The security parameter ensures the operation meets a defined cryptographic strength.

**Security Context:**
* A global constant, `minimum_lambda`, is set to 128. This represents the required security level (e.g., 128-bit security) for the underlying TFHE scheme, ensuring the homomorphic operations are cryptographically sound.

**Functional Correctness:**
* The `Main` method takes two integers, `a` and `b`, as inputs (simulating encrypted ciphertexts).
* It proves the functional correctness of the bitwise AND operation by asserting that the result adheres to the fundamental truth table of the AND gate for each individual bit.
* This formal verification ensures that if the homomorphic evaluation is correct, the decrypted result will be the exact bitwise AND of the original plaintexts.

    - Code should follow the following structure:
            1. Setup Parameters
            2. Key Generation
            3. Encryption of inputs
            4. Homomorphic Operations
            5. Decryption of the result
            6. Print the result


TASK: Write TFHE code in C that performs a bitwise AND operation on two input integers""".strip()