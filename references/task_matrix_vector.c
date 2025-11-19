// fhe_matvec_signed.c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <tfhe/tfhe.h>
#include <tfhe/tfhe_io.h>
#include <time.h>

/******** Bit full-adder ********/
static inline void full_adder(LweSample* s,
                              LweSample* cout,
                              const LweSample* a,
                              const LweSample* b,
                              const LweSample* cin,
                              const TFheGateBootstrappingCloudKeySet* bk) {
    LweSample* t = new_gate_bootstrapping_ciphertext(bk->params);
    LweSample* u = new_gate_bootstrapping_ciphertext(bk->params);
    LweSample* v = new_gate_bootstrapping_ciphertext(bk->params);

    bootsXOR(t, a, b, bk);      // t = a ^ b
    bootsXOR(s, t, cin, bk);    // s = t ^ cin
    bootsAND(u, a, b, bk);      // u = a & b
    bootsAND(v, cin, t, bk);    // v = cin & t
    bootsOR(cout, u, v, bk);    // cout = u | v

    delete_gate_bootstrapping_ciphertext(t);
    delete_gate_bootstrapping_ciphertext(u);
    delete_gate_bootstrapping_ciphertext(v);
}

/******** Ripple add on bit arrays: sum = a + b (+carry_in) ********/
// All arrays are LSB-first, length nbits.
void fhe_add_bits(LweSample** sum,
                  LweSample** a,
                  LweSample** b,
                  int nbits,
                  const TFheGateBootstrappingCloudKeySet* bk,
                  LweSample* carry_out,            // nullable
                  const LweSample* carry_in) {     // nullable
    LweSample* cin = new_gate_bootstrapping_ciphertext(bk->params);
    if (carry_in) bootsCOPY(cin, carry_in, bk);
    else          bootsCONSTANT(cin, 0, bk);

    for (int i = 0; i < nbits; i++) {
        LweSample* cout = new_gate_bootstrapping_ciphertext(bk->params);
        full_adder(sum[i], cout, a[i], b[i], cin, bk);
        delete_gate_bootstrapping_ciphertext(cin);
        cin = cout;
    }
    if (carry_out) bootsCOPY(carry_out, cin, bk);
    delete_gate_bootstrapping_ciphertext(cin);
}

/******** Unsigned schoolbook multiply: prod(2n) = a(n) * b(n) ********/
void fhe_mult_unsigned(LweSample** prod, LweSample** a, LweSample** b, int nbits,
                       const TFheGateBootstrappingCloudKeySet* bk) {
    for (int i = 0; i < 2*nbits; i++) bootsCONSTANT(prod[i], 0, bk);

    LweSample* ZERO = new_gate_bootstrapping_ciphertext(bk->params);
    bootsCONSTANT(ZERO, 0, bk);

    for (int j = 0; j < nbits; j++) {
        // row[i] = a[i] & b[j]
        LweSample** row = (LweSample**)malloc(nbits * sizeof(LweSample*));
        for (int i = 0; i < nbits; i++) {
            row[i] = new_gate_bootstrapping_ciphertext(bk->params);
            bootsAND(row[i], a[i], b[j], bk);
        }

        // ripple add row << j into prod
        LweSample* carry = new_gate_bootstrapping_ciphertext(bk->params);
        bootsCONSTANT(carry, 0, bk);

        for (int k = 0; k < nbits; k++) {
            int idx = j + k;
            LweSample* a_copy = new_gate_bootstrapping_ciphertext(bk->params);
            bootsCOPY(a_copy, prod[idx], bk);
            LweSample* cout = new_gate_bootstrapping_ciphertext(bk->params);
            full_adder(prod[idx], cout, a_copy, row[k], carry, bk);
            delete_gate_bootstrapping_ciphertext(a_copy);
            delete_gate_bootstrapping_ciphertext(carry);
            carry = cout;
        }
        for (int idx = j + nbits; idx < 2*nbits; idx++) {
            LweSample* a_copy = new_gate_bootstrapping_ciphertext(bk->params);
            bootsCOPY(a_copy, prod[idx], bk);
            LweSample* cout = new_gate_bootstrapping_ciphertext(bk->params);
            full_adder(prod[idx], cout, a_copy, ZERO, carry, bk);
            delete_gate_bootstrapping_ciphertext(a_copy);
            delete_gate_bootstrapping_ciphertext(carry);
            carry = cout;
        }
        delete_gate_bootstrapping_ciphertext(carry);

        for (int i = 0; i < nbits; i++) delete_gate_bootstrapping_ciphertext(row[i]);
        free(row);
    }

    delete_gate_bootstrapping_ciphertext(ZERO);
}

