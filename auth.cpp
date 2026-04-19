#include "auth.h"
#include <base64.hpp>  // Only include here to avoid multiple definition errors

bool checkAuth(String httpRequest) {
  int authIndex = httpRequest.indexOf("Authorization: Basic ");
  if (authIndex == -1) return false;

  int startIndex = authIndex + 21;
  int endIndex = httpRequest.indexOf('\r', startIndex);
  if (endIndex == -1) endIndex = httpRequest.indexOf('\n', startIndex);

  // Copy encoded credentials into a fixed char buffer — no heap String allocation
  char encodedBuf[128];
  int encLen = endIndex - startIndex;
  if (encLen <= 0 || encLen >= (int)sizeof(encodedBuf)) return false;
  httpRequest.substring(startIndex, endIndex).toCharArray(encodedBuf, sizeof(encodedBuf));

  // Trim trailing whitespace in-place
  int trimIdx = strlen(encodedBuf) - 1;
  while (trimIdx >= 0 && (encodedBuf[trimIdx] == ' ' || encodedBuf[trimIdx] == '\r' || encodedBuf[trimIdx] == '\n')) {
    encodedBuf[trimIdx--] = '\0';
  }

  // Decode Base64 into a fixed buffer — no heap allocation
  unsigned int decodedLength = decode_base64_length((unsigned char*)encodedBuf);
  if (decodedLength > 63) return false;

  unsigned char decodedBuf[64];
  decode_base64((unsigned char*)encodedBuf, decodedBuf);
  decodedBuf[decodedLength] = '\0';

  // Find the colon separator without allocating Strings
  char* colonPtr = strchr((char*)decodedBuf, ':');
  if (colonPtr == NULL) return false;

  // Split into username and password using pointer arithmetic
  *colonPtr = '\0';
  const char* username = (char*)decodedBuf;
  const char* password = colonPtr + 1;

  // Check username with constant-time-safe strcmp (username is not secret, this is fine)
  if (strcmp(username, AUTH_USERNAME) != 0) return false;

  // Hash salt+password with SHA256 and compare to stored hash
  SHA256 sha256;
  sha256.reset();
  sha256.update((const byte*)AUTH_SALT, strlen(AUTH_SALT));
  sha256.update((const byte*)password, strlen(password));

  byte hash[32];
  sha256.finalize(hash, 32);

  // Convert hash to hex string in a fixed buffer
  char hashHex[65];
  for (int i = 0; i < 32; i++) {
    sprintf(&hashHex[i * 2], "%02x", hash[i]);
  }
  hashHex[64] = '\0';

  return (strcmp(hashHex, AUTH_PASSWORD_SHA256) == 0);
}
