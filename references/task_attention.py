"""
Function-based Attention Block using OpenFHE CKKS Encryption

Implements scaled dot-product attention:
    Attention(Q, K, V) = softmax(Q @ K^T / sqrt(d_k)) @ V

- Q: (seq_len x d_k)
- K: (seq_len x d_k)
- V: (seq_len x d_v)

Softmax is computed row-wise using SoftmaxCKKSOpenFHE (encrypted),
matrix ops use openfhe_numpy (encrypted).
"""

import numpy as np
import time

try:
    from openfhe import *
    import openfhe_numpy as onp
    OPENFHE_AVAILABLE = True
except ImportError:
    OPENFHE_AVAILABLE = False
    print("Warning: OpenFHE / openfhe_numpy not available")

from softmax_openfhe import SoftmaxCKKSOpenFHE  # your existing softmax class


# -------------------------------------------------------------------------
# CKKS / matrix helper functions (no classes)
# -------------------------------------------------------------------------

def init_ckks_context(mult_depth: int = 30, scale_mod_size: int = 59):
    """
    Initialize a CKKS crypto context and key pair suitable for matrix ops
    with openfhe_numpy.
    """
    if not OPENFHE_AVAILABLE:
        raise ImportError("OpenFHE is required")

    params = CCParamsCKKSRNS()
    params.SetMultiplicativeDepth(mult_depth)
    params.SetScalingModSize(scale_mod_size)
    params.SetFirstModSize(60)
    params.SetScalingTechnique(FIXEDAUTO)
    params.SetSecretKeyDist(UNIFORM_TERNARY)
    # We don't set BatchSize explicitly; default is ringDim/2

    cc = GenCryptoContext(params)
    cc.Enable(PKESchemeFeature.PKE)
    cc.Enable(PKESchemeFeature.LEVELEDSHE)
    cc.Enable(PKESchemeFeature.ADVANCEDSHE)

    keys = cc.KeyGen()
    cc.EvalMultKeyGen(keys.secretKey)
    cc.EvalSumKeyGen(keys.secretKey)

    ring_dim = cc.GetRingDimension()
    print(f"CKKS context initialized: ring_dim={ring_dim}, slots={ring_dim // 2}")

    return cc, keys


def encrypt_matrix(cc, keys, matrix: np.ndarray, mode: str = "tile"):
    """
    Encrypt a NumPy matrix as an openfhe_numpy ciphertext matrix.
    """
    batch_size = cc.GetRingDimension() // 2
    return onp.array(
        cc=cc,
        data=matrix,
        batch_size=batch_size,
        order=onp.ROW_MAJOR,
        fhe_type="C",
        mode=mode,
        public_key=keys.publicKey,
    )


def decrypt_matrix(ct_matrix, keys, unpack_type: str = "original"):
    """
    Decrypt an openfhe_numpy ciphertext matrix back to NumPy.
    """
    return ct_matrix.decrypt(keys.secretKey, unpack_type=unpack_type)


def matmul_encrypted(cc, keys, A_ct, B_ct):
    """
    Encrypted matrix multiplication A_ct @ B_ct using openfhe_numpy.
    Assumes dimensions are compatible.
    """
    # Generate rotation keys required for square matmul
    # (use number of columns of A)
    onp.EvalSquareMatMultRotateKeyGen(keys.secretKey, A_ct.ncols)
    return A_ct @ B_ct


def matmul_with_transpose_encrypted(cc, keys, A_ct, B_ct):
    """
    Encrypted matrix multiplication A_ct @ B_ct^T.
    """
    B_T_ct = onp.transpose(B_ct)
    return matmul_encrypted(cc, keys, A_ct, B_T_ct)


# -------------------------------------------------------------------------
# Attention-specific helpers (functions, no class)
# -------------------------------------------------------------------------

def attention_scores_encrypted(cc, keys, Q_ct, K_ct):
    """
    Compute encrypted attention scores: Q @ K^T.
    Returns an openfhe_numpy ciphertext matrix.
    """
    return matmul_with_transpose_encrypted(cc, keys, Q_ct, K_ct)


