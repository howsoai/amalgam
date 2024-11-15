//project headers:
#include "Cryptography.h"
#include "PlatformSpecific.h"

//3rd party headers:
extern "C" {
#include "tweetnacl/tweetnacl.h"
}

//fills destination with length bytes of random data
//need to extern "C" because the tweetnacl.c library expects it
extern "C" void randombytes(unsigned char *destination, unsigned long long length)
{
	Platform_GenerateSecureRandomData(destination, length);
}

std::pair<std::string, std::string> GenerateSignatureKeyPair()
{
	unsigned char pk[crypto_sign_PUBLICKEYBYTES];
	unsigned char sk[crypto_sign_SECRETKEYBYTES];
	crypto_sign_keypair(&pk[0], &sk[0]);

	std::string pk_s(reinterpret_cast<char *>(&pk[0]), crypto_sign_PUBLICKEYBYTES);
	std::string sk_s(reinterpret_cast<char *>(&sk[0]), crypto_sign_SECRETKEYBYTES);

	return std::make_pair(pk_s, sk_s);
}

std::pair<std::string, std::string> GenerateEncryptionKeyPair()
{
	unsigned char pk[crypto_box_PUBLICKEYBYTES];
	unsigned char sk[crypto_box_SECRETKEYBYTES];
	crypto_box_keypair(&pk[0], &sk[0]);

	std::string pk_s(reinterpret_cast<char *>(&pk[0]), crypto_box_PUBLICKEYBYTES);
	std::string sk_s(reinterpret_cast<char *>(&sk[0]), crypto_box_SECRETKEYBYTES);

	return std::make_pair(pk_s, sk_s);
}

std::string SignMessage(std::string &message, std::string &secret_key)
{
	if(secret_key.size() != crypto_sign_SECRETKEYBYTES)
		return "";

	std::string signed_message(crypto_sign_BYTES + message.size(), '\0');
	//use the same type from tweetnacl
	unsigned long long signed_message_len = 0;

	crypto_sign(reinterpret_cast<unsigned char *>(&signed_message[0]),
		&signed_message_len,
		reinterpret_cast<unsigned char *>(&message[0]), message.size(),
		reinterpret_cast<unsigned char *>(&secret_key[0]));

	//extract just the signature part
	std::string signature(begin(signed_message), begin(signed_message) + crypto_sign_BYTES);
	return signature;
}

bool IsSignatureValid(std::string &message, std::string &public_key, std::string &signature)
{
	if(public_key.size() != crypto_sign_PUBLICKEYBYTES)
		return false;

	if(signature.size() != crypto_sign_BYTES)
		return false;
	
	//prepend the signature
	std::string signed_message = signature + message;

	//crypto_sign_open needs the full space of the signed message as its working space
	std::string original_message_buffer(crypto_sign_BYTES + message.size(), '\0');
	//variable to recieve the populated length (not used but crypto_sign_open needs it)
	//use the same type from tweetnacl
	unsigned long long original_message_buffer_len = 0;

	if(crypto_sign_open(
		reinterpret_cast<unsigned char *>(&original_message_buffer[0]), &original_message_buffer_len,
		reinterpret_cast<unsigned char *>(&signed_message[0]), signed_message.size(),
		reinterpret_cast<unsigned char *>(&public_key[0])) != 0)
	{
		return false;
	}

	return true;
}

std::string EncryptMessage(std::string &plaintext, std::string &secret_key, std::string &nonce)
{
	if(secret_key.size() != crypto_secretbox_KEYBYTES)
		return "";

	if(nonce.size() != crypto_secretbox_NONCEBYTES)
		nonce.resize(crypto_secretbox_NONCEBYTES, '\0');

	size_t total_len = plaintext.size() + crypto_secretbox_ZEROBYTES;
	std::vector<unsigned char> message_buffer(total_len, 0);
	for(size_t i = crypto_secretbox_ZEROBYTES; i < total_len; i++)
		message_buffer[i] = plaintext[i - crypto_secretbox_ZEROBYTES];

	std::string cypher_buffer(total_len, 0);
	crypto_secretbox(reinterpret_cast<unsigned char *>(&cypher_buffer[0]),
		&message_buffer[0], total_len,
		reinterpret_cast<unsigned char *>(&nonce[0]),
		reinterpret_cast<unsigned char *>(&secret_key[0]));

	cypher_buffer.erase(begin(cypher_buffer), begin(cypher_buffer) + crypto_secretbox_BOXZEROBYTES);
	return cypher_buffer;
}

