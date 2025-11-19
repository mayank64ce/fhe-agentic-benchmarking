// tfhe_cnn_relu_fc_minimal.c
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
            LweSample* cout = new_gate_bootstrapping_ciphertext(bk->params);
            bootsCOPY(a_copy, prod[idx], bk);
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

/******** ReLU on nbits (two's complement): out = (msb==1 ? 0 : x) ********/
void fhe_relu_bits(LweSample** out, LweSample** x, int nbits,
                   const TFheGateBootstrappingCloudKeySet* bk) {
    LweSample* msb  = new_gate_bootstrapping_ciphertext(bk->params);
    LweSample* mask = new_gate_bootstrapping_ciphertext(bk->params);
    bootsCOPY(msb, x[nbits-1], bk);
    bootsNOT(mask, msb, bk);
    for (int i = 0; i < nbits; i++) bootsAND(out[i], x[i], mask, bk);
    delete_gate_bootstrapping_ciphertext(msb);
    delete_gate_bootstrapping_ciphertext(mask);
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
                    fhe_add_bits(Tmp, Sum, Prod, W, bk, NULL, NULL);
                    for (int i = 0; i < W; i++) bootsCOPY(Sum[i], Tmp[i], bk);
                }
            }
        }
    }

    delete_gate_bootstrapping_ciphertext_array(W, encTmp);
    delete_gate_bootstrapping_ciphertext_array(W, encProd);
}

/******** Matrix–matrix multiply: (M×K) * (K×N) -> (M×N), 32×32→64 ********/
void fhe_matmul_32x32_to_64(LweSample** C,    // (M*N)*64 pointers
                            LweSample** A,    // (M*K)*32 pointers
                            LweSample** B,    // (K*N)*32 pointers
                            int M, int K, int N,
                            const TFheGateBootstrappingCloudKeySet* bk,
                            TFheGateBootstrappingParameterSet* params) {
    const int E = 32, W = 64;
    LweSample* encProd = new_gate_bootstrapping_ciphertext_array(W, params);
    LweSample* encTmp  = new_gate_bootstrapping_ciphertext_array(W, params);
    LweSample* Prod[W]; for (int i = 0; i < W; i++) Prod[i] = &encProd[i];
    LweSample* Tmp [W]; for (int i = 0; i < W; i++) Tmp [i] = &encTmp [i];

    for (int r = 0; r < M; r++) {
        for (int c = 0; c < N; c++) {
            LweSample* Sum[W];
            for (int i = 0; i < W; i++) {
                Sum[i] = C[(r*N + c)*W + i];
                bootsCONSTANT(Sum[i], 0, bk);
            }
            for (int t = 0; t < K; t++) {
                LweSample* A32[E]; LweSample* B32[E];
                for (int b = 0; b < E; b++) {
                    A32[b] = A[(r*K + t)*E + b];
                    B32[b] = B[(t*N + c)*E + b];
                }
                fhe_mul_signed32x32_to64(Prod, A32, B32, bk, params);
                fhe_add_bits(Tmp, Sum, Prod, W, bk, NULL, NULL);
                for (int i = 0; i < W; i++) bootsCOPY(Sum[i], Tmp[i], bk);
            }
        }
    }
    delete_gate_bootstrapping_ciphertext_array(W, encTmp);
    delete_gate_bootstrapping_ciphertext_array(W, encProd);
}

/******** Truncate 64b matrix to 32b (keep LSBs) ********/
void fhe_truncate64_to32_matrix(LweSample** out32, LweSample** in64,
                                int M, int N, const TFheGateBootstrappingCloudKeySet* bk) {
    const int E = 32, W = 64;
    for (int cell = 0; cell < M*N; cell++)
        for (int b = 0; b < E; b++)
            bootsCOPY(out32[cell*E + b], in64[cell*W + b], bk);
}

