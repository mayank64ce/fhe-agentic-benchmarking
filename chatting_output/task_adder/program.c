#include <tfhe/tfhe.h>
#include <tfhe/tfhe_io.h>
#include <assert.h>

// Global security parameter
const int minimum_lambda = 128;

// Full adder helper function using TFHE operations
void FullAdder(LweSample* sum, LweSample* cout, const LweSample* a, const LweSample* b, const LweSample* cin, const TFheGateBootstrappingCloudKeySet* bk) {
    // Temporary variables for intermediate results
    LweSample* temp1 = new_gate_bootstrapping_ciphertext(bk->params);
    LweSample* temp2 = new_gate_bootstrapping_ciphertext(bk->params);
    LweSample* temp3 = new_gate_bootstrapping_ciphertext(bk->params);
    
    // sum = a XOR b XOR cin
    bootsXOR(temp1, a, b, bk);
    bootsXOR(sum, temp1, cin, bk);
    
    // cout = (a AND b) OR (a AND cin) OR (b AND cin)
    bootsAND(temp1, a, b, bk);
    bootsAND(temp2, a, cin, bk);
    bootsAND(temp3, b, cin, bk);
    
    bootsOR(temp1, temp1, temp2, bk);
    bootsOR(cout, temp1, temp3, bk);
    
    // Cleanup
    delete_gate_bootstrapping_ciphertext(temp1);
    delete_gate_bootstrapping_ciphertext(temp2);
    delete_gate_bootstrapping_ciphertext(temp3);
}

int main() {
    // 1. Setup Parameters
    TFheGateBootstrappingParameterSet* params = new_default_gate_bootstrapping_parameters(minimum_lambda);
    
    // 2. Key Generation
    TFheGateBootstrappingSecretKeySet* secret_key = new_random_gate_bootstrapping_secret_keyset(params);
    const TFheGateBootstrappingCloudKeySet* cloud_key = &secret_key->cloud;
    
    // Test values
    uint32_t a_val = 123456789;
    uint32_t b_val = 987654321;
    uint32_t expected_sum = a_val + b_val;
    
    // 3. Encryption of inputs
    LweSample* a_bits[32];
    LweSample* b_bits[32];
    
    for (int i = 0; i < 32; i++) {
        a_bits[i] = new_gate_bootstrapping_ciphertext(params);
        b_bits[i] = new_gate_bootstrapping_ciphertext(params);
        
        bootsSymEncrypt(a_bits[i], (a_val >> i) & 1, secret_key);
        bootsSymEncrypt(b_bits[i], (b_val >> i) & 1, secret_key);
    }
    
    // 4. Homomorphic Operations
    LweSample* sum_bits[32];
    LweSample* carry = new_gate_bootstrapping_ciphertext(params);
    
    // Initialize carry to 0
    bootsSymEncrypt(carry, 0, secret_key);
    
    for (int i = 0; i < 32; i++) {
        sum_bits[i] = new_gate_bootstrapping_ciphertext(params);
        LweSample* new_carry = new_gate_bootstrapping_ciphertext(params);
        
        FullAdder(sum_bits[i], new_carry, a_bits[i], b_bits[i], carry, cloud_key);
        
        // Update carry for next iteration
        delete_gate_bootstrapping_ciphertext(carry);
        carry = new_carry;
    }
    
    // 5. Decryption of the result
    uint32_t result = 0;
    for (int i = 0; i < 32; i++) {
        int bit = bootsSymDecrypt(sum_bits[i], secret_key);
        result |= (bit << i);
    }
    
    // 6. Verification with assert
    assert(result == expected_sum && "Addition result does not match expected value");
    
    // Cleanup
    delete_gate_bootstrapping_ciphertext(carry);
    for (int i = 0; i < 32; i++) {
        delete_gate_bootstrapping_ciphertext(a_bits[i]);
        delete_gate_bootstrapping_ciphertext(b_bits[i]);
        delete_gate_bootstrapping_ciphertext(sum_bits[i]);
    }
    
    delete_gate_bootstrapping_secret_keyset(secret_key);
    delete_gate_bootstrapping_parameters(params);
    
    return 0;
}