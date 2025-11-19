#include <tfhe/tfhe.h>
#include <tfhe/tfhe_io.h>
#include <stdio.h>
#include <assert.h>

// Global security parameter
const int minimum_lambda = 128;

int main() {
    // Read two integers from stdin
    int a, b;
    scanf("%d %d", &a, &b);
    
    // Verify input constraints
    assert(a >= -2147483648 && a <= 2147483647);
    assert(b >= -2147483648 && b <= 2147483647);
    
    // 1. Setup Parameters
    TFheGateBootstrappingParameterSet* params = new_default_gate_bootstrapping_parameters(minimum_lambda);
    
    // 2. Key Generation
    TFheGateBootstrappingSecretKeySet* secret_key = new_random_gate_bootstrapping_secret_keyset(params);
    const TFheGateBootstrappingCloudKeySet* cloud_key = &secret_key->cloud;
    
    // Arrays to store encrypted bits
    LweSample* encrypted_a_bits[32];
    LweSample* encrypted_b_bits[32];
    LweSample* encrypted_result_bits[32];
    
    // Initialize ciphertexts
    for (int i = 0; i < 32; i++) {
        encrypted_a_bits[i] = new_gate_bootstrapping_ciphertext(params);
        encrypted_b_bits[i] = new_gate_bootstrapping_ciphertext(params);
        encrypted_result_bits[i] = new_gate_bootstrapping_ciphertext(params);
    }
    
    // 3. Encryption of inputs
    for (int i = 0; i < 32; i++) {
        int bit_a = (a >> i) & 1;
        int bit_b = (b >> i) & 1;
        bootsSymEncrypt(encrypted_a_bits[i], bit_a, secret_key);
        bootsSymEncrypt(encrypted_b_bits[i], bit_b, secret_key);
    }
    
    // 4. Homomorphic Operations - Bitwise AND
    for (int i = 0; i < 32; i++) {
        bootsAND(encrypted_result_bits[i], encrypted_a_bits[i], encrypted_b_bits[i], cloud_key);
    }
    
    // 5. Decryption of the result
    int result = 0;
    for (int i = 0; i < 32; i++) {
        int decrypted_bit = bootsSymDecrypt(encrypted_result_bits[i], secret_key);
        result |= (decrypted_bit << i);
    }
    
    // 6. Print the result
    printf("%d", result);
    
    // Verify functional correctness properties
    int expected_result = a & b;
    assert(result == expected_result);
    
    // Property 1: Each bit in result is 1 only if both input bits are 1
    for (int i = 0; i < 32; i++) {
        int result_bit = (result >> i) & 1;
        int a_bit = (a >> i) & 1;
        int b_bit = (b >> i) & 1;
        assert(result_bit == 1 ? (a_bit == 1 && b_bit == 1) : true);
        assert((a_bit == 1 && b_bit == 1) ? result_bit == 1 : true);
    }
    
    // Property 2: Idempotence
    assert((a & a) == a);
    assert((b & b) == b);
    
    // Property 3: Commutativity
    assert((a & b) == (b & a));
    
    // Property 4: Zero absorption
    assert((a & 0) == 0);
    assert((b & 0) == 0);
    
    // Property 5: Identity element
    assert((a & -1) == a);
    assert((b & -1) == b);
    
    // Cleanup
    for (int i = 0; i < 32; i++) {
        delete_gate_bootstrapping_ciphertext(encrypted_a_bits[i]);
        delete_gate_bootstrapping_ciphertext(encrypted_b_bits[i]);
        delete_gate_bootstrapping_ciphertext(encrypted_result_bits[i]);
    }
    delete_gate_bootstrapping_secret_keyset(secret_key);
    delete_gate_bootstrapping_parameters(params);
    
    return 0;
}