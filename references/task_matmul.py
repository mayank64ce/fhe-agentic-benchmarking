import sys
import numpy as np
from openfhe import *
import openfhe_numpy as onp


def read_matrices_from_stdin():
    """
    Read two matrices A and B from stdin.

    Expected stdin format:
        n1 m1 n2 m2
        <n1 * m1 entries for A, row-major>
        <n2 * m2 entries for B, row-major>

    Example:
        3 3 3 3
        1 2 3 4 5 6 7 8 9
        9 8 7 6 5 4 3 2 1
    """
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
    """
    Encrypt A and B with CKKS, compute A @ B homomorphically, and decrypt the result.
    """

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
