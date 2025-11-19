#include <stdio.h>
#include <tfhe.h>

void bitwise_AND(LweSample* result, const LweSample* a, const LweSample* b, 
                 int bits, const TFheGateBootstrappingCloudKeySet* bk) {
    // Perform bitwise AND by applying bootsAND to each bit
    for (int i = 0; i < bits; i++) {
        bootsAND(&result[i], &a[i], &b[i], bk);
    }
}

int main() {
    // Setup parameters and keys
    TFheGateBootstrappingParameterSet* params = new_default_gate_bootstrapping_parameters(128);
    TFheGateBootstrappingSecretKeySet* secret_key = new_random_gate_bootstrapping_secret_keyset(params);
    const TFheGateBootstrappingCloudKeySet* cloud_key = &secret_key->cloud;
    
    // Number of bits for our integers
    int bits = 4;
    
    // Create ciphertext arrays for inputs and result
    LweSample* encrypted_a = new_gate_bootstrapping_ciphertext_array(bits, params);
    LweSample* encrypted_b = new_gate_bootstrapping_ciphertext_array(bits, params);
    LweSample* encrypted_result = new_gate_bootstrapping_ciphertext_array(bits, params);
    
    // Test case: 5 (0101) AND 3 (0011) = 1 (0001)
    int plain_a = 5;  // binary: 0101
    int plain_b = 3;  // binary: 0011
    
    // Encrypt the inputs bit by bit
    for (int i = 0; i < bits; i++) {
        int bit_a = (plain_a >> i) & 1;
        int bit_b = (plain_b >> i) & 1;
        bootsSymEncrypt(&encrypted_a[i], bit_a, secret_key);
        bootsSymEncrypt(&encrypted_b[i], bit_b, secret_key);
    }
    
    // Perform bitwise AND operation
    bitwise_AND(encrypted_result, encrypted_a, encrypted_b, bits, cloud_key);
    
    // Decrypt and display results
    printf("Input A (%d): ", plain_a);
    for (int i = bits-1; i >= 0; i--) {
        printf("%d", bootsSymDecrypt(&encrypted_a[i], secret_key));
    }
    printf("\n");
    
    printf("Input B (%d): ", plain_b);
    for (int i = bits-1; i >= 0; i--) {
        printf("%d", bootsSymDecrypt(&encrypted_b[i], secret_key));
    }
    printf("\n");
    
    printf("Result: ");
    int result = 0;
    for (int i = bits-1; i >= 0; i--) {
        int bit = bootsSymDecrypt(&encrypted_result[i], secret_key);
        printf("%d", bit);
        result = (result << 1) | bit;
    }
    printf(" (%d)\n", result);
    
    // Verify the result
    printf("Expected: %d AND %d = %d\n", plain_a, plain_b, plain_a & plain_b);
    printf("Actual: %d AND %d = %d\n", plain_a, plain_b, result);
    
    // Cleanup
    delete_gate_bootstrapping_ciphertext_array(bits, encrypted_a);
    delete_gate_bootstrapping_ciphertext_array(bits, encrypted_b);
    delete_gate_bootstrapping_ciphertext_array(bits, encrypted_result);
    delete_gate_bootstrapping_secret_keyset(secret_key);
    delete_gate_bootstrapping_parameters(params);
    
    return 0;
}