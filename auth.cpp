#include "auth.h"
#include <base64.hpp>  // Only include here to avoid multiple definition errors

bool checkAuth(String httpRequest) {
  int authIndex = httpRequest.indexOf("Authorization: Basic ");
  if (authIndex == -1) return false;
  
  int startIndex = authIndex + 21;
  int endIndex = httpRequest.indexOf('\r', startIndex);
  if (endIndex == -1) endIndex = httpRequest.indexOf('\n', startIndex);
  
  String encodedCredentials = httpRequest.substring(startIndex, endIndex);
  encodedCredentials.trim();
  
  // Decode Base64 credentials using decode_base64
  unsigned int decodedLength = decode_base64_length((unsigned char*)encodedCredentials.c_str());
  unsigned char decodedCredentials[decodedLength + 1];
  decode_base64((unsigned char*)encodedCredentials.c_str(), decodedCredentials);
  decodedCredentials[decodedLength] = '\0';
  
  String credentials = String((char*)decodedCredentials);
  int colonIndex = credentials.indexOf(':');
  if (colonIndex == -1) return false;
  
  String username = credentials.substring(0, colonIndex);
  String password = credentials.substring(colonIndex + 1);
  
  // Check username
  if (!username.equals(AUTH_USERNAME)) return false;
  
  // Hash the provided password with salt and compare to stored hash
  // SECURITY: This implements salted SHA256 hashing to prevent rainbow table attacks
  // The salt is prepended to the password before hashing
  SHA256 sha256;
  sha256.reset();
  
  // Add salt first (prepend method)
  sha256.update((const byte*)AUTH_SALT, strlen(AUTH_SALT));
  
  // Add password
  sha256.update((const byte*)password.c_str(), password.length());
  
  byte hash[32];
  sha256.finalize(hash, 32);
  
  // Convert hash to hex string
  char hashHex[65];
  for (int i = 0; i < 32; i++) {
    sprintf(&hashHex[i * 2], "%02x", hash[i]);
  }
  hashHex[64] = '\0';
  
  // Compare with stored SHA256 hash
  return String(hashHex).equals(String(AUTH_PASSWORD_SHA256));
}