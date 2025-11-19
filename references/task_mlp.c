// tfhe_mlp_3x2_2x2_relu_2x1_minimal.c
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
            for (int i = 0; i < W; i++) { // zero-init output cell
                Sum[i] = C[(r*N + c)*W + i];
                bootsCONSTANT(Sum[i], 0, bk);
            }
            for (int t = 0; t < K; t++) { // dot-product over K
                LweSample* A32[E]; LweSample* B32[E];
                for (int b = 0; b < E; b++) {
                    A32[b] = A[(r*K + t)*E + b];
                    B32[b] = B[(t*N + c)*E + b];
                }
                fhe_mul_signed32x32_to64(Prod, A32, B32, bk, params);
                fhe_add_bits(Tmp, Sum, Prod, W, bk, /*carry_out=*/NULL, /*carry_in=*/NULL);
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

    // --- Hardcoded MLP values (signed 32-bit) ---
    // X (3x2): rows = [1,2], [-3,4], [0,-1]
    int32_t X_host[6]  = { 1,  2,  -3,  4,   0, -1 };
    // W1 (2x2): [[2,-1],[3,1]]
    int32_t W1_host[4] = { 2, -1,   3,  1 };
    // W2 (2x1): [[4],[-2]]
    int32_t W2_host[2] = { 4, -2 };

    const int E = 32, W = 64;

    // --- Encrypt as ciphertexts (public constants -> bootsCONSTANT for speed) ---
    LweSample* encX  = new_gate_bootstrapping_ciphertext_array(6*E,  params);
    LweSample* encW1 = new_gate_bootstrapping_ciphertext_array(4*E,  params);
    LweSample* encW2 = new_gate_bootstrapping_ciphertext_array(2*E,  params);

    for (int i = 0; i < 6; i++) {
        uint32_t v = (uint32_t)X_host[i];
        for (int b = 0; b < E; b++) bootsCONSTANT(&encX [i*E + b],  (v>>b)&1u, bk);
    }
    for (int i = 0; i < 4; i++) {
        uint32_t v = (uint32_t)W1_host[i];
        for (int b = 0; b < E; b++) bootsCONSTANT(&encW1[i*E + b], (v>>b)&1u, bk);
    }
    for (int i = 0; i < 2; i++) {
        uint32_t v = (uint32_t)W2_host[i];
        for (int b = 0; b < E; b++) bootsCONSTANT(&encW2[i*E + b], (v>>b)&1u, bk);
    }

    // Pointer views
    LweSample** X  = (LweSample**)malloc(6*E*sizeof(LweSample*));
    LweSample** W1 = (LweSample**)malloc(4*E*sizeof(LweSample*));
    LweSample** W2 = (LweSample**)malloc(2*E*sizeof(LweSample*));
    for (int i = 0; i < 6*E; i++) X[i]  = &encX[i];
    for (int i = 0; i < 4*E; i++) W1[i] = &encW1[i];
    for (int i = 0; i < 2*E; i++) W2[i] = &encW2[i];

    // --- Layer 1: Z1 = X(3x2) * W1(2x2) -> (3x2) 64-bit ---
    const int M = 3, K = 2, N1 = 2;
    LweSample* encZ1 = new_gate_bootstrapping_ciphertext_array(M*N1*W, params);
    LweSample** Z1 = (LweSample**)malloc(M*N1*W*sizeof(LweSample*));
    for (int i = 0; i < M*N1*W; i++) Z1[i] = &encZ1[i];

    fhe_matmul_32x32_to_64(Z1, X, W1, M, K, N1, bk, params);

    // ReLU per 64-bit cell
    for (int cell = 0; cell < M*N1; cell++) {
        LweSample* z[W]; for (int i = 0; i < W; i++) z[i] = Z1[cell*W + i];
        fhe_relu_bits(z, z, W, bk);
    }

    // Truncate to 32b to feed next layer: H1 (3x2)
    LweSample* encH1 = new_gate_bootstrapping_ciphertext_array(M*N1*E, params);
    LweSample** H1 = (LweSample**)malloc(M*N1*E*sizeof(LweSample*));
    for (int i = 0; i < M*N1*E; i++) H1[i] = &encH1[i];
    fhe_truncate64_to32_matrix(H1, Z1, M, N1, bk);

    // --- Layer 2: Y = H1(3x2) * W2(2x1) -> (3x1) 64-bit ---
    const int N2 = 1;
    LweSample* encY = new_gate_bootstrapping_ciphertext_array(M*N2*W, params);
    LweSample** Y = (LweSample**)malloc(M*N2*W*sizeof(LweSample*));
    for (int i = 0; i < M*N2*W; i++) Y[i] = &encY[i];

    fhe_matmul_32x32_to_64(Y, H1, W2, M, K, N2, bk, params);

    // --- Decrypt & print (signed int64) ---
    // printf("MLP output (3 x 1), decrypted as int64:\n");
    for (int r = 0; r < M; r++) {
        uint64_t acc = 0;
        for (int b = 0; b < W; b++) {
            int bit = bootsSymDecrypt(&encY[(r*N2 + 0)*W + b], sk);
            if (bit) acc |= (1ULL << b);
        }
        printf("%lld\n", (long long)acc);
    }

    // Cleartext check
    long long Href[3*2] = {0};
    for (int r=0;r<3;r++) for(int c=0;c<2;c++){
        long long s=0; for(int t=0;t<2;t++) s += (long long)X_host[r*2+t]*W1_host[t*2+c];
        Href[r*2+c] = (s<0?0:s);
    }
    long long Yref[3] = {0};
    for (int r=0;r<3;r++) {
        long long s=0; for (int t=0;t<2;t++) s += Href[r*2+t]*(long long)W2_host[t];
        Yref[r]=s;
    }
    fprintf(stderr,"[check]\n%lld\n%lld\n%lld\n", Yref[0], Yref[1], Yref[2]);

    // Cleanup
    free(Y); free(H1); free(Z1); free(W2); free(W1); free(X);
    delete_gate_bootstrapping_ciphertext_array(M*N2*W, encY);
    delete_gate_bootstrapping_ciphertext_array(M*N1*E, encH1);
    delete_gate_bootstrapping_ciphertext_array(M*N1*W, encZ1);
    delete_gate_bootstrapping_ciphertext_array(2*E, encW2);
    delete_gate_bootstrapping_ciphertext_array(4*E, encW1);
    delete_gate_bootstrapping_ciphertext_array(6*E, encX);
    delete_gate_bootstrapping_secret_keyset(sk);
    delete_gate_bootstrapping_parameters(params);
    return 0;
}
