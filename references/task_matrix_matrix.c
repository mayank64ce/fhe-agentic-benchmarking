// tfhe_matmul_demo.c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <tfhe/tfhe.h>
#include <tfhe/tfhe_io.h>

/******** 1-bit full adder ********/
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

/******** Ripple add on bit arrays (LSB-first) ********/
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
            LweSample* cout   = new_gate_bootstrapping_ciphertext(bk->params);
            bootsCOPY(a_copy, prod[idx], bk);
            full_adder(prod[idx], cout, a_copy, row[k], carry, bk);
            delete_gate_bootstrapping_ciphertext(a_copy);
            delete_gate_bootstrapping_ciphertext(carry);
            carry = cout;
        }
        for (int idx = j + nbits; idx < 2*nbits; idx++) {
            LweSample* a_copy = new_gate_bootstrapping_ciphertext(bk->params);
            LweSample* cout   = new_gate_bootstrapping_ciphertext(bk->params);
            bootsCOPY(a_copy, prod[idx], bk);
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

/******** Conditional negation: out = (s ? -x : x) ********/
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

/******** Matrix–matrix multiply: (M×K)*(K×N) -> (M×N), 32×32→64 ********/
void fhe_matmul_32x32_to_64(LweSample** C,    // (M*N)*64 pointers (row-major cells)
                            LweSample** A,    // (M*K)*32 pointers (row-major elems)
                            LweSample** B,    // (K*N)*32 pointers (row-major elems)
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
            for (int i = 0; i < W; i++) {           // zero-init C[r,c]
                Sum[i] = C[(r*N + c)*W + i];
                bootsCONSTANT(Sum[i], 0, bk);
            }
            for (int t = 0; t < K; t++) {           // dot over K
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

int main() {
    // --- Keys ---
    const int lambda = 110;
    TFheGateBootstrappingParameterSet* params = new_default_gate_bootstrapping_parameters(lambda);
    uint32_t seed[] = {314u, 1592u, 657u, 9323u};
    tfhe_random_generator_setSeed(seed, 4);
    TFheGateBootstrappingSecretKeySet* sk = new_random_gate_bootstrapping_secret_keyset(params);
    const TFheGateBootstrappingCloudKeySet* bk = &sk->cloud;

    // --- Example: A(2x3) * B(3x2) -> C(2x2) ---
    // A = [[ 1, -2,  3],
    //      [ 4,  0, -1]]
    // B = [[ 5,  2],
    //      [-3,  1],
    //      [ 0,  4]]
    // Expected C (clear): [[11, 12],[20, 4]]
    const int M=2, K=3, N=2;
    const int E=32, W=64;

    int32_t A_host[M*K] = { 1, -2,  3,
                            4,  0, -1 };
    int32_t B_host[K*N] = { 5,  2,
                           -3,  1,
                            0,  4 };

    // Encrypt constants as ciphertexts (public): use bootsCONSTANT per bit
    LweSample* encA = new_gate_bootstrapping_ciphertext_array(M*K*E, params);
    LweSample* encB = new_gate_bootstrapping_ciphertext_array(K*N*E, params);
    for (int i=0;i<M*K;i++){ uint32_t v=(uint32_t)A_host[i]; for(int b=0;b<E;b++) bootsCONSTANT(&encA[i*E+b], (v>>b)&1u, bk); }
    for (int i=0;i<K*N;i++){ uint32_t v=(uint32_t)B_host[i]; for(int b=0;b<E;b++) bootsCONSTANT(&encB[i*E+b], (v>>b)&1u, bk); }

    // Pointer views (arrays of bit pointers)
    LweSample** A = (LweSample**)malloc(M*K*E*sizeof(LweSample*));
    LweSample** B = (LweSample**)malloc(K*N*E*sizeof(LweSample*));
    for (int i=0;i<M*K*E;i++) A[i]=&encA[i];
    for (int i=0;i<K*N*E;i++) B[i]=&encB[i];

    // Output ciphertexts: C(2x2) in 64 bits
    LweSample* encC = new_gate_bootstrapping_ciphertext_array(M*N*W, params);
    LweSample** C = (LweSample**)malloc(M*N*W*sizeof(LweSample*));
    for (int i=0;i<M*N*W;i++) C[i]=&encC[i];

    // --- Homomorphic matmul ---
    fhe_matmul_32x32_to_64(C, A, B, M, K, N, bk, params);

    // --- Decrypt & print result (signed int64) ---
    printf("C = A * B (decrypted int64):\n");
    for (int r=0;r<M;r++){
        for (int c=0;c<N;c++){
            uint64_t acc=0;
            for (int b=0;b<W;b++){
                int bit = bootsSymDecrypt(&encC[(r*N + c)*W + b], sk);
                if (bit) acc |= (1ULL<<b);
            }
            printf("%lld%s", (long long)acc, (c+1==N? "\n":" "));
        }
    }

    // --- Cleartext check ---
    long long C_ref[M*N]={0};
    for (int r=0;r<M;r++) for (int c=0;c<N;c++){
        long long s=0;
        for (int t=0;t<K;t++) s += (long long)A_host[r*K+t]* (long long)B_host[t*N+c];
        C_ref[r*N+c]=s;
    }
    printf("[check]\n%lld %lld\n%lld %lld\n",
            C_ref[0], C_ref[1], C_ref[2], C_ref[3]);

    // Cleanup
    free(C); free(B); free(A);
    delete_gate_bootstrapping_ciphertext_array(M*N*W, encC);
    delete_gate_bootstrapping_ciphertext_array(K*N*E, encB);
    delete_gate_bootstrapping_ciphertext_array(M*K*E, encA);
    delete_gate_bootstrapping_secret_keyset(sk);
    delete_gate_bootstrapping_parameters(params);
    return 0;
}
