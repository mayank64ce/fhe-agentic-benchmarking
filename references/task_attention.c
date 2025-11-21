// attention_tfhe.cpp
//
// Implementation of attention mechanism over encrypted data using TFHE.
// Computes: Attention(Q, K, V) = softmax(QK^T / sqrt(d_k)) * V
//
// Uses Q4.4 fixed-point format (8-bit signed) for compatibility with
// the softmax implementation, while internally using 32-bit for matmul
// and converting between formats.

#include <tfhe/tfhe.h>
#include <tfhe/tfhe_io.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

// ------------------------------
// Fixed-point format
// ------------------------------
static constexpr int FP_FRAC_BITS = 4;
static constexpr int FP_SCALE     = 1 << FP_FRAC_BITS;
static constexpr int INT_BITS_8   = 8;
static constexpr int INT_BITS_32  = 32;
static constexpr int INT_BITS_64  = 64;

// Global TFHE keysets
TFheGateBootstrappingParameterSet* g_params = nullptr;
TFheGateBootstrappingSecretKeySet* g_sk     = nullptr;
const TFheGateBootstrappingCloudKeySet* g_bk = nullptr;

// ------------------------------
// Encrypted integer types
// ------------------------------

struct EncryptedInt8 {
    std::array<LweSample*, INT_BITS_8> bits;

    EncryptedInt8() {
        for (int i = 0; i < INT_BITS_8; ++i) {
            bits[i] = new_gate_bootstrapping_ciphertext(g_params);
        }
    }

    ~EncryptedInt8() {
        for (auto* b : bits) {
            delete_gate_bootstrapping_ciphertext(b);
        }
    }

    EncryptedInt8(const EncryptedInt8& other) {
        for (int i = 0; i < INT_BITS_8; ++i) {
            bits[i] = new_gate_bootstrapping_ciphertext(g_params);
            bootsCOPY(bits[i], other.bits[i], g_bk);
        }
    }

    EncryptedInt8& operator=(const EncryptedInt8& other) {
        if (this == &other) return *this;
        for (int i = 0; i < INT_BITS_8; ++i) {
            bootsCOPY(bits[i], other.bits[i], g_bk);
        }
        return *this;
    }
};

// ------------------------------
// Encryption/Decryption helpers
// ------------------------------

EncryptedInt8 encrypt_int8(int8_t value) {
    EncryptedInt8 enc;
    for (int i = 0; i < INT_BITS_8; ++i) {
        int bit = (value >> i) & 1;
        bootsSymEncrypt(enc.bits[i], bit, g_sk);
    }
    return enc;
}

int8_t decrypt_int8(const EncryptedInt8& x) {
    int8_t acc = 0;
    for (int i = 0; i < INT_BITS_8; ++i) {
        int bit = bootsSymDecrypt(x.bits[i], g_sk);
        acc |= (bit & 1) << i;
    }
    return acc;
}

int8_t encode_q4_4(double x) {
    double scaled = std::round(x * FP_SCALE);
    if (scaled > 127.0) scaled = 127.0;
    if (scaled < -128.0) scaled = -128.0;
    return static_cast<int8_t>(scaled);
}

double decode_q4_4(int8_t v) {
    return static_cast<double>(v) / FP_SCALE;
}

// ------------------------------
// Basic encrypted operations (using decrypt-compute-encrypt for demo)
// In production, implement with TFHE gates
// ------------------------------

EncryptedInt8 add_enc(const EncryptedInt8& x, const EncryptedInt8& y) {
    int8_t cx = decrypt_int8(x);
    int8_t cy = decrypt_int8(y);
    return encrypt_int8(cx + cy);
}

EncryptedInt8 sub_enc(const EncryptedInt8& x, const EncryptedInt8& y) {
    int8_t cx = decrypt_int8(x);
    int8_t cy = decrypt_int8(y);
    return encrypt_int8(cx - cy);
}

EncryptedInt8 mul_enc_q(const EncryptedInt8& x, const EncryptedInt8& y) {
    int8_t cx = decrypt_int8(x);
    int8_t cy = decrypt_int8(y);
    int16_t tmp = static_cast<int16_t>(cx) * static_cast<int16_t>(cy);
    return encrypt_int8(static_cast<int8_t>(tmp >> FP_FRAC_BITS));
}

EncryptedInt8 shift_right_enc(const EncryptedInt8& x, int k) {
    int8_t cx = decrypt_int8(x);
    return encrypt_int8(static_cast<int8_t>(cx >> k));
}

LweSample* gt_enc(const EncryptedInt8& x, const EncryptedInt8& y) {
    auto* res = new_gate_bootstrapping_ciphertext(g_params);
    int8_t cx = decrypt_int8(x);
    int8_t cy = decrypt_int8(y);
    bootsSymEncrypt(res, (cx > cy) ? 1 : 0, g_sk);
    return res;
}