int main() {
    // --- Keys ---
    const int minimum_lambda = 110;
    TFheGateBootstrappingParameterSet* params = new_default_gate_bootstrapping_parameters(minimum_lambda);
    uint32_t seed[] = {314u, 1592u, 657u, 9323u};
    tfhe_random_generator_setSeed(seed, 4);
    TFheGateBootstrappingSecretKeySet* sk = new_random_gate_bootstrapping_secret_keyset(params);
    const TFheGateBootstrappingCloudKeySet* bk = &sk->cloud;

    // --- Hardcoded inputs/weights ---
    // Image: 5x5 ramp 0..24 (row-major)
    int32_t img_host[25] = {
         0,  1,  2,  3,  4,
         5,  6,  7,  8,  9,
        10, 11, 12, 13, 14,
        15, 16, 17, 18, 19,
        20, 21, 22, 23, 24
    };
    // Kernel: plus mask
    int32_t ker_host[9] = {
        0, 1, 0,
        1, 1, 1,
        0, 1, 0
    };
    // Fully connected weights after flatten (9x1). Here all ones -> sum of features.
    int32_t wfc_host[9] = {1,1,1,1,1,1,1,1,1};

    const int E = 32, W = 64;

    // --- Encrypt image & kernel as ciphertext constants ---
    LweSample* encImg = new_gate_bootstrapping_ciphertext_array(25*E, params);
    LweSample* encKer = new_gate_bootstrapping_ciphertext_array( 9*E, params);
    for (int i = 0; i < 25; i++) {
        uint32_t v = (uint32_t)img_host[i];
        for (int b = 0; b < E; b++) bootsCONSTANT(&encImg[i*E + b], (v>>b)&1u, bk);
    }
    for (int i = 0; i < 9; i++) {
        uint32_t v = (uint32_t)ker_host[i];
        for (int b = 0; b < E; b++) bootsCONSTANT(&encKer[i*E + b], (v>>b)&1u, bk);
    }

    // Pointer views (bit pointers)
    LweSample** IMG = (LweSample**)malloc(25*E*sizeof(LweSample*));
    LweSample** KER = (LweSample**)malloc( 9*E*sizeof(LweSample*));
    for (int i = 0; i < 25*E; i++) IMG[i] = &encImg[i];
    for (int i = 0; i <  9*E; i++) KER[i] = &encKer[i];

    // --- Conv output: 3x3, 64-bit each ---
    LweSample* encFM = new_gate_bootstrapping_ciphertext_array(9*W, params);
    LweSample** FM = (LweSample**)malloc(9*W*sizeof(LweSample*));
    for (int i = 0; i < 9*W; i++) FM[i] = &encFM[i];

    // Conv
    fhe_conv5x5_3x3_valid(FM, IMG, KER, bk, params);

    // ReLU on each 64-bit conv cell
    for (int cell = 0; cell < 9; cell++) {
        LweSample* z[W]; for (int i = 0; i < W; i++) z[i] = FM[cell*W + i];
        fhe_relu_bits(z, z, W, bk);
    }

    // Flatten 3x3 -> length-9 vector and truncate to 32-bit for FC input
    LweSample* encFeat32 = new_gate_bootstrapping_ciphertext_array(9*E, params);
    LweSample** FEAT32 = (LweSample**)malloc(9*E*sizeof(LweSample*));
    for (int i = 0; i < 9*E; i++) FEAT32[i] = &encFeat32[i];
    // Treat FM as a 3x3 matrix of 64b cells, then copy LSB 32 bits
    fhe_truncate64_to32_matrix(FEAT32, FM, /*M=*/3, /*N=*/3, bk);

    // Encrypt FC weights (9x1) as constants
    LweSample* encWfc = new_gate_bootstrapping_ciphertext_array(9*E, params);
    for (int i = 0; i < 9; i++) {
        uint32_t v = (uint32_t)wfc_host[i];
        for (int b = 0; b < E; b++) bootsCONSTANT(&encWfc[i*E + b], (v>>b)&1u, bk);
    }
    LweSample** WFC = (LweSample**)malloc(9*E*sizeof(LweSample*));
    for (int i = 0; i < 9*E; i++) WFC[i] = &encWfc[i];

    // --- Final linear layer: (1x9) * (9x1) -> (1x1) 64-bit ---
    LweSample* encY = new_gate_bootstrapping_ciphertext_array(1*W, params);
    LweSample** Y = (LweSample**)malloc(1*W*sizeof(LweSample*));
    for (int i = 0; i < W; i++) Y[i] = &encY[i];

    // Matmul expects (M×K) and (K×N). We have M=1, K=9, N=1.
    // FEAT32 is (1×9) flattened; WFC is (9×1).
    fhe_matmul_32x32_to_64(/*C=*/Y, /*A=*/FEAT32, /*B=*/WFC,
                           /*M=*/1, /*K=*/9, /*N=*/1, bk, params);

    // --- Decrypt & print final scalar output ---
    uint64_t outu = 0;
    for (int b = 0; b < W; b++) {
        int bit = bootsSymDecrypt(&encY[b], sk);
        if (bit) outu |= (1ULL << b);
    }
    printf("%lld\n", (long long)outu);

    // --- Cleartext reference check ---
    long long conv_ref[9] = {0};
    for (int r=0;r<3;r++) for (int c=0;c<3;c++) {
        long long s=0;
        for (int kr=0;kr<3;kr++) for (int kc=0;kc<3;kc++) {
            int img_idx=(r+kr)*5+(c+kc);
            int ker_idx=kr*3+kc;
            s += (long long)img_host[img_idx]*ker_host[ker_idx];
        }
        if (s<0) s=0; // ReLU
        conv_ref[r*3+c]=s;
    }
    long long yref=0;
    for (int i=0;i<9;i++) yref += conv_ref[i]*(long long)wfc_host[i];
    // fprintf(stderr,"[check] clear output: %lld\n", yref);

    // --- Cleanup ---
    free(Y); free(WFC); free(FEAT32); free(FM); free(KER); free(IMG);
    delete_gate_bootstrapping_ciphertext_array(1*W, encY);
    delete_gate_bootstrapping_ciphertext_array(9*E, encWfc);
    delete_gate_bootstrapping_ciphertext_array(9*E, encFeat32);
    delete_gate_bootstrapping_ciphertext_array(9*W, encFM);
    delete_gate_bootstrapping_ciphertext_array(9*E, encKer);
    delete_gate_bootstrapping_ciphertext_array(25*E, encImg);
    delete_gate_bootstrapping_secret_keyset(sk);
    delete_gate_bootstrapping_parameters(params);
    return 0;
}
