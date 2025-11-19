// tfhe_conv5x5_3x3_hardcoded.c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <tfhe/tfhe.h>
#include <tfhe/tfhe_io.h>

/******** One-bit full adder ********/
static inline void full_adder(LweSample* s, LweSample* cout,
                              const LweSample* a, const LweSample* b, const LweSample* cin,
                              const TFheGateBootstrappingCloudKeySet* bk) {
    LweSample* t = new_gate_bootstrapping_ciphertext(bk->params);
    LweSample* u = new_gate_bootstrapping_ciphertext(bk->params);
    LweSample* v = new_gate_bootstrapping_ciphertext(bk->params);
    bootsXOR(t, a, b, bk);
    bootsXOR(s, t, cin, bk);
    bootsAND(u, a, b, bk);
    bootsAND(v, cin, t, bk);
    bootsOR(cout, u, v, bk);
    delete_gate_bootstrapping_ciphertext(t);
    delete_gate_bootstrapping_ciphertext(u);
    delete_gate_bootstrapping_ciphertext(v);
}

/******** Ripple add on bit arrays: sum = a + b (+carry_in). Arrays LSB-first. ********/
void fhe_add_bits(LweSample** sum, LweSample** a, LweSample** b, int nbits,
                  const TFheGateBootstrappingCloudKeySet* bk,
                  LweSample* carry_out, const LweSample* carry_in) {
    LweSample* cin = new_gate_bootstrapping_ciphertext(bk->params);
    if (carry_in) bootsCOPY(cin, carry_in, bk); else bootsCONSTANT(cin, 0, bk);
    for (int i = 0; i < nbits; i++) {
        LweSample* cout = new_gate_bootstrapping_ciphertext(bk->params);
        full_adder(sum[i], cout, a[i], b[i], cin, bk);
        delete_gate_bootstrapping_ciphertext(cin);
        cin = cout;
    }
    if (carry_out) bootsCOPY(carry_out, cin, bk);
    delete_gate_bootstrapping_ciphertext(cin);
}

/******** Unsigned schoolbook multiply: prod(2n) = a(n) * b(n) ********/
void fhe_mult_unsigned(LweSample** prod, LweSample** a, LweSample** b, int nbits,
                       const TFheGateBootstrappingCloudKeySet* bk) {
    for (int i = 0; i < 2*nbits; i++) bootsCONSTANT(prod[i], 0, bk);
    LweSample* ZERO = new_gate_bootstrapping_ciphertext(bk->params);
    bootsCONSTANT(ZERO, 0, bk);
    for (int j = 0; j < nbits; j++) {
        LweSample** row = (LweSample**)malloc(nbits * sizeof(LweSample*));
        for (int i = 0; i < nbits; i++) {
            row[i] = new_gate_bootstrapping_ciphertext(bk->params);
            bootsAND(row[i], a[i], b[j], bk);
        }
        LweSample* carry = new_gate_bootstrapping_ciphertext(bk->params);
        bootsCONSTANT(carry, 0, bk);
        for (int k = 0; k < nbits; k++) {
            int idx = j + k;
            LweSample* a_copy = new_gate_bootstrapping_ciphertext(bk->params);
            bootsCOPY(a_copy, prod[idx], bk);
            LweSample* cout = new_gate_bootstrapping_ciphertext(bk->params);
            full_adder(prod[idx], cout, a_copy, row[k], carry, bk);
            delete_gate_bootstrapping_ciphertext(a_copy);
            delete_gate_bootstrapping_ciphertext(carry);
            carry = cout;
        }
        for (int idx = j + nbits; idx < 2*nbits; idx++) {
            LweSample* a_copy = new_gate_bootstrapping_ciphertext(bk->params);
            bootsCOPY(a_copy, prod[idx], bk);
            LweSample* cout = new_gate_bootstrapping_ciphertext(bk->params);
            full_adder(prod[idx], cout, a_copy, ZERO, carry, bk);
            delete_gate_bootstrapping_ciphertext(a_copy);
            delete_gate_bootstrapping_ciphertext(carry);
            carry = cout;
        }
        delete_gate_bootstrapping_ciphertext(carry);
        for (int i = 0; i < nbits; i++) delete_gate_bootstrapping_ciphertext(row[i]);
        free(row);
    }
    delete_gate_bootstrapping_ciphertext(ZERO);
}

