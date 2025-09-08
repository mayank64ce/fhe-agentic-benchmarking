#include <tfhe/tfhe.h>
#include <tfhe/tfhe_io.h>
#include <stdio.h>
#include <assert.h>

int32_t relu(int32_t input) {
    // performing relu
    int mask = input >> 31;
    int output = input & ~mask;
    return output;
}

int main() {
    // Generate a keyset
    const int minimum_lambda = 110;
    TFheGateBootstrappingParameterSet* params = new_default_gate_bootstrapping_parameters(minimum_lambda);

    // Generate a random key
    uint32_t seed[] = { 314, 1592, 657 };
    tfhe_random_generator_setSeed(seed, 3);
    TFheGateBootstrappingSecretKeySet* key = new_random_gate_bootstrapping_secret_keyset(params);

    int32_t plaintext1;

    scanf("%d", &plaintext1);

    // Allocating space for ciphertext
    LweSample* ciphertext1 = new_gate_bootstrapping_ciphertext_array(32, params);
    LweSample* result = new_gate_bootstrapping_ciphertext_array(32, params);
    LweSample* mask = new_gate_bootstrapping_ciphertext_array(32, params);

    // Encrypting the plaintexts
    for (int i = 0; i < 32; i++) {
        bootsSymEncrypt(&ciphertext1[i], (plaintext1 >> i) & 1, key);
        bootsSymEncrypt(&mask[i], 0, key);
    }
   
   // constructing the mask
   for (int i=0;i<32;i++){
         bootsCOPY(&mask[i], &ciphertext1[31], &key->cloud); // result = input    
   }

   // inverting the mask

   for (int i=0;i<32;i++){
         bootsNOT(&mask[i], &mask[i], &key->cloud); // result = ~input    
   }

   // result = input & ~mask

   for (int i = 0; i < 32; i++){
    bootsAND(&result[i], &ciphertext1[i], &mask[i], &key->cloud);
   }

    // --------------------------------------------------

    int final_result = 0;

    // Decrypt and print the result (for verification purposes)
    for (int i = 0; i < 32; i++) {
        int bit = bootsSymDecrypt(&result[i], key);
        final_result |= (bit << i);
    }
    printf("%d\n", final_result);

    assert(final_result == relu(plaintext1));

    delete_gate_bootstrapping_ciphertext_array(32, mask);
    delete_gate_bootstrapping_ciphertext_array(32, result);
    delete_gate_bootstrapping_ciphertext_array(32, ciphertext1);
    delete_gate_bootstrapping_secret_keyset(key);
    delete_gate_bootstrapping_parameters(params);

    return 0;
}
