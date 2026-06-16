import rsa
import hashlib
from Crypto.Cipher import AES
import json
import os

with open("test.json", "r", encoding="utf-8") as f:
    words_dict = json.load(f)
    
def get_words_by_class(word_class):
    return words_dict.get(word_class, [])


def gen_keys() -> tuple:
    (public_key, private_key) = rsa.newkeys(1024)
    
    public_key = hex_to_seed(hex(public_key.n)[2:], get_words_by_class(get_words_by_class("keys_words")))
    private_key_d = hex_to_seed(hex(private_key.d)[2:], get_words_by_class("keys_words"))
    private_key_p = hex_to_seed(hex(private_key.p)[2:], get_words_by_class("keys_words"))
    private_key_q = hex_to_seed(hex(private_key.q)[2:], get_words_by_class("keys_words"))

    return public_key, private_key_d, private_key_p, private_key_q

def get_fingerprint(public_seed):
    public_key = rsa.PublicKey(int(seed_to_hex(public_seed, get_words_by_class("keys_words")), 16), 65537)
    key_hash = hashlib.md5(public_key.n.to_bytes((public_key.n.bit_length() + 7) // 8, 'big')).digest()
    fingerprint = "".join([f"{byte:02x}" for byte in key_hash])
    fingerprint_sep = ":".join([f"{byte:02x}" for byte in key_hash])
    
    return hex_to_seed(fingerprint, get_words_by_class("keys_words")), fingerprint_sep

def hex_to_seed(hex_word: str, dictionary: list) -> str:
    hex_nums = [hex_word[i: i + 2] for i in range(0, len(hex_word), 2)]
    seed_key_words = []
    
    for num in hex_nums:
        seed_word = dictionary[int(num, 16)]
        seed_key_words.append(seed_word)
        
    return ' '.join(word for word in seed_key_words)


def seed_to_hex(seed: str, dictionary: list) -> str:
    hexx = ''
    
    for word in seed.split():
        hex_num = hex(dictionary.index(word))[2:]
        if len(hex_num) < 2:
            hex_num = '0' + hex_num
        hexx += hex_num
        
    return hexx


def keys_from_seed(public_seed, private_d, private_p, private_q):
    private_key = None
    
    if private_d and private_p and private_q:
        private_d = seed_to_hex(private_d, get_words_by_class("keys_words"))
        private_p = seed_to_hex(private_p, get_words_by_class("keys_words"))
        private_q = seed_to_hex(private_q, get_words_by_class("keys_words"))
        private_key = rsa.PrivateKey(int(seed_to_hex(public_seed, get_words_by_class("keys_words")), 16), 65537,
                                     int(private_d, 16), int(private_p, 16), int(private_q, 16))
    public_key = rsa.PublicKey(int(seed_to_hex(public_seed, get_words_by_class("keys_words")), 16), 65537)
    
    return public_key, private_key


def encrypt_message(message, public_key):
    splitter = b'\x00\x53\x48\x00' # genering with time_key
    aes_key = rsa.randnum.read_random_bits(256)
    
    if type(message) == str:
        message = message.encode('utf-8')
    
    aes_cipher = AES.new(aes_key, AES.MODE_EAX)
    aes_nonce = aes_cipher.nonce
    aes_ciphertext, aes_tag = aes_cipher.encrypt_and_digest(message)
    encrypted_aes_key = rsa.encrypt(aes_key, public_key)
    
    transmitted_message = encrypted_aes_key + splitter + aes_nonce + splitter + aes_ciphertext
    
    crypto = hex_to_seed(transmitted_message.hex(), get_words_by_class("ciphertext_curse"))
    
    return crypto


def decrypt_message(crypto, private_key, utf_encoded: bool = True):
    splitter = b'\x00\x53\x48\x00'
    
    crypto_hex = seed_to_hex(crypto, get_words_by_class("ciphertext_curse"))
    
    crypto = bytes.fromhex(crypto_hex).split(b'\x00\x53\x48\x00')
    
    aes_key = rsa.decrypt(crypto[0], private_key)
    cipher = AES.new(aes_key, AES.MODE_EAX, nonce=crypto[1])
    message = cipher.decrypt(crypto[2])
    
    if utf_encoded:
        message = message.decode('utf-8')
        
    return message



def socket_encrypt(message, public_seed: str):
    public_key = rsa.PublicKey(int(seed_to_hex(public_seed, get_words_by_class("keys_words")), 16), 65537)
    return encrypt_message(message, public_key)


def socket_decrypt(crypto, public_seed: str, private_seeds: list[str]):
    _, private_key = keys_from_seed(public_seed, private_seeds[0], private_seeds[1], private_seeds[2])
    return decrypt_message(crypto, private_key)


if __name__ == "__main__":
    # if os.path.isfile('public.nik') & os.path.isfile('private.nik'):
    #     with open('public.nik', 'r', encoding='utf-8') as p:
    #         my_public = p.read()
    #     with open('private.nik', 'r', encoding='utf-8') as v:
    #         my_private = v.read().split('\n')
    # public_key, private_key = keys_from_seed(my_public, my_private[0], my_private[1], my_private[2])
    # crypto = encrypt_message(input(), public_key)
    # crypto_len = len(crypto) // 2
    # # print(crypto_len)
    # start_point = 0
    # points = [4, 5, 8]
    # decrypted = ''
    # while start_point <= crypto_len:
    #     for i in points:
    #         start_point += i
    #         if start_point <= crypto_len:
    #             if ord(crypto[start_point]) > 1085:
    #                 decrypted += str((1103 - ord(crypto[start_point])) % 3)
    #             elif ord(crypto[start_point]) == 32:
    #                 if points[0] == i:
    #                     decrypted += '*'
    #                 elif points[1] == i:
    #                     decrypted += '&'
    #                 else:
    #                     decrypted += '^'
    #             else:
    #                 decrypted += crypto[start_point]
    # print(decrypted)
    # msg = decrypt_message(crypto, private_key)
    # # print(crypto)
    # print(msg)
    # # Below is an example
    
    public_key, private_key_d, private_key_p, private_key_q = gen_keys()
    public_key, private_key = keys_from_seed(public_key, private_key_d, private_key_p, private_key_q)
    crypto = encrypt_message(input(), public_key)
    msg = decrypt_message(crypto, private_key)
    print(msg)
    