std::string DecryptMessage(std::string &cyphertext, std::string &secret_key, std::string &nonce)
{
	if(secret_key.size() != crypto_secretbox_KEYBYTES)
		return "";

	if(nonce.size() != crypto_secretbox_NONCEBYTES)
		nonce.resize(crypto_secretbox_NONCEBYTES, '\0');

	size_t total_len = cyphertext.size() + crypto_secretbox_BOXZEROBYTES;
	std::vector<unsigned char> message_buffer(total_len, 0);
	for(size_t i = crypto_secretbox_BOXZEROBYTES; i < total_len; i++)
		message_buffer[i] = cyphertext[i - crypto_secretbox_BOXZEROBYTES];

	std::string plaintext_buffer(total_len, 0);
	if(crypto_secretbox_open(reinterpret_cast<unsigned char *>(&plaintext_buffer[0]),
		&message_buffer[0], total_len,
		reinterpret_cast<unsigned char *>(&nonce[0]),
		reinterpret_cast<unsigned char *>(&secret_key[0])))
	{
		return "";
	}

	plaintext_buffer.erase(begin(plaintext_buffer), begin(plaintext_buffer) + crypto_secretbox_ZEROBYTES);
	return plaintext_buffer;
}

std::string EncryptMessage(std::string &plaintext,
	std::string &receiver_public_key, std::string &sender_secret_key, std::string &nonce)
{
	if(receiver_public_key.size() != crypto_box_PUBLICKEYBYTES)
		return "";

	if(sender_secret_key.size() != crypto_box_SECRETKEYBYTES)
		return "";

	if(nonce.size() != crypto_box_NONCEBYTES)
		nonce.resize(crypto_box_NONCEBYTES, '\0');

	size_t total_len = plaintext.size() + crypto_box_ZEROBYTES;
	std::vector<unsigned char> message_buffer(total_len, 0);
	for(size_t i = crypto_box_ZEROBYTES; i < total_len; i++)
		message_buffer[i] = plaintext[i - crypto_box_ZEROBYTES];

	std::string cypher_buffer(total_len, 0);
	crypto_box(reinterpret_cast<unsigned char *>(&cypher_buffer[0]),
		&message_buffer[0], total_len,
		reinterpret_cast<unsigned char *>(&nonce[0]),
		reinterpret_cast<unsigned char *>(&receiver_public_key[0]),
		reinterpret_cast<unsigned char *>(&sender_secret_key[0]));

	cypher_buffer.erase(begin(cypher_buffer), begin(cypher_buffer) + crypto_box_BOXZEROBYTES);
	return cypher_buffer;
}

std::string DecryptMessage(std::string &cyphertext,
	std::string &sender_public_key, std::string &receiver_secret_key, std::string &nonce)
{
	if(sender_public_key.size() != crypto_box_PUBLICKEYBYTES)
		return "";

	if(receiver_secret_key.size() != crypto_box_SECRETKEYBYTES)
		return "";

	if(nonce.size() != crypto_box_NONCEBYTES)
		nonce.resize(crypto_box_NONCEBYTES, '\0');

	size_t total_len = cyphertext.size() + crypto_box_BOXZEROBYTES;
	std::vector<unsigned char> message_buffer(total_len, 0);
	for(size_t i = crypto_box_BOXZEROBYTES; i < total_len; i++)
		message_buffer[i] = cyphertext[i - crypto_box_BOXZEROBYTES];

	std::string plaintext_buffer(total_len, 0);
	if(crypto_box_open(reinterpret_cast<unsigned char *>(&plaintext_buffer[0]),
		&message_buffer[0], total_len,
		reinterpret_cast<unsigned char *>(&nonce[0]),
		reinterpret_cast<unsigned char *>(&sender_public_key[0]),
		reinterpret_cast<unsigned char *>(&receiver_secret_key[0])))
	{
		return "";
	}

	plaintext_buffer.erase(begin(plaintext_buffer), begin(plaintext_buffer) + crypto_box_ZEROBYTES);
	return plaintext_buffer;
}