/******** Conditional negation: out = (s ? -x : x) = (x XOR s_mask) + s ********/
void fhe_conditional_negate(LweSample** out, LweSample** x, int nbits,
                            const LweSample* s,
                            const TFheGateBootstrappingCloudKeySet* bk,
                            TFheGateBootstrappingParameterSet* params) {
    LweSample* temp_arr = new_gate_bootstrapping_ciphertext_array(nbits, params);
    for (int i = 0; i < nbits; i++) bootsXOR(&temp_arr[i], x[i], s, bk);
    LweSample** temp  = (LweSample**)malloc(nbits * sizeof(LweSample*));
    LweSample** zeros = (LweSample**)malloc(nbits * sizeof(LweSample*));
    LweSample* ZERO = new_gate_bootstrapping_ciphertext(params);
    bootsCONSTANT(ZERO, 0, bk);
    for (int i = 0; i < nbits; i++) { temp[i] = &temp_arr[i]; zeros[i] = ZERO; }
    fhe_add_bits(out, temp, zeros, nbits, bk, /*carry_out=*/NULL, s);
    delete_gate_bootstrapping_ciphertext(ZERO);
    free(zeros); free(temp);
    delete_gate_bootstrapping_ciphertext_array(nbits, temp_arr);
}

/******** Signed 32x32 -> 64 multiply ********/
void fhe_mul_signed32x32_to64(LweSample** prod64,
                              LweSample** a32, LweSample** b32,
                              const TFheGateBootstrappingCloudKeySet* bk,
                              TFheGateBootstrappingParameterSet* params) {
    const int N = 32, W = 64;
    LweSample* sA = new_gate_bootstrapping_ciphertext(params); bootsCOPY(sA, a32[N-1], bk);
    LweSample* sB = new_gate_bootstrapping_ciphertext(params); bootsCOPY(sB, b32[N-1], bk);
    LweSample* sP = new_gate_bootstrapping_ciphertext(params); bootsXOR(sP, sA, sB, bk);

    LweSample* encAabs = new_gate_bootstrapping_ciphertext_array(N, params);
    LweSample* encBabs = new_gate_bootstrapping_ciphertext_array(N, params);
    LweSample* Aabs[32], *Babs[32];
    for (int i = 0; i < N; i++) { Aabs[i] = &encAabs[i]; Babs[i] = &encBabs[i]; }
    fhe_conditional_negate(Aabs, a32, N, sA, bk, params);
    fhe_conditional_negate(Babs, b32, N, sB, bk, params);

    LweSample* encProd = new_gate_bootstrapping_ciphertext_array(W, params);
    LweSample* Prod[64]; for (int i = 0; i < W; i++) Prod[i] = &encProd[i];
    fhe_mult_unsigned(Prod, Aabs, Babs, N, bk);

    fhe_conditional_negate(prod64, Prod, W, sP, bk, params);

    delete_gate_bootstrapping_ciphertext_array(W, encProd);
    delete_gate_bootstrapping_ciphertext_array(N, encAabs);
    delete_gate_bootstrapping_ciphertext_array(N, encBabs);
    delete_gate_bootstrapping_ciphertext(sP);
    delete_gate_bootstrapping_ciphertext(sB);
    delete_gate_bootstrapping_ciphertext(sA);
}

/******** Convolution: 5x5 * 3x3 (valid) -> 3x3 (each cell 64-bit signed) ********/
void fhe_conv5x5_3x3_valid(LweSample** out64, LweSample** img32, LweSample** ker32,
                           const TFheGateBootstrappingCloudKeySet* bk,
                           TFheGateBootstrappingParameterSet* params) {
    const int E = 32, W = 64;
    LweSample* encProd = new_gate_bootstrapping_ciphertext_array(W, params);
    LweSample* encTmp  = new_gate_bootstrapping_ciphertext_array(W, params);
    LweSample* Prod[W]; for (int i = 0; i < W; i++) Prod[i] = &encProd[i];
    LweSample* Tmp [W]; for (int i = 0; i < W; i++) Tmp [i] = &encTmp [i];

    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            int ocell = r*3 + c;
            LweSample* Sum[W];
            for (int i = 0; i < W; i++) {
                Sum[i] = out64[ocell*W + i];
                bootsCONSTANT(Sum[i], 0, bk);
            }
            for (int kr = 0; kr < 3; kr++) {
                for (int kc = 0; kc < 3; kc++) {
                    int img_idx = (r + kr)*5 + (c + kc);
                    int ker_idx = kr*3 + kc;
                    LweSample* A32[32]; LweSample* B32[32];
                    for (int b = 0; b < 32; b++) {
                        A32[b] = img32[img_idx*32 + b];
                        B32[b] = ker32[ker_idx*32 + b];
                    }
                    fhe_mul_signed32x32_to64(Prod, A32, B32, bk, params);
                    fhe_add_bits(Tmp, Sum, Prod, W, bk, /*carry_out=*/NULL, /*carry_in=*/NULL);
                    for (int i = 0; i < W; i++) bootsCOPY(Sum[i], Tmp[i], bk);
                }
            }
        }
    }

    delete_gate_bootstrapping_ciphertext_array(W, encTmp);
    delete_gate_bootstrapping_ciphertext_array(W, encProd);
}

