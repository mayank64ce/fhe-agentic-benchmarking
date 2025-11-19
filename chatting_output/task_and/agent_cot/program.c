#include <stdio.h>
#include <tfhe.h>

// Function to perform bitwise AND on two encrypted integers
void bitwise_and_integers(int bit_length, LweSample** result_bits, LweSample** a_bits, LweSample** b_bits, 
                         const TFheGateBootstrappingCloudKeySet* cloud_key, 
                         const TFheGateBootstrappingParameterSet* params) {
    // AND each bit position
    for (int i = 0; i < bit_length; i++) {
        bootsAND(result_bits[i], a_bits[i], b_bits[i], cloud_key);
    }
}

int main() {
    // Set cryptographic parameters
    TFheGateBootstrappingParameterSet* params = new_default_gate_bootstrapping_parameters(128);
    TFheGateBootstrappingSecretKeySet* secret_key = new_random_gate_bate_bootstrapping_secret_keyset(params);
    const TFheGateBootstrappingCloudKeySet* cloud_key = &secret_key->cloud;
    
    // Define the integers we want to encrypt and AND
    int a = 0b1101;  // 13 in decimal
    int b = 0b1011;  // 11 in decimal
    int expected = a & b;  // 0b1001 = 9 in decimal
    
    const int BIT_LENGTH = 4;  // Using 4 bits for simplicity
    
    // Create arrays of ciphertexts for each bit
    LweSample** a_bits = malloc(BIT_LENGTH * sizeof(LweSample*));
    LweSample** b_bits = malloc(BIT_LENGTH * sizeof(LweSample*));
    LweSample** result_bits = malloc(BIT_LENGTH * sizeof(LweSample*));
    
    for (int i = 0; i < BIT_LENGTH; i++) {
        a_bits[i] = new_gate_bootstrapping_ciphertext(params);
        b_bits[i] = new_gate_bootstrapping_ciphertext(params);
        result_bits[i] = new_gate_bootstrapping_ciphertext(params);
    }
    
    // Encrypt the integers bit by bit
    for (int i = 0; i < BIT_LENGTH; i++) {
        int bit_a = (a >> i) & 1;
        int bit_b = (b >> i) & 1;
        bootsSymEncrypt(a_bits[i], bit_a, secret_key);
        bootsSymEncrypt(b_bits[i], bit_b, secret_key);
    }
    
    // Perform bitwise AND
    bitwise_and_integers(BIT_LENGTH, result_bits, a_bits, b_bits, cloud_key, params);
    
    // Decrypt and reconstruct the result
    int decrypted_result = 0;
    for (int i = 0; i < BIT_LENGTH; i++) {
        int bit = bootsSymDecrypt(result_bits[i], secret_key);
        decrypted_result |= (bit << i);
    }
    
    // Print results
    printf("Input A: %d (binary: %04b)\n", a, a);
    printf("Input B: %d (binary: %04b)\n", b, b);
    printf("Expected A & B: %d (binary: %04b)\n", expected, expected);
    printf("Decrypted result: %d (binary: %04b)\n", decrypted_result, decrypted_result);
    
    // Verify correctness
    if (decrypted_result == expected) {
        printf("✓ Bitwise AND operation successful!\n");
    } else {
        printf("✗ Error in bitwise AND operation\n");
    }
    
    // Cleanup
    for (int i = 0; i < BIT_LENGTH; i++) {
        delete_gate_bootstrapping_ciphertext(a_bits[i]);
        delete_gate_bootstrapping_ciphertext(b_bits[i]);
        delete_gate_bootstrapping_ciphertext(result_bits[i]);
    }
    free(a_bits);
    free(b_bits);
    free(result_bits);
    delete_gate_bootstrapping_secret_keyset(secret_key);
    delete_gate_bootstrapping_parameters(params);
    
    return 0;
}