/**
 * SHURIUM Mobile Wallet - Cryptographic Utilities
 * Key derivation, address generation, and cryptographic operations
 */

import { Buffer } from 'buffer';
import * as bip39 from 'bip39';
import { NetworkType } from '../types';

// SHURIUM address prefixes by network
const ADDRESS_PREFIXES: Record<NetworkType, { pubkey: number; script: number }> = {
  mainnet: { pubkey: 0x3f, script: 0x7f }, // SHR prefix
  testnet: { pubkey: 0x6f, script: 0xc4 }, // tSHR prefix  
  regtest: { pubkey: 0x6f, script: 0xc4 }, // Same as testnet
};

// BIP44 coin type for SHURIUM
const SHURIUM_COIN_TYPE = 0x8000072f; // 1839 - registered coin type

/**
 * Generate a new BIP39 mnemonic phrase
 * @param strength - Entropy bits (128 = 12 words, 256 = 24 words)
 */
export function generateMnemonic(strength: 128 | 256 = 256): string {
  return bip39.generateMnemonic(strength);
}

/**
 * Validate a BIP39 mnemonic phrase
 */
export function validateMnemonic(mnemonic: string): boolean {
  return bip39.validateMnemonic(mnemonic);
}

/**
 * Convert mnemonic to seed
 * @param mnemonic - BIP39 mnemonic phrase
 * @param passphrase - Optional passphrase for additional security
 */
export async function mnemonicToSeed(mnemonic: string, passphrase?: string): Promise<Buffer> {
  return Buffer.from(await bip39.mnemonicToSeed(mnemonic, passphrase));
}

/**
 * Get derivation path for SHURIUM
 * BIP44: m/44'/coin_type'/account'/change/address_index
 */
export function getDerivationPath(
  accountIndex: number = 0,
  change: boolean = false,
  addressIndex: number = 0
): string {
  const changeValue = change ? 1 : 0;
  return `m/44'/${SHURIUM_COIN_TYPE}'/${accountIndex}'/${changeValue}/${addressIndex}`;
}

/**
 * Base58Check encoding (Bitcoin-style)
 */
export function base58CheckEncode(payload: Buffer, version: number): string {
  const ALPHABET = '123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz';
  
  // Prepend version byte
  const versionedPayload = Buffer.concat([Buffer.from([version]), payload]);
  
  // Calculate checksum (double SHA256, take first 4 bytes)
  const checksum = doubleSha256(versionedPayload).slice(0, 4);
  
  // Append checksum
  const data = Buffer.concat([versionedPayload, checksum]);
  
  // Convert to Base58
  let num = BigInt('0x' + data.toString('hex'));
  let result = '';
  
  while (num > 0n) {
    const remainder = Number(num % 58n);
    num = num / 58n;
    result = ALPHABET[remainder] + result;
  }
  
  // Add leading '1's for leading zero bytes
  for (let i = 0; i < data.length && data[i] === 0; i++) {
    result = '1' + result;
  }
  
  return result;
}

/**
 * Base58Check decoding
 */
export function base58CheckDecode(address: string): { version: number; payload: Buffer } | null {
  const ALPHABET = '123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz';
  
  // Convert from Base58
  let num = 0n;
  for (const char of address) {
    const index = ALPHABET.indexOf(char);
    if (index === -1) return null;
    num = num * 58n + BigInt(index);
  }
  
  // Convert to bytes
  let hex = num.toString(16);
  if (hex.length % 2) hex = '0' + hex;
  const data = Buffer.from(hex, 'hex');
  
  // Account for leading zeros
  const leadingZeros = address.split('').findIndex(c => c !== '1');
  const fullData = Buffer.concat([Buffer.alloc(leadingZeros), data]);
  
  // Verify checksum
  if (fullData.length < 5) return null;
  
  const payload = fullData.slice(0, -4);
  const checksum = fullData.slice(-4);
  const expectedChecksum = doubleSha256(payload).slice(0, 4);
  
  if (!checksum.equals(expectedChecksum)) return null;
  
  return {
    version: payload[0],
    payload: payload.slice(1),
  };
}

/**
 * Double SHA256 hash
 */
export function doubleSha256(data: Buffer): Buffer {
  // Note: In production, use react-native-quick-crypto for performance
  // This is a placeholder that will be replaced with native implementation
  const crypto = require('crypto');
  const first = crypto.createHash('sha256').update(data).digest();
  return crypto.createHash('sha256').update(first).digest();
}

/**
 * RIPEMD160(SHA256(data))
 */
export function hash160(data: Buffer): Buffer {
  const crypto = require('crypto');
  const sha256 = crypto.createHash('sha256').update(data).digest();
  return crypto.createHash('ripemd160').update(sha256).digest();
}

/**
 * Convert public key to SHURIUM address
 */
export function publicKeyToAddress(publicKey: Buffer, network: NetworkType): string {
  const prefix = ADDRESS_PREFIXES[network];
  const pubKeyHash = hash160(publicKey);
  return base58CheckEncode(pubKeyHash, prefix.pubkey);
}

/**
 * Validate a SHURIUM address
 */
export function validateAddress(address: string, network: NetworkType): boolean {
  const decoded = base58CheckDecode(address);
  if (!decoded) return false;
  
  const prefix = ADDRESS_PREFIXES[network];
  return decoded.version === prefix.pubkey || decoded.version === prefix.script;
}

/**
 * Parse a SHURIUM URI (shurium:address?amount=X&label=Y)
 */
export function parseShuriumURI(uri: string): {
  address: string;
  amount?: number;
  label?: string;
  message?: string;
} | null {
  try {
    // Handle both 'shurium:' and 'SHURIUM:' prefixes
    const normalized = uri.toLowerCase();
    if (!normalized.startsWith('shurium:')) {
      return null;
    }
    
    const withoutPrefix = uri.slice(8); // Remove 'shurium:'
    const [address, queryString] = withoutPrefix.split('?');
    
    const result: { address: string; amount?: number; label?: string; message?: string } = {
      address,
    };
    
    if (queryString) {
      const params = new URLSearchParams(queryString);
      
      const amount = params.get('amount');
      if (amount) {
        result.amount = parseFloat(amount);
      }
      
      const label = params.get('label');
      if (label) {
        result.label = decodeURIComponent(label);
      }
      
      const message = params.get('message');
      if (message) {
        result.message = decodeURIComponent(message);
      }
    }
    
    return result;
  } catch {
    return null;
  }
}

/**
 * Create a SHURIUM URI
 */
export function createShuriumURI(
  address: string,
  amount?: number,
  label?: string,
  message?: string
): string {
  let uri = `shurium:${address}`;
  const params: string[] = [];
  
  if (amount !== undefined && amount > 0) {
    params.push(`amount=${amount}`);
  }
  if (label) {
    params.push(`label=${encodeURIComponent(label)}`);
  }
  if (message) {
    params.push(`message=${encodeURIComponent(message)}`);
  }
  
  if (params.length > 0) {
    uri += '?' + params.join('&');
  }
  
  return uri;
}

/**
 * Format amount from satoshis to SHR
 */
export function formatSHR(satoshis: number, decimals: number = 8): string {
  const shr = satoshis / 100000000;
  return shr.toFixed(decimals);
}

/**
 * Parse SHR amount to satoshis
 */
export function parseSHR(shr: string | number): number {
  const value = typeof shr === 'string' ? parseFloat(shr) : shr;
  return Math.round(value * 100000000);
}

/**
 * Generate a random hex string
 */
export function randomHex(bytes: number): string {
  const crypto = require('crypto');
  return crypto.randomBytes(bytes).toString('hex');
}
