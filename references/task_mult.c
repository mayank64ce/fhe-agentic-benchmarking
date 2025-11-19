#include <stdio.h>
#include <stdint.h>
#include <tfhe/tfhe.h>
#include <tfhe/tfhe_io.h>
#include <time.h>

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
void fhe_mult(LweSample** prod,
              LweSample** a,
              LweSample** b,
              const int nbits,
              const TFheGateBootstrappingCloudKeySet* bk) {
    // Make sure prod is initialized. If not, zero it here.
    // (Comment this block out if you pre-zero prod elsewhere.)
    for (int i = 0; i < 2*nbits; i++) {
        bootsCONSTANT(prod[i], 0, bk);
    }

    // Reusable constants
    LweSample* ZERO = new_LweSample(bk->params->in_out_params);
    bootsCONSTANT(ZERO, 0, bk);

    // For each bit of b (the multiplier), conditionally add (a << j) into prod.
    for (int j = 0; j < nbits; j++) {
        // Build masked row: row[i] = a[i] & b[j]
        LweSample** row = (LweSample**)malloc(nbits * sizeof(LweSample*));
        for (int i = 0; i < nbits; i++) {
            row[i] = new_LweSample(bk->params->in_out_params);
            bootsAND(row[i], a[i], b[j], bk);
        }

        // Add the row into prod at offset j using ripple-carry.
        // carry starts at 0
        LweSample* carry = new_LweSample(bk->params->in_out_params);
        bootsCONSTANT(carry, 0, bk);

        // Add across the nbits of the row
        for (int k = 0; k < nbits; k++) {
            int idx = j + k; // destination position in prod
            LweSample* a_copy = new_LweSample(bk->params->in_out_params);
            bootsCOPY(a_copy, prod[idx], bk); // preserve current prod[idx] as input
            LweSample* cout = new_LweSample(bk->params->in_out_params);

            full_adder(/*s=*/prod[idx], /*cout=*/cout,
                       /*a=*/a_copy, /*b=*/row[k], /*cin=*/carry, bk);

            delete_LweSample(a_copy);
            delete_LweSample(carry);
            carry = cout; // propagate
        }

        // Propagate the final carry through higher product bits (j+nbits .. 2*nbits-1)
        for (int idx = j + nbits; idx < 2*nbits; idx++) {
            LweSample* a_copy = new_LweSample(bk->params->in_out_params);
            bootsCOPY(a_copy, prod[idx], bk);
            LweSample* cout = new_LweSample(bk->params->in_out_params);

            full_adder(/*s=*/prod[idx], /*cout=*/cout,
                       /*a=*/a_copy, /*b=*/ZERO, /*cin=*/carry, bk);

            delete_LweSample(a_copy);
            delete_LweSample(carry);
            carry = cout;
        }
        // final carry beyond MSB is discarded (fits in 2*nbits)

        delete_LweSample(carry);

        // Clean masked row
        for (int i = 0; i < nbits; i++) delete_LweSample(row[i]);
        free(row);
    }

    delete_LweSample(ZERO);
}

int main() {
    clock_t start, end;
    double cpu_time_used;

    const int minimum_lambda = 110;
    TFheGateBootstrappingParameterSet* params =
        new_default_gate_bootstrapping_parameters(minimum_lambda);

    // Secret + cloud keys
    TFheGateBootstrappingSecretKeySet* sk =
        new_random_gate_bootstrapping_secret_keyset(params);
    const TFheGateBootstrappingCloudKeySet* bk = &sk->cloud;

    const int nbits = 32;
    int32_t A, B;

    // Read two 32-bit ints
    if (scanf("%d %d", &A, &B) != 2) {
        fprintf(stderr, "Expected two integers on stdin.\n");
        return 1;
    }

    // Allocate contiguous ciphertext arrays for bits
    LweSample* encA = new_gate_bootstrapping_ciphertext_array(nbits, params);
    LweSample* encB = new_gate_bootstrapping_ciphertext_array(nbits, params);
    LweSample* encP = new_gate_bootstrapping_ciphertext_array(2*nbits, params);

    // Encrypt plaintext bits (LSB first).
    // Use unsigned view to extract two's-complement bits safely.
    uint32_t Au = (uint32_t)A;
    uint32_t Bu = (uint32_t)B;
    for (int i = 0; i < nbits; i++) {
        bootsSymEncrypt(&encA[i], (Au >> i) & 1u, sk);
        bootsSymEncrypt(&encB[i], (Bu >> i) & 1u, sk);
    }

    // Build pointer views expected by fhe_mult (arrays of pointers)
    LweSample* A_bits[32];
    LweSample* B_bits[32];
    LweSample* P_bits[64];
    for (int i = 0; i < nbits; i++) {
        A_bits[i] = &encA[i];
        B_bits[i] = &encB[i];
    }
    for (int i = 0; i < 2*nbits; i++) {
        P_bits[i] = &encP[i];
    }

    // Homomorphic multiplication
    start = clock();
    fhe_mult(P_bits, A_bits, B_bits, nbits, bk);
    end = clock();
    cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Time for multiplication: %f seconds\n", cpu_time_used);

    // Decrypt 64-bit product
    uint64_t prod_plain = 0;
    for (int i = 0; i < 2*nbits; i++) {
        int bit = bootsSymDecrypt(&encP[i], sk);
        if (bit) prod_plain |= (1ULL << i);
    }

    // Print (both signed and unsigned interpretations for clarity)
    printf("A = %d, B = %d\n", A, B);
    printf("A * B (decrypted, unsigned view) = %llu\n",
           (unsigned long long)prod_plain);
    // If you want the signed 64-bit interpretation of two's-complement:
    printf("A * B (expected, signed 64-bit) = %lld\n",
           (long long)((int64_t)A * (int64_t)B));

    // Clean up
    delete_gate_bootstrapping_ciphertext_array(nbits, encA);
    delete_gate_bootstrapping_ciphertext_array(nbits, encB);
    delete_gate_bootstrapping_ciphertext_array(2*nbits, encP);

    delete_gate_bootstrapping_secret_keyset(sk);
    delete_gate_bootstrapping_parameters(params);
    return 0;
}
