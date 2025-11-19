// fhe_vector_add.c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <tfhe/tfhe.h>
#include <tfhe/tfhe_io.h>

/******** Bit full-adder ********/
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

/******** Encrypted 32-bit add on flat (contiguous) bit arrays ********/
/* sum[i] = a[i] + b[i] (LSB-first), modulo 2^nbits */
void fhe_add_flat(LweSample* sum_base,
                  LweSample* a_base,
                  LweSample* b_base,
                  int nbits,
                  const TFheGateBootstrappingCloudKeySet* bk) {
    LweSample* cin = new_gate_bootstrapping_ciphertext(bk->params);
    bootsCONSTANT(cin, 0, bk);

    for (int i = 0; i < nbits; i++) {
        LweSample* cout = new_gate_bootstrapping_ciphertext(bk->params);
        full_adder(&sum_base[i], cout, &a_base[i], &b_base[i], cin, bk);
        delete_gate_bootstrapping_ciphertext(cin);
        cin = cout;
    }
    delete_gate_bootstrapping_ciphertext(cin);
}

/******** Encrypted vector addition (element-wise) on flat layout ********/
/* Each element uses nbits bits; arrays are laid out as: [elem0_bits][elem1_bits]... */
void fhe_vec_add_flat(LweSample* out_flat,
                      LweSample* a_flat,
                      LweSample* b_flat,
                      int len,
                      int nbits,
                      const TFheGateBootstrappingCloudKeySet* bk) {
    for (int e = 0; e < len; e++) {
        LweSample* sum_e = &out_flat[e * nbits];
        LweSample* a_e   = &a_flat [e * nbits];
        LweSample* b_e   = &b_flat [e * nbits];
        fhe_add_flat(sum_e, a_e, b_e, nbits, bk);
    }
}

int main() {
    const int minimum_lambda = 110;
    TFheGateBootstrappingParameterSet* params =
        new_default_gate_bootstrapping_parameters(minimum_lambda);

    // (Optional) deterministic randomness
    uint32_t seed[] = {314159265u, 358979323u, 846264338u, 327950288u};
    tfhe_random_generator_setSeed(seed, 4);

    // Keys
    TFheGateBootstrappingSecretKeySet* sk =
        new_random_gate_bootstrapping_secret_keyset(params);
    const TFheGateBootstrappingCloudKeySet* bk = &sk->cloud;

    // Read inputs
    int LEN;
    if (scanf("%d", &LEN) != 1 || LEN <= 0) {
        fprintf(stderr, "Usage: LEN then LEN ints for A and LEN ints for B\n");
        return 1;
    }
    int32_t* A = (int32_t*)malloc(LEN * sizeof(int32_t));
    int32_t* B = (int32_t*)malloc(LEN * sizeof(int32_t));
    for (int i = 0; i < LEN; i++) {
        if (scanf("%d", &A[i]) != 1) { fprintf(stderr, "Failed to read A[%d]\n", i); return 1; }
    }
    for (int i = 0; i < LEN; i++) {
        if (scanf("%d", &B[i]) != 1) { fprintf(stderr, "Failed to read B[%d]\n", i); return 1; }
    }

    const int nbits = 32;
    const int total_bits = LEN * nbits;

    // Allocate flat ciphertext arrays for A, B, and Sum
    LweSample* encA = new_gate_bootstrapping_ciphertext_array(total_bits, params);
    LweSample* encB = new_gate_bootstrapping_ciphertext_array(total_bits, params);
    LweSample* encS = new_gate_bootstrapping_ciphertext_array(total_bits, params);

    // Encrypt vectors bitwise (LSB-first per element)
    for (int e = 0; e < LEN; e++) {
        uint32_t Au = (uint32_t)A[e];
        uint32_t Bu = (uint32_t)B[e];
        for (int i = 0; i < nbits; i++) {
            bootsSymEncrypt(&encA[e*nbits + i], (Au >> i) & 1u, sk);
            bootsSymEncrypt(&encB[e*nbits + i], (Bu >> i) & 1u, sk);
            // (encS will be overwritten by fhe_vec_add_flat)
        }
    }

    // Encrypted vector addition: encS = encA + encB (element-wise)
    fhe_vec_add_flat(encS, encA, encB, LEN, nbits, bk);

    // Decrypt and print result
    printf("Result (A + B):\n");
    for (int e = 0; e < LEN; e++) {
        uint32_t sum_u = 0u;
        for (int i = 0; i < nbits; i++) {
            int bit = bootsSymDecrypt(&encS[e*nbits + i], sk);
            if (bit) sum_u |= (1u << i);
        }
        int32_t sum_s = (int32_t)sum_u;  // interpret as signed 32-bit if desired
        printf("%d%c", sum_s, (e + 1 == LEN ? '\n' : ' '));
    }

    // Cleanup
    delete_gate_bootstrapping_ciphertext_array(total_bits, encA);
    delete_gate_bootstrapping_ciphertext_array(total_bits, encB);
    delete_gate_bootstrapping_ciphertext_array(total_bits, encS);
    free(A);
    free(B);
    delete_gate_bootstrapping_secret_keyset(sk);
    delete_gate_bootstrapping_parameters(params);
    return 0;
}