def softmax_rowwise_encrypted(scores_ct, d_k: int, softmax_cfg: dict):
    """
    Apply encrypted softmax row-wise to decrypted attention scores.

    scores_ct: openfhe_numpy array (ciphertext)
    d_k: key/query dimension
    softmax_cfg: dict with config for SoftmaxCKKSOpenFHE, e.g.
      {
        "n": d_k,
        "K": 64,
        "scale_factor": 8,
        "mult_depth": 30
      }

    Returns:
        attention_weights (NumPy array, seq_len x d_k)
    """
    # Decrypt scores
    cc_mat = softmax_cfg.get("cc_mat")    # not required, but we keep cfg generic
    keys_mat = softmax_cfg.get("keys_mat")
    scores = decrypt_matrix(scores_ct, keys_mat, unpack_type="original")

    nrows, ncols = scores.shape
    scale = 1.0 / np.sqrt(d_k)

    # Initialize a separate CKKS context for softmax if not provided
    # (this is how SoftmaxCKKSOpenFHE is designed: its own context)
    softmax = SoftmaxCKKSOpenFHE(
        n=softmax_cfg["n"],
        K=softmax_cfg["K"],
        scale_factor=softmax_cfg["scale_factor"],
        mult_depth=softmax_cfg["mult_depth"],
    )

    # Scale scores and apply softmax per row
    scores = scores * scale
    attention_weights = np.zeros_like(scores)

    for i in range(nrows):
        row = scores[i, :]
        # Pad to softmax dimension if needed
        if len(row) < softmax.n:
            row_padded = np.zeros(softmax.n)
            row_padded[: len(row)] = row
        else:
            row_padded = row[: softmax.n]

        row_softmax = softmax.softmax_encrypted(row_padded)
        attention_weights[i, :] = row_softmax[:ncols]

    return attention_weights


def attention_encrypted(Q: np.ndarray,
                        K: np.ndarray,
                        V: np.ndarray,
                        mult_depth: int = 30,
                        scale_mod_size: int = 59,
                        softmax_K: int = 64,
                        softmax_scale_factor: int = 8):
    """
    Full attention pipeline:

      Attention(Q, K, V) = softmax(Q @ K^T / sqrt(d_k)) @ V

    - Q, K, V: NumPy arrays
    - Returns: (output, attention_weights)
    """
    if not OPENFHE_AVAILABLE:
        raise ImportError("OpenFHE / openfhe_numpy is required")

    print("\n" + "=" * 80)
    print("  Computing Attention Block (function-based)")
    print("=" * 80)

    seq_len, d_k = Q.shape
    assert K.shape == (seq_len, d_k), "K shape mismatch"
    d_v = V.shape[1]
    assert V.shape == (seq_len, d_v), "V shape mismatch"

    # 1) Init CKKS context for matrix ops
    cc, keys = init_ckks_context(mult_depth=mult_depth, scale_mod_size=scale_mod_size)

    # 2) Encrypt Q, K, V
    print("\n[1/5] Encrypting Q, K, V matrices...")
    start = time.time()
    Q_ct = encrypt_matrix(cc, keys, Q, mode="tile")
    K_ct = encrypt_matrix(cc, keys, K, mode="tile")
    V_ct = encrypt_matrix(cc, keys, V, mode="tile")
    print(f"  Encryption time: {time.time() - start:.2f}s")

    # 3) Compute attention scores Q @ K^T
    print("\n[2/5] Computing attention scores (Q @ K^T)...")
    start = time.time()
    scores_ct = attention_scores_encrypted(cc, keys, Q_ct, K_ct)
    print(f"  Attention scores time: {time.time() - start:.2f}s")

    # 4) Apply scaling + softmax row-wise (softmax is encrypted internally)
    print("\n[3/5] Applying softmax to attention scores (with scaling)...")
    start = time.time()
    softmax_cfg = {
        "n": d_k,
        "K": softmax_K,
        "scale_factor": softmax_scale_factor,
        "mult_depth": mult_depth,
        "cc_mat": cc,
        "keys_mat": keys,
    }
    attention_weights = softmax_rowwise_encrypted(scores_ct, d_k, softmax_cfg)
    print(f"  Softmax time: {time.time() - start:.2f}s")
    print(f"  Attention weights shape: {attention_weights.shape}")

    # 5) Encrypt attention weights
    print("\n[4/5] Encrypting attention weights...")
    start = time.time()
    attention_weights_ct = encrypt_matrix(cc, keys, attention_weights, mode="tile")
    print(f"  Encryption time: {time.time() - start:.2f}s")

    # 6) Compute final output = attention_weights @ V (encrypted)
    print("\n[5/5] Computing final output (attention_weights @ V)...")
    start = time.time()
    output_ct = matmul_encrypted(cc, keys, attention_weights_ct, V_ct)
    output = decrypt_matrix(output_ct, keys, unpack_type="original")
    print(f"  Final computation time: {time.time() - start:.2f}s")
    print(f"  Output shape: {output.shape}")

    return output, attention_weights


