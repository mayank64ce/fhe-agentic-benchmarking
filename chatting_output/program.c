#include <tfhe/tfhe.h>
#include <tfhe/tfhe_io.h>
#include <assert.h>
#include <stdio.h>

int main() {
    // Setup Parameters
    const int minimum_lambda = 128;
    TFheGateBootstrappingParameterSet* params = new_default_gate_bootstrapping_parameters(minimum_lambda);
    
    // Key Generation
    uint32_t seed[] = {314, 1592, 657};  // Example seed for reproducibility
    tfhe_random_generator_setSeed(seed, 3);
    TFheGateBootstrappingSecretKeySet* key = new_random_gate_bootstrapping_secret_keyset(params);
    
    // Read inputs from stdin
    int a_plain, b_plain;
    scanf("%d %d", &a_plain, &b_plain);
    
    // Ensure non-negative inputs as per requires
    assert(a_plain >= 0 && b_plain >= 0);
    
    // Encryption of inputs
    LweSample* a_encrypted = new_gate_bootstrapping_ciphertext_array(32, params);
    LweSample* b_encrypted = new_gate_bootstrapping_ciphertext_array(32, params);
    for (int i = 0; i < 32; i++) {
        bootsSymEncrypt(&a_encrypted[i], (a_plain >> i) & 1, key);
        bootsSymEncrypt(&b_encrypted[i], (b_plain >> i) & 1, key);
    }
    
    // Homomorphic Operations: Bitwise AND
    LweSample* result_encrypted = new_gate_bootstrapping_ciphertext_array(32, params);
    for (int i = 0; i < 32; i++) {
        bootsAND(&result_encrypted[i], &a_encrypted[i], &b_encrypted[i], &key->cloud);
    }
    
    // Decryption of the result
    int result_plain = 0;
    for (int i = 0; i < 32; i++) {
        int bit = bootsSymDecrypt(&result_encrypted[i], key);
        result_plain |= (bit << i);
    }
    
    // Print the result
    printf("%d\n", result_plain);
    
    // Verify functional correctness assertions after decryption
    // Check each bit position
    for (int i = 0; i < 32; i++) {
        int result_bit = (result_plain >> i) & 1;
        int a_bit = (a_plain >> i) & 1;
        int b_bit = (b_plain >> i) & 1;
        assert(result_bit == (a_bit & b_bit));
    }
    
    // Commutativity: a & b == b & a
    assert(result_plain == (b_plain & a_plain));
    
    // Identity: a & a == a
    assert((a_plain & a_plain) == a_plain);
    
    // Zero property: a & 0 == 0
    assert((a_plain & 0) == 0);
    
    // Clean up
    delete_gate_bootstrapping_ciphertext_array(32, a_encrypted);
    delete_gate_bootstrapping_ciphertext_array(32, b_encrypted);
    delete_gate_bootstrapping_ciphertext_array(32, result_encrypted);
    delete_gate_bootstrapping_secret_keyset(key);
    delete_gate_bootstrapping_parameters(params);
    
    return 0;
}