/******** Conditional negation: out = (s ? -x : x) ********/
/* Implements two's complement: (x XOR s_mask) + s */
void fhe_conditional_negate(LweSample** out, LweSample** x, int nbits,
                            const LweSample* s,
                            const TFheGateBootstrappingCloudKeySet* bk,
                            TFheGateBootstrappingParameterSet* params) {
    // temp = x ^ s
    LweSample* temp_arr = new_gate_bootstrapping_ciphertext_array(nbits, params);
    for (int i = 0; i < nbits; i++) {
        bootsXOR(&temp_arr[i], x[i], s, bk);
    }
    // pointer views + zero vector
    LweSample** temp  = (LweSample**)malloc(nbits * sizeof(LweSample*));
    LweSample** zeros = (LweSample**)malloc(nbits * sizeof(LweSample*));
    LweSample* ZERO = new_gate_bootstrapping_ciphertext(params);
    bootsCONSTANT(ZERO, 0, bk);
    for (int i = 0; i < nbits; i++) { temp[i] = &temp_arr[i]; zeros[i] = ZERO; }

    // out = temp + zeros + carry_in=s
    fhe_add_bits(out, temp, zeros, nbits, bk, /*carry_out=*/NULL, s);

    delete_gate_bootstrapping_ciphertext(ZERO);
    free(zeros);
    free(temp);
    delete_gate_bootstrapping_ciphertext_array(nbits, temp_arr);
}

/******** Signed 32x32 -> 64 multiply ********/
void fhe_mul_signed32x32_to64(LweSample** prod64,
                              LweSample** a32,
                              LweSample** b32,
                              const TFheGateBootstrappingCloudKeySet* bk,
                              TFheGateBootstrappingParameterSet* params) {
    const int N = 32, W = 64;

    // signs
    LweSample* sA = new_gate_bootstrapping_ciphertext(params); bootsCOPY(sA, a32[N-1], bk);
    LweSample* sB = new_gate_bootstrapping_ciphertext(params); bootsCOPY(sB, b32[N-1], bk);
    LweSample* sP = new_gate_bootstrapping_ciphertext(params); bootsXOR(sP, sA, sB, bk);

    // |a|, |b|
    LweSample* encAabs = new_gate_bootstrapping_ciphertext_array(N, params);
    LweSample* encBabs = new_gate_bootstrapping_ciphertext_array(N, params);
    LweSample* Aabs[32]; LweSample* Babs[32];
    for (int i = 0; i < N; i++) { Aabs[i] = &encAabs[i]; Babs[i] = &encBabs[i]; }

    fhe_conditional_negate(Aabs, a32, N, sA, bk, params);
    fhe_conditional_negate(Babs, b32, N, sB, bk, params);

    // unsigned multiply magnitudes → 64 bits
    LweSample* encProd = new_gate_bootstrapping_ciphertext_array(2*N, params);
    LweSample* Prod[64]; for (int i = 0; i < 2*N; i++) Prod[i] = &encProd[i];
    fhe_mult_unsigned(Prod, Aabs, Babs, N, bk);

    // conditional negate by sP to get signed product
    fhe_conditional_negate(prod64, Prod, 2*N, sP, bk, params);

    delete_gate_bootstrapping_ciphertext_array(2*N, encProd);
    delete_gate_bootstrapping_ciphertext_array(N, encAabs);
    delete_gate_bootstrapping_ciphertext_array(N, encBabs);
    delete_gate_bootstrapping_ciphertext(sP);
    delete_gate_bootstrapping_ciphertext(sB);
    delete_gate_bootstrapping_ciphertext(sA);
}

