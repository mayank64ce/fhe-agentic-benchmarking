import sys
import numpy as np
from openfhe import *


# ---------- CKKS setup ----------

def init_ckks(n=8, mult_depth=10):
    """
    Initialize a CKKS crypto context and keys.

    Args:
        n: vector length / batch size
        mult_depth: multiplicative depth

    Returns:
        cc   : crypto context
        keys : key pair with publicKey and secretKey
    """
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

    return cc, keys


# ---------- Encrypt / decrypt helpers ----------

def encrypt_vector(cc, public_key, x, n):
    """
    Encrypt real vector x into CKKS ciphertext (padded/truncated to length n).
    """
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
    """
    Decrypt ciphertext to numpy vector of length n.
    """
    pt = cc.Decrypt(secret_key, ct)
    pt.SetLength(n)
    vals = pt.GetRealPackedValue()
    return np.array(vals[:n])


# ---------- ReLU polynomial coefficients ----------

def relu_poly_coeffs(degree=7):
    """
    Return polynomial coefficients for ReLU approximation on [-5,5].

    P(x) ≈ max(0, x)
    Coefficients are in increasing order:
        P(x) = c0 + c1 x + c2 x^2 + ... + c_d x^d

    Degrees: 3, 5, 7, or 9.
    """
    if degree == 3:
        # Degree 3: fast, less accurate
        return np.array([0.469219, 0.500000, 0.093656, 0.000000])
    elif degree == 5:
        # Degree 5: balanced
        return np.array([0.293262, 0.500000, 0.163899, -0.000000, -0.003271, 0.000000])
    elif degree == 7:
        # Degree 7: accurate (default)
        return np.array([0.213837, 0.500000, 0.230484, -0.000000,
                         -0.011246, 0.000000, 0.000233, 0.000000])
    elif degree == 9:
        # Degree 9: more accurate, deeper
        return np.array([0.168397, 0.500000, 0.295788, -0.000000,
                         -0.025584, 0.000000, 0.001226, -0.000000,
                         -0.000021, 0.000000])
    else:
        raise ValueError(f"Unsupported degree {degree}. Use 3, 5, 7, or 9.")


# ---------- Homomorphic polynomial evaluation ----------

def eval_poly_encrypted(cc, coeffs, x_ct, n):
    """
    Evaluate polynomial P(x) = c0 + c1 x + ... + c_d x^d on encrypted x_ct.

    Uses Horner's method:
        P(x) = (...((c_d x + c_{d-1}) x + c_{d-2})... ) x + c_0

    All coefficients are encoded as plaintext vectors and combined with x_ct.
    """
    # Start with highest-degree coefficient as plaintext
    c_last = coeffs[-1]
    result_ct = cc.MakeCKKSPackedPlaintext([c_last] * n)  # plaintext

    # Horner loop: multiply by x, then add next coefficient
    for i in range(len(coeffs) - 2, -1, -1):
        # result = result * x
        result_ct = cc.EvalMult(x_ct, result_ct)  # ciphertext result

        # add c_i
        c_i_pt = cc.MakeCKKSPackedPlaintext([coeffs[i]] * n)
        result_ct = cc.EvalAdd(result_ct, c_i_pt)

    return result_ct


# ---------- CKKS ReLU pipeline ----------

def ckks_relu(cc, keys, x, n=8, degree=7):
    """
    Compute approximated ReLU(x) homomorphically with a polynomial.

    Steps:
      1) Get polynomial coefficients.
      2) Encrypt input vector x.
      3) Evaluate polynomial on ciphertext.
      4) Decrypt result.

    Returns:
      numpy array of length len(x) with approximated ReLU.
    """
    x = np.array(x, dtype=float)
    L = len(x)
    if L > n:
        x = x[:n]
        L = n

    coeffs = relu_poly_coeffs(degree=degree)

    # Encrypt x
    x_ct = encrypt_vector(cc, keys.publicKey, x, n)

    # Evaluate polynomial
    relu_ct = eval_poly_encrypted(cc, coeffs, x_ct, n)

    # Decrypt
    relu_vals = decrypt_vector(cc, keys.secretKey, relu_ct, n)

    # Return only the actual length (discard padding)
    return relu_vals[:L]


# ---------- Reference ReLU ----------

def numpy_relu(x):
    return np.maximum(0.0, x)


# ---------- Input / main ----------

def read_vector_from_stdin(n=8):
    """
    Read up to n numbers from stdin (space or newline separated).
    If more than n numbers are given, the extra ones are ignored.
    """
    tokens = sys.stdin.read().strip().split()
    if len(tokens) == 0:
        raise ValueError("Expected at least one number as input.")

    vals = list(map(float, tokens[:n]))
    return np.array(vals, dtype=float)


def main():
    n = 8        # number of slots
    degree = 7   # polynomial degree (3, 5, 7, or 9)

    x = read_vector_from_stdin(n)

    cc, keys = init_ckks(n=n, mult_depth=10)

    relu_he = ckks_relu(cc, keys, x, n=n, degree=degree)

    np.set_printoptions(suppress=True, precision=6)

    print(relu_he)


if __name__ == "__main__":
    main()
