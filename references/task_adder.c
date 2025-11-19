#include <stdio.h>
#include <stdint.h>
#include <tfhe/tfhe.h>
#include <tfhe/tfhe_io.h>

void full_adder(LweSample* s,
                              LweSample* cout,
                              const LweSample* a,
                              const LweSample* b,
                              const LweSample* cin,
                              const TFheGateBootstrappingCloudKeySet* bk) {
    LweSample* t = new_LweSample(bk->params->in_out_params);
    LweSample* u = new_LweSample(bk->params->in_out_params);
    LweSample* v = new_LweSample(bk->params->in_out_params);

    bootsXOR(t, a, b, bk);      // t = a ^ b
    bootsXOR(s, t, cin, bk);    // s = t ^ cin
    bootsAND(u, a, b, bk);      // u = a & b
    bootsAND(v, cin, t, bk);    // v = cin & t
    bootsOR(cout, u, v, bk);    // cout = u | v

    delete_LweSample(t);
    delete_LweSample(u);
    delete_LweSample(v);
}

/**
 * Encrypted addition: sum = a + b (+ optional carry-in)
 * - a, b, sum are bit arrays (LSB first) of length nbits.
 * - If carry_out != NULL, the final carry is written there.
 */
void fhe_add(LweSample** sum,
             LweSample** a,
             LweSample** b,
             const int nbits,
             const TFheGateBootstrappingCloudKeySet* bk,
             LweSample* carry_out,                // nullable
             const LweSample* carry_in) {         // nullable

    LweSample* cin = new_LweSample(bk->params->in_out_params);
    if (carry_in) bootsCOPY(cin, carry_in, bk);
    else          bootsCONSTANT(cin, 0, bk);

    for (int i = 0; i < nbits; i++) {
        LweSample* cout = new_LweSample(bk->params->in_out_params);
        full_adder(sum[i], cout, a[i], b[i], cin, bk);
        delete_LweSample(cin);
        cin = cout; // propagate carry
    }

    if (carry_out) bootsCOPY(carry_out, cin, bk);
    delete_LweSample(cin);
}


int main() {
    const int minimum_lambda = 110;
    TFheGateBootstrappingParameterSet* params =
        new_default_gate_bootstrapping_parameters(minimum_lambda);

    // Secret + cloud keys
    TFheGateBootstrappingSecretKeySet* sk =
        new_random_gate_bootstrapping_secret_keyset(params);
    const TFheGateBootstrappingCloudKeySet* bk = &sk->cloud;

    const int nbits = 32;
    int32_t A ,B;

    scanf("%d %d", &A, &B);

    // Allocate contiguous ciphertext arrays for bits
    LweSample* encA = new_gate_bootstrapping_ciphertext_array(nbits, params);
    LweSample* encB = new_gate_bootstrapping_ciphertext_array(nbits, params);
    LweSample* encS = new_gate_bootstrapping_ciphertext_array(nbits, params);
    LweSample* encCout = new_LweSample(params->in_out_params);

    // Encrypt plaintext bits (LSB first)
    for (int i = 0; i < nbits; i++) {
        bootsSymEncrypt(&encA[i], (A >> i) & 1, sk);
        bootsSymEncrypt(&encB[i], (B >> i) & 1, sk);
        // pre-allocate sum slots with any value (will be overwritten)
        bootsCONSTANT(&encS[i], 0, bk);
    }

    // Build pointer views expected by fhe_add (array of pointers)
    LweSample* A_bits[32];
    LweSample* B_bits[32];
    LweSample* S_bits[32];
    for (int i = 0; i < nbits; i++) {
        A_bits[i] = &encA[i];
        B_bits[i] = &encB[i];
        S_bits[i] = &encS[i];
    }

    // Homomorphic addition
    fhe_add(S_bits, A_bits, B_bits, nbits, bk, encCout, /*carry_in=*/NULL);

    // Decrypt result
    int32_t sum_plain = 0;
    for (int i = 0; i < nbits; i++) {
        int bit = bootsSymDecrypt(&encS[i], sk);
        if (bit) sum_plain |= (1ULL << i);
    }
    int cout_plain = bootsSymDecrypt(encCout, sk);

    printf("A = %d, B = %d\n", A, B);
    printf("A + B = %d\n", (int)sum_plain);
    // printf("carry_out = %d\n", cout_plain);

    // Expect: 13 and carry_out = 0
    // Clean up
    delete_gate_bootstrapping_ciphertext_array(nbits, encA);
    delete_gate_bootstrapping_ciphertext_array(nbits, encB);
    delete_gate_bootstrapping_ciphertext_array(nbits, encS);
    delete_LweSample(encCout);

    delete_gate_bootstrapping_secret_keyset(sk);
    delete_gate_bootstrapping_parameters(params);
    return 0;
}
