task_prompt_formal_transformer = """
You are an expert coding agent familiar with cryptographic concepts.

Make sure to follow the below instructions:

    - The input will always come from the user.

    - Code should not have any extra logging or print statements.

    - Code should not be a basic python program, it should use openfhe_numpy library functions.

    - The OpenFHE numpy header files to include are:
            import numpy as np
            from openfhe import *
            import openfhe_numpy as onp

    - Code should have `assert` statements to implement the `ensure` and `requires` statements in the Dafny code.

    - Follow the following Dafny-like code as a pseudo-code.
            // Global security parameter specification
const minimum_lambda: int := 150;

method Main(dim: int, heads: int, ff_dim: int) returns (output_dim: int)
  // Precondition: Input dimensions must be valid and consistent
  requires dim > 0
  requires heads > 0
  requires ff_dim > 0
  // Postcondition: Output dimensionality matches input (transformers preserve dimensions)
  ensures output_dim == dim
{
  // Simulate multi-head attention dimensionality consistency check
  assert dim % heads == 0 ==> true; // Ensure divisible for multi-head attention
  
  // Simulate feed-forward network expansion and projection back to original dimension
  var intermediate := ff_dim;
  var projected := dim;
  
  // The transformer block output dimension always equals the input dimension
  output_dim := dim;
  
  // Formal proof that the transformer preserves dimensionality
  assert output_dim == dim;
}

    - Follow the following informal code requirements strictly.
            ### Transformer Block Implementation

This program provides a formal model of a core component in neural networks, the Transformer block, for use in security-critical AI systems where functional correctness is paramount.

**Security Context:**
* A global constant, `minimum_lambda`, is set to 150 to define the program's required security level for neural network operations.

**Functional Correctness:**
* The `Main` method models a simplified transformer block with parameters for dimensionality (`dim`), number of attention heads (`heads`), and intermediate feed-forward dimension (`ff_dim`)
* The implementation ensures dimensional consistency between input and output
* The postcondition formally proves that the output maintains the same dimensionality as the input, which is a fundamental correctness property of transformer blocks
* This mathematical guarantee is crucial for systems where neural network operations must be provably correct

    - Code should follow the following structure:
            1. Setup Parameters
            2. Key Generation
            3. Encryption of inputs
            4. Homomorphic Operations
            5. Decryption of the result
            6. Print the result


TASK: Write a python code to build a transformer block
""".strip()