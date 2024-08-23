#pragma once

//project headers:
#include "PlatformSpecific.h"

//system headers:
#include <string>

//generates a public and secret key for signing, returned in that order
std::pair<std::string, std::string> GenerateSignatureKeyPair();

//generates a public and secret key for encryption, returned in that order
std::pair<std::string, std::string> GenerateEncryptionKeyPair();

//returns the signature for the given message and secret_key
std::string SignMessage(std::string &message, std::string &secret_key);

//returns true if the signature is valid for the message given the public key
bool IsSignatureValid(std::string &message, std::string &public_key, std::string &signature);

//returns an encrypted form of the message plaintext secret key and nonce (of up to 24 bytes)
// nonce will be resized and padded with 0s if not the right size
std::string EncryptMessage(std::string &plaintext, std::string &secret_key, std::string &nonce);

//returns an decrypted form of the message cyphertext given secret key and nonce (of up to 24 bytes)
// nonce will be resized and padded with 0s if not the right size
std::string DecryptMessage(std::string &cyphertext, std::string &secret_key, std::string &nonce);

//returns an encrypted form of the message plaintext given public and secret keys and nonce (of up to 24 bytes)
// nonce will be resized and padded with 0s if not the right size
std::string EncryptMessage(std::string &plaintext,
							std::string &receiver_public_key, std::string &sender_secret_key, std::string &nonce);

//returns an decrypted form of the message cyphertext given public and secret keys and nonce (of up to 24 bytes)
// nonce will be resized and padded with 0s if not the right size
std::string DecryptMessage(std::string &cyphertext,
							std::string &sender_public_key, std::string &receiver_secret_key, std::string &nonce);
