#include "AESCoder.h"

// 字符串转字节数组
void AESCoder::char_2_byte(byte* bytes, const char* str) {
    int len = std::strlen(str);
    for (int ii = 0; ii < len; ++ii) {
        bytes[ii] = static_cast<byte>(str[ii]);
    }
}

// 加密函数
std::string AESCoder::aesEncrypt(const std::string& plaintext) {
    std::string ciphertext;

    // 先将密钥进行数据类型转换
    byte key_b[CryptoPP::AES::DEFAULT_KEYLENGTH], iv_b[CryptoPP::AES::DEFAULT_KEYLENGTH];
    char_2_byte(key_b, _key), char_2_byte(iv_b, _iv);

    // 加密字节流
	CryptoPP::AES::Encryption aesEncryption(key_b, CryptoPP::AES::DEFAULT_KEYLENGTH);
	CryptoPP::CBC_Mode_ExternalCipher::Encryption cbcEncryption(aesEncryption, iv_b);
	CryptoPP::StreamTransformationFilter stfEncryptor(cbcEncryption, new CryptoPP::StringSink(ciphertext));
	stfEncryptor.Put(reinterpret_cast<const unsigned char*>(plaintext.c_str()), plaintext.length());
	stfEncryptor.MessageEnd();

    return ciphertext;
}

// 解密函数
std::string AESCoder::aesDecrypt(const std::string &ciphertext) {
    std::string decryptedText;

    // 先将密钥进行数据类型转换
    byte key_b[CryptoPP::AES::DEFAULT_KEYLENGTH], iv_b[CryptoPP::AES::DEFAULT_KEYLENGTH];
    char_2_byte(key_b, _key), char_2_byte(iv_b, _iv);

    // 解密字节流
	CryptoPP::AES::Decryption aesDecryption(key_b, CryptoPP::AES::DEFAULT_KEYLENGTH);
	CryptoPP::CBC_Mode_ExternalCipher::Decryption cbcDecryption(aesDecryption, iv_b);
	CryptoPP::StreamTransformationFilter stfDecryptor(cbcDecryption, new CryptoPP::StringSink(decryptedText));
	stfDecryptor.Put(reinterpret_cast<const unsigned char*>(ciphertext.c_str()), ciphertext.size());
	stfDecryptor.MessageEnd();

    return decryptedText;
}

const char* AESCoder::_key = "EasyPalm20220922";
const char* AESCoder::_iv  = "20220922EasyPalm";