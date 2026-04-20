#include "auth.h"
#include <base64.hpp>  // Only include here to avoid multiple definition errors

bool checkAuth(const char* authLine) {
  // Find "Authorization: Basic " in the raw header line
  const char* marker = strstr(authLine, "Authorization: Basic ");
  if (!marker) return false;

  const char* encoded = marker + 21;

  // Find end of the Base64 token (stop at \r, \n, or \0)
  uint8_t encodedLen = 0;
  while (encoded[encodedLen] && encoded[encodedLen] != '\r' && encoded[encodedLen] != '\n') {
    encodedLen++;
  }
  if (encodedLen == 0) return false;

  // Decode Base64 into a fixed buffer
  // Base64 expands at 4:3 ratio; 88 encoded chars → 66 decoded (plenty for user:pass)
  char encodedBuf[88];
  if (encodedLen >= sizeof(encodedBuf)) return false;
  memcpy(encodedBuf, encoded, encodedLen);
  encodedBuf[encodedLen] = '\0';

  unsigned int decodedLength = decode_base64_length((unsigned char*)encodedBuf);
  if (decodedLength >= 64) return false;   // Sanity check — reject oversized credentials

  unsigned char decodedBuf[64];
  decode_base64((unsigned char*)encodedBuf, decodedBuf);
  decodedBuf[decodedLength] = '\0';

  // Split "username:password" at the colon
  char* colon = strchr((char*)decodedBuf, ':');
  if (!colon) return false;
  *colon = '\0';                          // Terminate username in-place
  const char* username = (char*)decodedBuf;
  const char* password = colon + 1;

  // Check username
  if (strcmp(username, AUTH_USERNAME) != 0) return false;

  // Hash salt+password with SHA256
  SHA256 sha256;
  sha256.reset();
  sha256.update((const byte*)AUTH_SALT, strlen(AUTH_SALT));
  sha256.update((const byte*)password,  strlen(password));

  byte hash[32];
  sha256.finalize(hash, 32);

  // Convert hash bytes to hex string in a fixed buffer
  char hashHex[65];
  for (int i = 0; i < 32; i++) {
    sprintf(&hashHex[i * 2], "%02x", hash[i]);
  }
  hashHex[64] = '\0';

  return (strcmp(hashHex, AUTH_PASSWORD_SHA256) == 0);
}