int main() {
    // --- Keys ---
    const int minimum_lambda = 110;
    TFheGateBootstrappingParameterSet* params = new_default_gate_bootstrapping_parameters(minimum_lambda);
    uint32_t seed[] = {314u, 1592u, 657u, 9323u};
    tfhe_random_generator_setSeed(seed, 4);
    TFheGateBootstrappingSecretKeySet* sk = new_random_gate_bootstrapping_secret_keyset(params);
    const TFheGateBootstrappingCloudKeySet* bk = &sk->cloud;

    // --- Hardcoded plaintext image (5x5) & kernel (3x3), signed 32-bit ---
    // Image: ramp 0..24 (row-major)
    int32_t img_host[25] = {
         0,  1,  2,  3,  4,
         5,  6,  7,  8,  9,
        10, 11, 12, 13, 14,
        15, 16, 17, 18, 19,
        20, 21, 22, 23, 24
    };
    // Kernel: "plus" mask
    int32_t ker_host[9] = {
        0, 1, 0,
        1, 1, 1,
        0, 1, 0
    };

    // --- Encrypt constants (via bootsCONSTANT on each bit) ---
    const int E = 32, W = 64;
    LweSample* encImg_flat = new_gate_bootstrapping_ciphertext_array(25*E, params);
    LweSample* encKer_flat = new_gate_bootstrapping_ciphertext_array( 9*E, params);

    for (int idx = 0; idx < 25; idx++) {
        uint32_t v = (uint32_t)img_host[idx];
        for (int b = 0; b < E; b++) {
            bootsCONSTANT(&encImg_flat[idx*E + b], (v >> b) & 1u, bk);
        }
    }
    for (int idx = 0; idx < 9; idx++) {
        uint32_t v = (uint32_t)ker_host[idx];
        for (int b = 0; b < E; b++) {
            bootsCONSTANT(&encKer_flat[idx*E + b], (v >> b) & 1u, bk);
        }
    }

    // Pointer views (array of pointers to bits)
    LweSample** IMG = (LweSample**)malloc(25*E*sizeof(LweSample*));
    LweSample** KER = (LweSample**)malloc( 9*E*sizeof(LweSample*));
    for (int i = 0; i < 25*E; i++) IMG[i] = &encImg_flat[i];
    for (int i = 0; i <  9*E; i++) KER[i] = &encKer_flat[i];

    // Output ciphertexts: 3x3 cells, 64 bits each (flat row-major)
    LweSample* encOut_flat = new_gate_bootstrapping_ciphertext_array(9*W, params);
    LweSample** OUT = (LweSample**)malloc(9*W*sizeof(LweSample*));
    for (int i = 0; i < 9*W; i++) OUT[i] = &encOut_flat[i];

    // --- Homomorphic convolution ---
    fhe_conv5x5_3x3_valid(/*out64=*/OUT, /*img32=*/IMG, /*ker32=*/KER, bk, params);

    // --- Decrypt & print the 3x3 result (int64) + cleartext check ---
    printf("Convolution result (3x3), decrypted as int64:\n");
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            int ocell = r*3 + c;
            uint64_t acc = 0;
            for (int b = 0; b < W; b++) {
                int bit = bootsSymDecrypt(&encOut_flat[ocell*W + b], sk);
                if (bit) acc |= (1ULL << b);
            }
            long long ysigned = (long long)acc;
            printf("%lld%c", ysigned, (c==2?'\n':' '));
        }
    }

    // Cleartext reference (same hardcoded inputs)
    long long ref[9] = {0};
    for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) {
        long long s = 0;
        for (int kr = 0; kr < 3; kr++) for (int kc = 0; kc < 3; kc++) {
            int img_idx = (r+kr)*5 + (c+kc);
            int ker_idx = kr*3 + kc;
            s += (long long)img_host[img_idx] * (long long)ker_host[ker_idx];
        }
        ref[r*3+c] = s;
    }
    fprintf(stderr, "[check] clear:\n");
    for (int i = 0; i < 3; i++) {
        fprintf(stderr, "%lld %lld %lld\n", ref[i*3+0], ref[i*3+1], ref[i*3+2]);
    }

    // Cleanup
    free(OUT); free(KER); free(IMG);
    delete_gate_bootstrapping_ciphertext_array(9*W,  encOut_flat);
    delete_gate_bootstrapping_ciphertext_array(9*E,  encKer_flat);
    delete_gate_bootstrapping_ciphertext_array(25*E, encImg_flat);
    delete_gate_bootstrapping_secret_keyset(sk);
    delete_gate_bootstrapping_parameters(params);
    return 0;
}