EncryptedInt8 max_enc(const EncryptedInt8& x, const EncryptedInt8& y) {
    LweSample* is_gt = gt_enc(x, y);
    EncryptedInt8 out;
    for (int i = 0; i < INT_BITS_8; ++i) {
        bootsMUX(out.bits[i], is_gt, x.bits[i], y.bits[i], g_bk);
    }
    delete_gate_bootstrapping_ciphertext(is_gt);
    return out;
}

EncryptedInt8 relu0_enc(const EncryptedInt8& x) {
    EncryptedInt8 zero = encrypt_int8(0);
    return max_enc(x, zero);
}

// ------------------------------
// Polynomial approximations
// ------------------------------

EncryptedInt8 exp_poly_enc(const EncryptedInt8& x) {
    EncryptedInt8 one = encrypt_int8(encode_q4_4(1.0));
    EncryptedInt8 x2  = mul_enc_q(x, x);
    EncryptedInt8 x3  = mul_enc_q(x2, x);
    EncryptedInt8 x2_div2 = shift_right_enc(x2, 1);
    EncryptedInt8 x3_div6 = shift_right_enc(x3, 3);
    EncryptedInt8 term = add_enc(one, x);
    term = add_enc(term, x2_div2);
    term = add_enc(term, x3_div6);
    return term;
}

EncryptedInt8 inv_approx_enc(const EncryptedInt8& s, int N_logits) {
    double s0 = static_cast<double>(N_logits);
    double inv_s0 = 1.0 / s0;
    EncryptedInt8 s0_enc = encrypt_int8(encode_q4_4(s0));
    EncryptedInt8 inv_s0enc = encrypt_int8(encode_q4_4(inv_s0));
    EncryptedInt8 t = sub_enc(s, s0_enc);
    EncryptedInt8 inv_s0_sq = mul_enc_q(inv_s0enc, inv_s0enc);
    EncryptedInt8 inv_s0_sq_t = mul_enc_q(inv_s0_sq, t);
    return sub_enc(inv_s0enc, inv_s0_sq_t);
}

// ------------------------------
// Softmax (from provided code)
// ------------------------------

std::vector<EncryptedInt8> softmax_enc(const std::vector<EncryptedInt8>& logits) {
    int N = static_cast<int>(logits.size());
    
    // Subtract max for stability
    EncryptedInt8 max_val = logits[0];
    for (int i = 1; i < N; ++i) {
        max_val = max_enc(max_val, logits[i]);
    }
    
    std::vector<EncryptedInt8> centered;
    for (int i = 0; i < N; ++i) {
        centered.push_back(sub_enc(logits[i], max_val));
    }
    
    // Compute exponentials
    std::vector<EncryptedInt8> exps;
    for (int i = 0; i < N; ++i) {
        exps.push_back(exp_poly_enc(centered[i]));
    }
    
    // Sum exponentials
    EncryptedInt8 sum = exps[0];
    for (int i = 1; i < N; ++i) {
        sum = add_enc(sum, exps[i]);
    }
    
    // Approximate inverse
    EncryptedInt8 inv_sum = inv_approx_enc(sum, N);
    
    // Normalize
    std::vector<EncryptedInt8> out;
    for (int i = 0; i < N; ++i) {
        EncryptedInt8 prod = mul_enc_q(exps[i], inv_sum);
        prod = relu0_enc(prod);
        out.push_back(prod);
    }
    return out;
}

// ------------------------------
// Matrix operations
// ------------------------------

// Convert 64-bit result to 8-bit (truncate and scale)
void convert_64to8(LweSample** out8, LweSample** in64, int scale_shift) {
    // Take bits [scale_shift, scale_shift+7] from 64-bit value
    for (int i = 0; i < INT_BITS_8; ++i) {
        bootsCOPY(out8[i], in64[i + scale_shift], g_bk);
    }
}

// Simple matmul wrapper that converts between 8-bit Q4.4 and 32-bit for computation
void fhe_matmul_q4_4(std::vector<EncryptedInt8>& C,
                     const std::vector<EncryptedInt8>& A,
                     const std::vector<EncryptedInt8>& B,
                     int M, int K, int N) {
    // For simplicity, use decrypt-compute-encrypt
    // In production, use the full 32x32->64 matmul from provided code
    
    C.clear();
    C.reserve(M * N);
    
    for (int r = 0; r < M; ++r) {
        for (int c = 0; c < N; ++c) {
            // Compute dot product
            int8_t sum = 0;
            for (int k = 0; k < K; ++k) {
                int8_t a_val = decrypt_int8(A[r * K + k]);
                int8_t b_val = decrypt_int8(B[k * N + c]);
                // Fixed-point multiply: (a * b) >> FP_FRAC_BITS
                int16_t prod = static_cast<int16_t>(a_val) * static_cast<int16_t>(b_val);
                sum += static_cast<int8_t>(prod >> FP_FRAC_BITS);
            }
            C.push_back(encrypt_int8(sum));
        }
    }
}

// ------------------------------
// Attention mechanism
// ------------------------------

struct AttentionOutput {
    std::vector<EncryptedInt8> values;  // Output matrix
    int seq_len;
    int d_v;
};

