/**
 * @file jwt_util.h
 * @brief Header for JWT (JSON Web Token) generation utility functions
 */

#ifndef jwt_util_h
#define jwt_util_h

#include "mbedtls/base64.h"
#include "mbedtls/sha256.h"
#include "mbedtls/entropy.h"
#include "mbedtls/pk.h"
#include "mbedtls/ctr_drbg.h"

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "firebase_config.h"

// JSON templates for the JWT payload and header
static const unsigned char* payload_format = (unsigned char*)"{"
    "\"iss\":\"%s\","
    "\"sub\":\"%s\","
    "\"aud\":\"https://firestore.googleapis.com/\","
    "\"iat\":\"%ld\","
    "\"exp\":\"%ld\""
    "}";

static const unsigned char* header_format = (unsigned char*)"{"
    "\"alg\":\"RS256\","
    "\"typ\":\"JWT\","
    "\"kid\":\"%s\""
    "}";

// Buffers used to store intermediate and final results
static unsigned char    payload_base64[512];
static size_t           payload_base64_len;

static unsigned char    header_base64[512];
static size_t           header_base64_len;

static unsigned char    concatenated[1024];

static unsigned char    signature[MBEDTLS_PK_SIGNATURE_MAX_SIZE];
static size_t           signature_len;

static unsigned char    signature_base64[1024];
static size_t           signature_base64_len;

unsigned char           jwt[2048];


// Encodes data into Base64 URL-safe format by replacing + with -, / with _, and removing padding (=)
static void base64_url_encode(unsigned char *dst, size_t dlen, size_t *olen, const unsigned char *src, size_t slen) {
    unsigned char temp[dlen];
    size_t temp_len;
    mbedtls_base64_encode(temp, dlen, &temp_len, src, slen);
    size_t dst_i = 0;
    for (size_t i = 0; i < temp_len; i++) {
        if ((temp[i] == (unsigned char)'=')) continue;
        if (temp[i] == (unsigned char)'+') dst[dst_i++] = (unsigned char)'-';
        else if (temp[i] == (unsigned char)'/') dst[dst_i++] = (unsigned char)'_';
        else dst[dst_i++] = temp[i];
    }

    dst[dst_i + 1] = (unsigned char)'\0';
    *olen = dst_i + 1;
}

// Main function that generates the JWT
// 1. Encodes the header and payload into Base64 URL-safe format
// 2. Concatenates the header and payload
// 3. Signs the concatenated string using RS256 (SHA-256 with RSA) algorithm
// 4. Encodes the signature into Base64 URL-safe format
// 5. Combines the header, payload and signature into the final JWT
static void generate_jwt(long int time_since_epoch) {
    // Create header JSON with key ID from Firebase config
    unsigned char header[256];
    snprintf((char*)header, 256, (char*)header_format, FIREBASE_SERVICE_ACCOUNT_PRIVATE_KEY_ID); // Create the header JSON

    // Create payload JSON
    unsigned char payload[512];
    snprintf((char*)payload, 512, (char*)payload_format,
        FIREBASE_SERVICE_ACCOUNT_EMAIL,
        FIREBASE_SERVICE_ACCOUNT_EMAIL,
        time_since_epoch,
        time_since_epoch + 3600 // 1 Hour expiration time
    );
    
    // Encode header and payload into Base64 URL-safe format
    base64_url_encode(payload_base64, 512, &payload_base64_len, payload, strlen((char*)payload));   // Base64 URL encode for payload
    base64_url_encode(header_base64, 512, &header_base64_len, header, strlen((char*)header));       // Base64 URL encode for header

    // Concatenate header and payload
    snprintf((char*)concatenated, 1024, "%s.%s", header_base64, payload_base64);                    // Concatenate header + payload to format of <header>.<payload>

    // Initialize cryptographic contexts for signing
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

    mbedtls_entropy_context entropy;
    mbedtls_entropy_init(&entropy);

    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ctr_drbg_init(&ctr_drbg);

    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, MBEDTLS_CTR_DRBG_MAX_SEED_INPUT);

    // Initialize random number generator
    unsigned char personalization[] = "firebase_jwt_generator";
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, personalization, sizeof(personalization));

    // Parse the private key NEW
    int ret = mbedtls_pk_parse_key(&pk, (const unsigned char *)FIREBASE_SERVICE_ACCOUNT_PRIVATE_KEY,
                             (size_t)strlen(FIREBASE_SERVICE_ACCOUNT_PRIVATE_KEY) + 1, NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg);
    // Parse the private key OLD
    //mbedtls_pk_parse_key(&pk, private_key, (strlen((char*)private_key) + 1), NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg);

    if (ret != 0) {
        printf("Private key parsing failed with error: %d\n", ret);
        mbedtls_pk_free(&pk);
        mbedtls_entropy_free(&entropy);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        return;
    }

    // Hash the concatenated header and payload using SHA-256
    unsigned char hash[32];
    mbedtls_sha256(concatenated, strlen((char*)concatenated), hash, 0);

    // Sign the hash using RS256
    ret = mbedtls_pk_sign(&pk, MBEDTLS_MD_SHA256, hash, sizeof(hash), signature, MBEDTLS_PK_SIGNATURE_MAX_SIZE, &signature_len, mbedtls_ctr_drbg_random, &ctr_drbg);

    if (ret != 0) {
        printf("Signing failed with error: %d\n", ret);
        mbedtls_pk_free(&pk);
        mbedtls_entropy_free(&entropy);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        return;
    }

    // Encode the signature into Base64 URL-safe format
    base64_url_encode(signature_base64, 1024, &signature_base64_len, signature, signature_len);     // Base64 URL encode for signature

    // Form the final JWT
    snprintf((char*)jwt, sizeof(jwt), "%s.%s.%s", header_base64, payload_base64, signature_base64);        // Form JWT of format <header>.<payload>.<signature>
    
    // Clean up mbedtls (cryptographic) contexts
    mbedtls_pk_free(&pk);
    mbedtls_entropy_free(&entropy);
    mbedtls_ctr_drbg_free(&ctr_drbg);   
}
#endif