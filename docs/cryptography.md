### Opcode: `crypto_sign`
#### Parameters
`string message string secret_key`
#### Description
Signs `message` given `secret_key` and returns the signature using the Ed25519 algorithm.  Note that `message` is not included in the `signature`.  The `system` opcode using the command "sign_key_pair" can be used to create a public/secret key pair.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(seq
	(declare
		(zip
			["public_sign_key" "secret_sign_key"]
			(system "sign_key_pair")
		)
	)
	(declare
		{message "hello"}
	)
	(declare
		{
			signature (crypto_sign message secret_sign_key)
		}
	)
	(concat
		"valid signature: "
		(crypto_sign_verify message public_sign_key signature)
		"\n"
	)
)
```
Output:
```amalgam
"valid signature: .true\n"
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `crypto_sign_verify`
#### Parameters
`string message string public_key string signature`
#### Description
Verifies that `message` was signed with the signature via the public key using the Ed25519 algorithm and returns true if the signature is valid, false otherwise.  Note that `message` is not included in the `signature`.  The `system` opcode using the command "sign_key_pair" can be used to create a public/secret key pair.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(seq
	(declare
		(zip
			["public_sign_key" "secret_sign_key"]
			(system "sign_key_pair")
		)
	)
	(declare
		{message "hello"}
	)
	(declare
		{
			signature (crypto_sign message secret_sign_key)
		}
	)
	(concat
		"valid signature: "
		(crypto_sign_verify message public_sign_key signature)
		"\n"
	)
)
```
Output:
```amalgam
"valid signature: .true\n"
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `encrypt`
#### Parameters
`string plaintext_message string key1 [string nonce] [string key2]`
#### Description
If `key2` is not provided, then it uses the XSalsa20 algorithm to perform shared secret key encryption on the `message`, returning the encrypted value.  If `key2` is provided, then the Curve25519 algorithm will additionally be used, and `key1` will represent the receiver's public key and `key2` will represent the sender's secret key.  The `nonce` is a string of bytes up to 24 bytes long, that will be used to randomize the encryption, and will need to be provided to the decryption in order to work.  Nonces are not technically required, but strongly recommended to prevent replay attacks.  The `system` opcode using the command "encrypt_key_pair" can be used to create a public/secret key pair.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(seq
	(declare
		(zip
			["public_encrypt_key" "secret_encrypt_key"]
			(system "encrypt_key_pair")
		)
	)
	(declare
		{
			encrypted (encrypt message secret_encrypt_key "1234")
		}
	)
	(concat
		"decrypted: "
		(decrypt encrypted secret_encrypt_key "1234")
		"\n"
	)
)
```
Output:
```amalgam
"decrypted: \n"
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `decrypt`
#### Parameters
`string cyphertext_message string key1 [string nonce] [string key2]`
#### Description
If `key2` is not provided, then it uses the XSalsa20 algorithm to perform shared secret key decryption on the `message`, returning the encrypted value.  If `key2` is provided, then the Curve25519 algorithm will additionally be used, and `key1` will represent the sender's public key and `key2` will represent the receiver's secret key.  The `nonce` is a string of bytes up to 24 bytes long, that will be used to randomize the encryption, and will need to be provided to the decryption in order to work.  Nonces are not technically required, but strongly recommended to prevent replay attacks.  The `system` opcode using the command "encrypt_key_pair" can be used to create a public/secret key pair.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(seq
	(declare
		(zip
			["public_encrypt_key" "secret_encrypt_key"]
			(system "encrypt_key_pair")
		)
	)
	(declare
		{
			encrypted (encrypt message secret_encrypt_key "1234")
		}
	)
	(concat
		"decrypted: "
		(decrypt encrypted secret_encrypt_key "1234")
		"\n"
	)
)
```
Output:
```amalgam
"decrypted: \n"
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