int main() {
    clock_t start, end;
    double cpu_time_used;

    const int minimum_lambda = 110;
    TFheGateBootstrappingParameterSet* params =
        new_default_gate_bootstrapping_parameters(minimum_lambda);

    // (Optional) deterministic RNG
    uint32_t seed[] = {314159265u, 358979323u, 846264338u, 327950288u};
    tfhe_random_generator_setSeed(seed, 4);

    // Keys
    TFheGateBootstrappingSecretKeySet* sk =
        new_random_gate_bootstrapping_secret_keyset(params);
    const TFheGateBootstrappingCloudKeySet* bk = &sk->cloud;

    // Read matrix (M x N) and vector (N)
    int M, N;
    if (scanf("%d %d", &M, &N) != 2 || M <= 0 || N <= 0) {
        fprintf(stderr, "Usage: M N then M*N ints (row-major A) then N ints (x)\n");
        return 1;
    }
    int64_t MN = (int64_t)M * (int64_t)N;
    int32_t* Ahost = (int32_t*)malloc((size_t)MN * sizeof(int32_t));
    int32_t* Xhost = (int32_t*)malloc((size_t)N * sizeof(int32_t));
    for (int64_t i = 0; i < MN; i++) {
        if (scanf("%d", &Ahost[i]) != 1) { fprintf(stderr, "Failed to read A[%lld]\n", (long long)i); return 1; }
    }
    for (int i = 0; i < N; i++) {
        if (scanf("%d", &Xhost[i]) != 1) { fprintf(stderr, "Failed to read x[%d]\n", i); return 1; }
    }

    // Encrypt A (flat row-major), X, outputs Y (M rows of 64-bit)
    const int E = 32;          // bits per input element
    const int W = 64;          // bits per output element
    const int A_bits = (int)MN * E;
    const int X_bits = N * E;
    const int Y_bits = M * W;

    LweSample* encA = new_gate_bootstrapping_ciphertext_array(A_bits, params);
    LweSample* encX = new_gate_bootstrapping_ciphertext_array(X_bits, params);
    LweSample* encY = new_gate_bootstrapping_ciphertext_array(Y_bits, params);

    // Encrypt A
    for (int r = 0; r < M; r++) {
        for (int c = 0; c < N; c++) {
            int64_t idx = (int64_t)r * N + c;
            uint32_t val = (uint32_t)Ahost[idx];
            for (int b = 0; b < E; b++) {
                bootsSymEncrypt(&encA[idx*E + b], (val >> b) & 1u, sk);
            }
        }
    }
    // Encrypt X
    for (int c = 0; c < N; c++) {
        uint32_t val = (uint32_t)Xhost[c];
        for (int b = 0; b < E; b++) {
            bootsSymEncrypt(&encX[c*E + b], (val >> b) & 1u, sk);
        }
    }

    // Temporary 64-bit buffers reused in the loops
    LweSample* encProd = new_gate_bootstrapping_ciphertext_array(W, params);
    LweSample* encTmp  = new_gate_bootstrapping_ciphertext_array(W, params);
    LweSample* Prod[W]; for (int i = 0; i < W; i++) Prod[i] = &encProd[i];
    LweSample* Tmp [W]; for (int i = 0; i < W; i++) Tmp [i] = &encTmp [i];

    // For each row: y[r] = sum_{c} A[r,c] * X[c]   (all encrypted)
    start = clock();
    for (int r = 0; r < M; r++) {
        // Sum64 (encrypted) starts at 0
        LweSample* SumRow64[W];
        for (int i = 0; i < W; i++) {
            SumRow64[i] = &encY[r*W + i];
            bootsCONSTANT(SumRow64[i], 0, bk);
        }

        for (int c = 0; c < N; c++) {
            // Views of A[r,c] and X[c] (each 32 bits)
            LweSample* A32[E];
            LweSample* X32[E];
            int64_t a_base = ((int64_t)r * N + c) * E;
            int     x_base = c * E;
            for (int b = 0; b < E; b++) {
                A32[b] = &encA[a_base + b];
                X32[b] = &encX[x_base + b];
            }

            // Prod = signed 32x32 -> 64
            fhe_mul_signed32x32_to64(Prod, A32, X32, bk, params);

            // Tmp = SumRow64 + Prod
            fhe_add_bits(Tmp, SumRow64, Prod, W, bk, /*carry_out=*/NULL, /*carry_in=*/NULL);

            // SumRow64 = Tmp   (copy back)
            for (int i = 0; i < W; i++) bootsCOPY(SumRow64[i], Tmp[i], bk);
        }
        // Next row…
    }
    end = clock();
    cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Time for multiplication: %f seconds\n", cpu_time_used);
    
    // Decrypt and print y (each row 64-bit signed)
    printf("y = A * x (encrypted, decrypted as int64 per row):\n");
    for (int r = 0; r < M; r++) {
        uint64_t yu = 0;
        for (int i = 0; i < W; i++) {
            int bit = bootsSymDecrypt(&encY[r*W + i], sk);
            if (bit) yu |= (1ULL << i);
        }
        long long ysigned = (long long)yu; // interpret as signed 64-bit
        printf("%lld%c", ysigned, (r+1==M?'\n':' '));
    }

    // (Optional) cleartext check
    long long* y_ref = (long long*)calloc(M, sizeof(long long));
    for (int r = 0; r < M; r++) {
        long long acc = 0;
        for (int c = 0; c < N; c++) {
            acc += (long long)Ahost[(long long)r*N + c] * (long long)Xhost[c];
        }
        y_ref[r] = acc;
    }
    fprintf(stderr, "[check] cleartext y:\n");
    for (int r = 0; r < M; r++) {
        fprintf(stderr, "%lld%c", y_ref[r], (r+1==M?'\n':' '));
    }

    // Cleanup
    free(y_ref);
    delete_gate_bootstrapping_ciphertext_array(W, encTmp);
    delete_gate_bootstrapping_ciphertext_array(W, encProd);
    delete_gate_bootstrapping_ciphertext_array(Y_bits, encY);
    delete_gate_bootstrapping_ciphertext_array(X_bits, encX);
    delete_gate_bootstrapping_ciphertext_array(A_bits, encA);
    free(Ahost);
    free(Xhost);
    delete_gate_bootstrapping_secret_keyset(sk);
    delete_gate_bootstrapping_parameters(params);
    return 0;
}
