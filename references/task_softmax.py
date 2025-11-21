import sys
import numpy as np
from openfhe import *


# ---------- Basic CKKS setup ----------

def init_ckks(n=8, mult_depth=10):
    """
    Initialize a CKKS crypto context and keys.

    Args:
        n: batch size / number of slots we will use
        mult_depth: multiplicative depth

    Returns:
        cc  : crypto context
        keys: key pair (publicKey, secretKey)
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
    cc.EvalSumKeyGen(keys.secretKey)

    # rotation keys for sum-of-slots (for n=8 we need 1,2,4)
    cc.EvalRotateKeyGen(keys.secretKey, [1, 2, 4])

    return cc, keys


# ---------- Encrypt / decrypt helpers ----------

def encrypt_vector(cc, public_key, x, n):
    """
    Encrypt a real vector x into a CKKS ciphertext.

    Pads or truncates to length n.
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
    Decrypt ciphertext to a numpy vector of length n.
    """
    pt = cc.Decrypt(secret_key, ct)
    pt.SetLength(n)
    vals = pt.GetRealPackedValue()
    return np.array(vals[:n])


# ---------- Homomorphic building blocks ----------

def homomorphic_exp_poly(cc, ct, n):
    """
    Approximate exp(x) on encrypted slots with a simple polynomial:
        exp(x) ≈ 1 + x + x^2 / 2

    This is just for demonstration and is NOT a high-precision exp.
    """
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
    """
    Sum all n slots using rotate-and-add (for n=8: rotations 1,2,4).
    Result: ciphertext where each slot contains the total sum.
    """
    s = ct
    # for n=8: log2(8) = 3 steps: rotate by 1, 2, 4
    for shift in [1, 2, 4]:
        rotated = cc.EvalRotate(s, shift)
        s = cc.EvalAdd(s, rotated)
    return s


# ---------- Softmax pipeline ----------

def ckks_softmax(cc, keys, x, n=8):
    """
    Compute softmax(x) homomorphically with a simple exp approximation.

    Steps:
      1) Shift x by max(x) for numerical stability (in plaintext).
      2) Encrypt shifted x.
      3) Compute exp(x) approximately on ciphertext.
      4) Sum all slots homomorphically.
      5) Decrypt sum and divide plaintext-wise (1/s) as a simple demo.
      6) Multiply encrypted exponentials by 1/s (in ciphertext).
      7) Decrypt result.

    Returns:
      softmax_values: numpy array of length n (first len(x) entries are valid)
    """
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
    """
    Read up to n numbers from stdin (space or newline separated).
    If fewer than n provided, pad with zeros.
    """
    tokens = sys.stdin.read().strip().split()
    if len(tokens) == 0:
        raise ValueError(f"Expected at least 1 number, got 0.")

    vals = list(map(float, tokens[:n]))
    return np.array(vals, dtype=float)


def main():
    n = 8  # number of slots / vector length we use

    x = read_vector_from_stdin(n)

    # Initialize CKKS
    print("\nInitializing CKKS context...")
    cc, keys = init_ckks(n=n, mult_depth=10)

    # CKKS softmax
    print("Computing homomorphic softmax...")
    softmax_he = ckks_softmax(cc, keys, x, n=n)


    np.set_printoptions(suppress=True, precision=6)

    print(softmax_he)



if __name__ == "__main__":
    main()
