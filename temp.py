import json

test = '{"name": "compile_execute_code", "arguments": {"code": "#include <tfhe/tfhe.h>\\n#include <tfhe/tfhe_io.h>\\n\\nconst int minimum_lambda = 128;\\n\\nint Main(int a, int b) {\\n    // Key Generation\\n    tfhe_main_context_t *context = tfhe_context_new();\n    \\n    // Encryption of inputs\\n    tfhe_integer_t *A = tfhe_int_new_from整32(a, context);\\n    tfhe_integer_t *B = tfhe_int_new_from inté32(b, context);\\n\\n    // Homomorphic Operations\\n    tfhe_integer_t *result = tfhe_int_and(A, B, context);\\n\\n    // Decryption of the result\\n    int decrypted_result = tfhe_int_get_value(result, context);\\n\\n    // Print the result\\n    return decrypted_result;\\n}\\n" }, "id": 0}'
test = '{"name": "compile_execute_code", "arguments": {"code": "#include <tfhe/tfhe.h>\\n#include <tfhe/tfhe_io.h>\\n\\nconst int minimum_lambda = 128;\\n\\nint Main(int a, int b) {\\n    // Key Generation\\n    tfhe_main_context_t *context = tfhe_context_new();\\n    \\n    // Encryption of inputs\\n    tfhe_integer_t *A = tfhe_int_new_from整32(a, context);\\n    tfhe_integer_t *B = tfhe_int_new_from inté32(b, context);\\n\\n    // Homomorphic Operations\\n    tfhe_integer_t *result = tfhe_int_and(A, B, context);\\n\\n    // Decryption of the result\\n    int decrypted_result = tfhe_int_get_value(result, context);\\n\\n    // Print the result\\n    return decrypted_result;\\n}\\n" }, "id": 0}'

dictionary = {}

dictionary = json.loads(test)
print(dictionary)