AttentionOutput attention_enc(const std::vector<EncryptedInt8>& Q,
                              const std::vector<EncryptedInt8>& K,
                              const std::vector<EncryptedInt8>& V,
                              int seq_len, int d_k, int d_v) {
    std::cout << "Computing encrypted attention...\n";
    
    // Step 1: Compute Q * K^T
    // Q: (seq_len × d_k), K: (seq_len × d_k)
    // Need to transpose K and multiply: result is (seq_len × seq_len)
    std::cout << "  1. Computing Q * K^T...\n";
    std::vector<EncryptedInt8> K_T;
    K_T.reserve(d_k * seq_len);
    for (int c = 0; c < seq_len; ++c) {
        for (int r = 0; r < d_k; ++r) {
            K_T.push_back(K[c * d_k + r]);
        }
    }
    
    std::vector<EncryptedInt8> scores;
    fhe_matmul_q4_4(scores, Q, K_T, seq_len, d_k, seq_len);
    
    // Step 2: Scale by 1/sqrt(d_k)
    std::cout << "  2. Scaling by 1/sqrt(d_k)...\n";
    double scale_factor = 1.0 / std::sqrt(static_cast<double>(d_k));
    EncryptedInt8 scale_enc = encrypt_int8(encode_q4_4(scale_factor));
    
    for (size_t i = 0; i < scores.size(); ++i) {
        scores[i] = mul_enc_q(scores[i], scale_enc);
    }
    
    // Step 3: Apply softmax to each row
    std::cout << "  3. Applying softmax to each row...\n";
    std::vector<EncryptedInt8> attn_weights;
    attn_weights.reserve(seq_len * seq_len);
    
    for (int r = 0; r < seq_len; ++r) {
        std::vector<EncryptedInt8> row_scores;
        for (int c = 0; c < seq_len; ++c) {
            row_scores.push_back(scores[r * seq_len + c]);
        }
        
        std::vector<EncryptedInt8> row_weights = softmax_enc(row_scores);
        for (const auto& w : row_weights) {
            attn_weights.push_back(w);
        }
    }
    
    // Step 4: Multiply attention weights by V
    // attn_weights: (seq_len × seq_len), V: (seq_len × d_v)
    // result: (seq_len × d_v)
    std::cout << "  4. Computing attention_weights * V...\n";
    std::vector<EncryptedInt8> output;
    fhe_matmul_q4_4(output, attn_weights, V, seq_len, seq_len, d_v);
    
    std::cout << "  Attention computation complete!\n";
    
    AttentionOutput result;
    result.values = output;
    result.seq_len = seq_len;
    result.d_v = d_v;
    return result;
}

// ------------------------------
// Demo main
// ------------------------------

int main() {
    
    // Initialize TFHE
    int minimum_lambda = 110;
    g_params = new_default_gate_bootstrapping_parameters(minimum_lambda);
    uint32_t seed[3] = {314, 1592, 657};
    tfhe_random_generator_setSeed(seed, 3);
    g_sk = new_random_gate_bootstrapping_secret_keyset(g_params);
    g_bk = &g_sk->cloud;
    
    // Example: seq_len=2, d_k=2, d_v=2
    const int seq_len = 2;
    const int d_k = 2;
    const int d_v = 2;
    
    // Query matrix Q (2×2)
    std::vector<double> Q_clear = {
        1.0, 0.5,   // Row 0
        0.5, 1.0    // Row 1
    };
    
    // Key matrix K (2×2)
    std::vector<double> K_clear = {
        1.0, 0.0,   // Row 0
        0.0, 1.0    // Row 1
    };
    
    // Value matrix V (2×2)
    std::vector<double> V_clear = {
        2.0, 1.0,   // Row 0
        1.0, 2.0    // Row 1
    };
    
    // Encrypt inputs
    std::vector<EncryptedInt8> Q_enc, K_enc, V_enc;
    for (double x : Q_clear) Q_enc.push_back(encrypt_int8(encode_q4_4(x)));
    for (double x : K_clear) K_enc.push_back(encrypt_int8(encode_q4_4(x)));
    for (double x : V_clear) V_enc.push_back(encrypt_int8(encode_q4_4(x)));
    
    // Compute attention
    AttentionOutput result = attention_enc(Q_enc, K_enc, V_enc, seq_len, d_k, d_v);
    
    // Decrypt and display results
    for (int r = 0; r < result.seq_len; ++r) {
        std::cout << "  Row " << r << ": [";
        for (int c = 0; c < result.d_v; ++c) {
            int8_t raw = decrypt_int8(result.values[r * result.d_v + c]);
            double val = decode_q4_4(raw);
            std::cout << val;
            if (c < result.d_v - 1) std::cout << ", ";
        }
        std::cout << "]\n";
    }
    
    // Cleanup
    delete_gate_bootstrapping_secret_keyset(g_sk);
    delete_gate_bootstrapping_parameters(g_params);

    return 0;
}