# -------------------------------------------------------------------------
# NumPy reference attention
# -------------------------------------------------------------------------

def numpy_attention(Q, K, V):
    """
    Reference attention implementation using NumPy.
    """
    d_k = Q.shape[1]
    scores = Q @ K.T / np.sqrt(d_k)

    attention_weights = np.zeros_like(scores)
    for i in range(scores.shape[0]):
        row = scores[i, :]
        exp_row = np.exp(row - np.max(row))  # numerical stability
        attention_weights[i, :] = exp_row / np.sum(exp_row)

    output = attention_weights @ V
    return output, attention_weights


# -------------------------------------------------------------------------
# Basic test
# -------------------------------------------------------------------------

if __name__ == "__main__":
    print("=" * 80)
    print("  Attention Block - Function-based Test")
    print("=" * 80)

    if not OPENFHE_AVAILABLE:
        print("\n❌ OpenFHE / openfhe_numpy not available")
        print("Install with: pip install openfhe openfhe_numpy")
        raise SystemExit(1)

    # Small test case
    seq_len, d_k, d_v = 4, 4, 4
    print(f"\nTest configuration:")
    print(f"  Sequence length: {seq_len}")
    print(f"  Key/Query dimension: {d_k}")
    print(f"  Value dimension: {d_v}")

    # Random Q, K, V
    np.random.seed(42)
    Q = np.random.randn(seq_len, d_k)
    K = np.random.randn(seq_len, d_k)
    V = np.random.randn(seq_len, d_v)

    print(f"\nInput matrices:")
    print(f"  Q shape: {Q.shape}")
    print(f"  K shape: {K.shape}")
    print(f"  V shape: {V.shape}")

    # Reference
    print("\nComputing reference (NumPy)...")
    ref_output, ref_weights = numpy_attention(Q, K, V)

    # Encrypted
    print("\nComputing encrypted attention (function-based)...")
    start = time.time()
    enc_output, enc_weights = attention_encrypted(
        Q, K, V,
        mult_depth=30,
        softmax_K=32,          # smaller K for speed in test
        softmax_scale_factor=4
    )
    compute_time = time.time() - start

    print(f"\n{'=' * 80}")
    print("  Results")
    print("=" * 80)
    print(f"\nTotal computation time: {compute_time:.2f}s")

    print(f"\nOutput comparison (first row):")
    print(f"  Reference: {ref_output[0, :]}")
    print(f"  Encrypted: {enc_output[0, :]}")

    output_error = np.max(np.abs(enc_output - ref_output))
    print(f"\n  Max output error: {output_error:.6f}")

    print(f"\nAttention weights comparison (first row):")
    print(f"  Reference: {ref_weights[0, :]}")
    print(f"  Encrypted: {enc_weights[0, :]}")

    weights_error = np.max(np.abs(enc_weights - ref_weights))
    print(f"\n  Max weights error: {weights_error:.6f}")

    if output_error < 0.1 and weights_error < 0.1:
        print("\n✅ TEST PASSED - Attention computation acceptable")
    else:
        print("\n⚠️  TEST WARNING - Large error (check softmax approximation)")
