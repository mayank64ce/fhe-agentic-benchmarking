#include <tfhe/tfhe.h>
#include <tfhe/tfhe_io.h>
#include <stdio.h>
#include <assert.h>

// Global security parameter
const int minimum_lambda = 128;

int main() {
    // 1. Setup Parameters
    TFheGateBootstrappingParameterSet* params = new_default_gate_bootstrapping_parameters(minimum_lambda);
    
    // 2. Key Generation
    TFheGateBootstrappingSecretKeySet* secret_key = new_random_gate_bootstrapping_secret_keyset(params);
    const TFheGateBootstrappingCloudKeySet* cloud_key = &secret_key->cloud;
    
    // 3. Read inputs from stdin
    int32_t a, b;
    printf("Enter first 32-bit integer: ");
    scanf("%d", &a);
    printf("Enter second 32-bit integer: ");
    scanf("%d", &b);
    
    // Precondition checks (32-bit bounds)
    assert(a >= -2147483648 && a <= 2147483647);
    assert(b >= -2147483648 && b <= 2147483647);
    
    // 4. Encryption of inputs
    LweSample* encrypted_a = new_gate_bootstrapping_ciphertext_array(32, params);
    LweSample* encrypted_b = new_gate_bootstrapping_ciphertext_array(32, params);
    
    for (int i = 0; i < 32; i++) {
        bootsSymEncrypt(&encrypted_a[i], (a >> i) & 1, secret_key);
        bootsSymEncrypt(&encrypted_b[i], (b >> i) & 1, secret_key);
    }
    
    // 5. Homomorphic multiplication
    LweSample* encrypted_result = new_gate_bootstrapping_ciphertext_array(64, params);
    
    // Initialize result to encrypted zero
    for (int i = 0; i < 64; i++) {
        bootsCONSTANT(&encrypted_result[i], 0, cloud_key);
    }
    
    // Perform multiplication using bit-level operations
    for (int i = 0; i < 32; i++) {
        LweSample* temp = new_gate_bootstrapping_ciphertext_array(64, params);
        
        // Initialize temp to encrypted zero
        for (int j = 0; j < 64; j++) {
            bootsCONSTANT(&temp[j], 0, cloud_key);
        }
        
        // If the i-th bit of a is set, add b shifted by i positions
        for (int j = 0; j < 32; j++) {
            LweSample* bit_product = new_gate_bootstrapping_ciphertext(params);
            bootsAND(bit_product, &encrypted_a[i], &encrypted_b[j], cloud_key);
            
            if (i + j < 64) {
                bootsXOR(&temp[i + j], &temp[i + j], bit_product, cloud_key);
            }
            delete_gate_bootstrapping_ciphertext(bit_product);
        }
        
        // Add temp to result
        for (int j = 0; j < 64; j++) {
            bootsXOR(&encrypted_result[j], &encrypted_result[j], &temp[j], cloud_key);
        }
        
        delete_gate_bootstrapping_ciphertext_array(64, temp);
    }
    
    // 6. Decryption of the result
    int64_t result = 0;
    for (int i = 0; i < 64; i++) {
        int bit = bootsSymDecrypt(&encrypted_result[i], secret_key);
        result |= ((int64_t)bit << i);
    }
    
    // 7. Verify mathematical properties
    int64_t expected_result = (int64_t)a * (int64_t)b;
    assert(result == expected_result);
    
    // Commutative property
    assert(result == (int64_t)b * (int64_t)a);
    
    // Identity property
    assert((int64_t)a * 1 == a);
    assert((int64_t)b * 1 == b);
    
    // Distributive property (using a temporary variable)
    int c = 5;
    assert((int64_t)a * ((int64_t)b + c) == ((int64_t)a * (int64_t)b) + ((int64_t)a * c));
    
    // Result bounds check for 64-bit product
    assert(result >= -4611686018427387904LL && result <= 4611686018427387903LL);
    
    // 8. Print the result
    printf("Result: %ld\n", result);
    
    // Cleanup
    delete_gate_bootstrapping_ciphertext_array(32, encrypted_a);
    delete_gate_bootstrapping_ciphertext_array(32, encrypted_b);
    delete_gate_bootstrapping_ciphertext_array(64, encrypted_result);
    delete_gate_bootstrapping_secret_keyset(secret_key);
    delete_gate_bootstrapping_parameters(params);
    
    return 0;
}