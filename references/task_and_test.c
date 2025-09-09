#include <tfhe/tfhe.h>
#include <tfhe/tfhe_io.h>
#include <stdio.h>
#include <assert.h>

int main() {
    // Generate a keyset
    const int minimum_lambda = 110;
    TFheGateBootstrappingParameterSet* params = new_default_gate_bootstrapping_parameters(minimum_lambda);

    // right after creating `params`
    FILE* pf = fopen("params.tfhe", "wb");  // "wb" is safe cross-platform
    if (!pf) {
        perror("params.tfhe");
        return 1;
    }
    export_tfheGateBootstrappingParameterSet_toFile(pf, params);
    fclose(pf);

    // Generate a random key
    uint32_t seed[] = { 314, 1592, 657 };
    tfhe_random_generator_setSeed(seed, 3);
    TFheGateBootstrappingSecretKeySet* key = new_random_gate_bootstrapping_secret_keyset(params);

    int32_t plaintext1;
    int32_t plaintext2;

    scanf("%d %d", &plaintext1, &plaintext2);

    // Allocating space for ciphertext
    LweSample* ciphertext1 = new_gate_bootstrapping_ciphertext_array(32, params);
    LweSample* ciphertext2 = new_gate_bootstrapping_ciphertext_array(32, params);
    LweSample* result = new_gate_bootstrapping_ciphertext_array(32, params);

    // printf("Anding %d and %d...\n", plaintext1, plaintext2);

    // Encrypting the plaintexts
    for (int i = 0; i < 32; i++) {
        bootsSymEncrypt(&ciphertext1[i], (plaintext1 >> i) & 1, key);
        bootsSymEncrypt(&ciphertext2[i], (plaintext2 >> i) & 1, key);
    }

    for (int i = 0; i < 32; i++) {
        // result = a and b
        bootsAND(&result[i], &ciphertext1[i], &ciphertext2[i], &key->cloud);
    }

    int final_result = 0;

    // Decrypt and print the result (for verification purposes)
    for (int i = 0; i < 32; i++) {
        int bit = bootsSymDecrypt(&result[i], key);
        final_result |= (bit << i);
    }
    printf("%d\n", final_result);

    assert(final_result == (plaintext1 & plaintext2));

    delete_gate_bootstrapping_ciphertext_array(32, result);
    delete_gate_bootstrapping_ciphertext_array(32, ciphertext2);
    delete_gate_bootstrapping_ciphertext_array(32, ciphertext1);
    delete_gate_bootstrapping_secret_keyset(key);
    delete_gate_bootstrapping_parameters(params);

    return 0;
}
