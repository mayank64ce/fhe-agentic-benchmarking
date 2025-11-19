// fhe_vector_dot_signed.c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <tfhe/tfhe.h>
#include <tfhe/tfhe_io.h>

/***************** Bit full-adder *****************/
static inline void full_adder(LweSample* s,
                              LweSample* cout,
                              const LweSample* a,
                              const LweSample* b,
                              const LweSample* cin,
                              const TFheGateBootstrappingCloudKeySet* bk) {
    LweSample* t = new_gate_bootstrapping_ciphertext(bk->params);
    LweSample* u = new_gate_bootstrapping_ciphertext(bk->params);
    LweSample* v = new_gate_bootstrapping_ciphertext(bk->params);

    bootsXOR(t, a, b, bk);      // t = a ^ b
    bootsXOR(s, t, cin, bk);    // s = t ^ cin
    bootsAND(u, a, b, bk);      // u = a & b
    bootsAND(v, cin, t, bk);    // v = cin & t
    bootsOR(cout, u, v, bk);    // cout = u | v

    delete_gate_bootstrapping_ciphertext(t);
    delete_gate_bootstrapping_ciphertext(u);
    delete_gate_bootstrapping_ciphertext(v);
}

/***************** Ripple add on bit arrays *****************/
// sum = a + b (+ optional carry_in); all arrays LSB-first of length nbits.
void fhe_add_bits(LweSample** sum,
                  LweSample** a,
                  LweSample** b,
                  int nbits,
                  const TFheGateBootstrappingCloudKeySet* bk,
                  LweSample* carry_out,          // nullable
                  const LweSample* carry_in) {   // nullable
    LweSample* cin = new_gate_bootstrapping_ciphertext(bk->params);
    if (carry_in) bootsCOPY(cin, carry_in, bk);
    else          bootsCONSTANT(cin, 0, bk);

    for (int i = 0; i < nbits; i++) {
        LweSample* cout = new_gate_bootstrapping_ciphertext(bk->params);
        full_adder(sum[i], cout, a[i], b[i], cin, bk);
        delete_gate_bootstrapping_ciphertext(cin);
        cin = cout;
    }
    if (carry_out) bootsCOPY(carry_out, cin, bk);
    delete_gate_bootstrapping_ciphertext(cin);
}

/***************** Unsigned schoolbook multiply *****************/
// prod(2n) = a(n) * b(n), LSB-first, unsigned.
void fhe_mult_unsigned(LweSample** prod, LweSample** a, LweSample** b, int nbits,
                       const TFheGateBootstrappingCloudKeySet* bk) {
    // zero product
    for (int i = 0; i < 2*nbits; i++) bootsCONSTANT(prod[i], 0, bk);

    LweSample* ZERO = new_gate_bootstrapping_ciphertext(bk->params);
    bootsCONSTANT(ZERO, 0, bk);

    for (int j = 0; j < nbits; j++) {
        // row[i] = a[i] & b[j]
        LweSample** row = (LweSample**)malloc(nbits * sizeof(LweSample*));
        for (int i = 0; i < nbits; i++) {
            row[i] = new_gate_bootstrapping_ciphertext(bk->params);
            bootsAND(row[i], a[i], b[j], bk);
        }

        // ripple add row << j into prod
        LweSample* carry = new_gate_bootstrapping_ciphertext(bk->params);
        bootsCONSTANT(carry, 0, bk);

        for (int k = 0; k < nbits; k++) {
            int idx = j + k;
            LweSample* a_copy = new_gate_bootstrapping_ciphertext(bk->params);
            bootsCOPY(a_copy, prod[idx], bk);
            LweSample* cout = new_gate_bootstrapping_ciphertext(bk->params);

            full_adder(/*s=*/prod[idx], /*cout=*/cout, a_copy, row[k], carry, bk);

            delete_gate_bootstrapping_ciphertext(a_copy);
            delete_gate_bootstrapping_ciphertext(carry);
            carry = cout;
        }
        for (int idx = j + nbits; idx < 2*nbits; idx++) {
            LweSample* a_copy = new_gate_bootstrapping_ciphertext(bk->params);
            bootsCOPY(a_copy, prod[idx], bk);
            LweSample* cout = new_gate_bootstrapping_ciphertext(bk->params);

            full_adder(/*s=*/prod[idx], /*cout=*/cout, a_copy, ZERO, carry, bk);

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

/***************** Conditional negate: out = (s ? -x : x) *****************/
// Implements two's complement: (x XOR s_mask) + s
void fhe_conditional_negate(LweSample** out, LweSample** x, int nbits,
                            const LweSample* s,
                            const TFheGateBootstrappingCloudKeySet* bk,
                            TFheGateBootstrappingParameterSet* params) {
    // temp = x ^ s
    LweSample* temp_arr = new_gate_bootstrapping_ciphertext_array(nbits, params);
    for (int i = 0; i < nbits; i++) {
        bootsXOR(&temp_arr[i], x[i], s, bk);
    }
    // pointer views + zero vector
    LweSample** temp  = (LweSample**)malloc(nbits * sizeof(LweSample*));
    LweSample** zeros = (LweSample**)malloc(nbits * sizeof(LweSample*));
    LweSample* ZERO = new_gate_bootstrapping_ciphertext(params);
    bootsCONSTANT(ZERO, 0, bk);
    for (int i = 0; i < nbits; i++) { temp[i] = &temp_arr[i]; zeros[i] = ZERO; }

    // out = temp + zeros + carry_in=s
    fhe_add_bits(out, temp, zeros, nbits, bk, /*carry_out=*/NULL, s);

    delete_gate_bootstrapping_ciphertext(ZERO);
    free(zeros);
    free(temp);
    delete_gate_bootstrapping_ciphertext_array(nbits, temp_arr);
}

/***************** Signed 32x32 -> 64 multiply *****************/
// prod64 = (int32)a * (int32)b  (two's-complement), LSB-first
void fhe_mul_signed32x32_to64(LweSample** prod64,
                              LweSample** a32,
                              LweSample** b32,
                              const TFheGateBootstrappingCloudKeySet* bk,
                              TFheGateBootstrappingParameterSet* params) {
    const int N = 32, W = 64;

    // signs
    LweSample* sA = new_gate_bootstrapping_ciphertext(params); bootsCOPY(sA, a32[N-1], bk);
    LweSample* sB = new_gate_bootstrapping_ciphertext(params); bootsCOPY(sB, b32[N-1], bk);
    LweSample* sP = new_gate_bootstrapping_ciphertext(params); bootsXOR(sP, sA, sB, bk);

    // |a|, |b|
    LweSample* encAabs = new_gate_bootstrapping_ciphertext_array(N, params);
    LweSample* encBabs = new_gate_bootstrapping_ciphertext_array(N, params);
    LweSample* Aabs[32]; LweSample* Babs[32];
    for (int i = 0; i < N; i++) { Aabs[i] = &encAabs[i]; Babs[i] = &encBabs[i]; }

    fhe_conditional_negate(Aabs, a32, N, sA, bk, params);
    fhe_conditional_negate(Babs, b32, N, sB, bk, params);

    // unsigned multiply magnitudes → 64 bits
    LweSample* encProd = new_gate_bootstrapping_ciphertext_array(W, params);
    LweSample* Prod[64]; for (int i = 0; i < W; i++) Prod[i] = &encProd[i];
    fhe_mult_unsigned(Prod, Aabs, Babs, N, bk);

    // conditional negate by sP to get signed product
    fhe_conditional_negate(prod64, Prod, W, sP, bk, params);

    // cleanup
    delete_gate_bootstrapping_ciphertext_array(W, encProd);
    delete_gate_bootstrapping_ciphertext_array(N, encAabs);
    delete_gate_bootstrapping_ciphertext_array(N, encBabs);
    delete_gate_bootstrapping_ciphertext(sP);
    delete_gate_bootstrapping_ciphertext(sB);
    delete_gate_bootstrapping_ciphertext(sA);
}

int main() {
    const int minimum_lambda = 20;
    TFheGateBootstrappingParameterSet* params =
        new_default_gate_bootstrapping_parameters(minimum_lambda);

    // (Optional) deterministic RNG
    uint32_t seed[] = {314159265u, 358979323u, 846264338u, 327950288u};
    tfhe_random_generator_setSeed(seed, 4);

    // Keys
    TFheGateBootstrappingSecretKeySet* sk =
        new_random_gate_bootstrapping_secret_keyset(params);
    const TFheGateBootstrappingCloudKeySet* bk = &sk->cloud;

    // Read input
    int LEN;
    if (scanf("%d", &LEN) != 1 || LEN <= 0) {
        fprintf(stderr, "Usage: LEN then LEN ints for A and LEN ints for B\n");
        return 1;
    }
    int32_t* Ahost = (int32_t*)malloc(LEN * sizeof(int32_t));
    int32_t* Bhost = (int32_t*)malloc(LEN * sizeof(int32_t));
    for (int i = 0; i < LEN; i++) {
        if (scanf("%d", &Ahost[i]) != 1) { fprintf(stderr, "Failed to read A[%d]\n", i); return 1; }
    }
    for (int i = 0; i < LEN; i++) {
        if (scanf("%d", &Bhost[i]) != 1) { fprintf(stderr, "Failed to read B[%d]\n", i); return 1; }
    }

    // Encrypt A, B as flat bit arrays (each element = 32 bits, LSB-first)
    const int N = 32, W = 64;
    const int total_bits = LEN * N;
    LweSample* encA = new_gate_bootstrapping_ciphertext_array(total_bits, params);
    LweSample* encB = new_gate_bootstrapping_ciphertext_array(total_bits, params);
    for (int e = 0; e < LEN; e++) {
        uint32_t Au = (uint32_t)Ahost[e];
        uint32_t Bu = (uint32_t)Bhost[e];
        for (int i = 0; i < N; i++) {
            bootsSymEncrypt(&encA[e*N + i], (Au >> i) & 1u, sk);
            bootsSymEncrypt(&encB[e*N + i], (Bu >> i) & 1u, sk);
        }
    }

    // Running 64-bit encrypted sum (initialize to 0)
    LweSample* encSum = new_gate_bootstrapping_ciphertext_array(W, params);
    for (int i = 0; i < W; i++) bootsCONSTANT(&encSum[i], 0, bk);

    // Pointer views for sum and temp
    LweSample* Sum[64]; for (int i = 0; i < W; i++) Sum[i] = &encSum[i];

    // Temporary buffers reused per iteration
    LweSample* encProd = new_gate_bootstrapping_ciphertext_array(W, params);
    LweSample* Prod[64]; for (int i = 0; i < W; i++) Prod[i] = &encProd[i];

    LweSample* encTmp  = new_gate_bootstrapping_ciphertext_array(W, params);
    LweSample* Tmp[64]; for (int i = 0; i < W; i++) Tmp[i] = &encTmp[i];

    // Dot product accumulation: sum += signed_mul(A[e], B[e])
    for (int e = 0; e < LEN; e++) {
        // views into element e
        LweSample* A32[32]; LweSample* B32[32];
        for (int i = 0; i < N; i++) {
            A32[i] = &encA[e*N + i];
            B32[i] = &encB[e*N + i];
        }

        // Prod = signed 32x32 -> 64
        fhe_mul_signed32x32_to64(Prod, A32, B32, bk, params);

        // Tmp = Sum + Prod (64-bit)
        fhe_add_bits(Tmp, Sum, Prod, W, bk, /*carry_out=*/NULL, /*carry_in=*/NULL);

        // Copy Tmp back to Sum
        for (int i = 0; i < W; i++) bootsCOPY(Sum[i], Tmp[i], bk);
    }

    // Decrypt final sum
    uint64_t dot_u = 0;
    for (int i = 0; i < W; i++) {
        int bit = bootsSymDecrypt(&encSum[i], sk);
        if (bit) dot_u |= (1ULL << i);
    }
    long long expected = 0;
    for (int e = 0; e < LEN; e++) expected += (long long)Ahost[e] * (long long)Bhost[e];

    printf("Encrypted dot product (unsigned view) = %llu\n", (unsigned long long)dot_u);
    printf("Expected (signed int64)               = %lld\n", expected);

    // Cleanup
    delete_gate_bootstrapping_ciphertext_array(W, encTmp);
    delete_gate_bootstrapping_ciphertext_array(W, encProd);
    delete_gate_bootstrapping_ciphertext_array(W, encSum);
    delete_gate_bootstrapping_ciphertext_array(total_bits, encA);
    delete_gate_bootstrapping_ciphertext_array(total_bits, encB);
    free(Ahost); free(Bhost);
    delete_gate_bootstrapping_secret_keyset(sk);
    delete_gate_bootstrapping_parameters(params);
    return 0;
}
