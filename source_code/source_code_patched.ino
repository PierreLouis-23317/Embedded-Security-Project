// Minimal password prompt (Arduino) with explicit early-exit char-by-char compare
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Crypto.h>
#include <SHA3.h>

const char PASSWORD[] = "f7-@Jp0w";
//const char PASSWORD[] = "password";
const char *secret = "Je suis une petite tortue";
const int salt_size = 4;

//const uint8_t SALT[] = { 0x41, 0x42, 0x43, 0x44 }; 
// Buffer for digest (32 bytes for SHA3-256)
uint8_t digest[32];

// choose pins: RX is the pin Arduino listens on (connect TX of other device here)
// TX is the pin Arduino transmits on (connect RX of other device here)
const uint8_t MY_RX_PIN = 10;
const uint8_t MY_TX_PIN = 11;

SoftwareSerial comms(MY_RX_PIN, MY_TX_PIN); // RX, TX

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  Serial.println();
  Serial.println("Welcome to the vault. This is not the main entrance.");
  comms.begin(9600);          // software serial communications
  comms.println("Enter password:");

  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
}

void hashPassword(const char *password, const uint8_t *salt, size_t saltLen, uint8_t *outDigest) {
  SHA3_256 sha3;
  sha3.reset();

  // Add salt
  sha3.update(salt, saltLen);

  // Add password
  sha3.update((const uint8_t *)password, strlen(password));

  // Finalize
  sha3.finalize(outDigest, 32);
}

String readLine(Stream &s) {
  String str;
  while (true) {
    if (s.available()) {
      char c = s.read();
      if (c == '\r' or c == '\n') break;
      str += c;
    }
  }
  return str;
}

// PATCH (TIMING ATTACK): Compare every character without returning on the first mismatch.
bool early_exit_compare(const char *attempt) {
  const size_t pass_len = sizeof(PASSWORD) - 1;
  uint8_t mismatch = 0;
  for (size_t i = 0; i < pass_len; ++i) {
    mismatch |= (uint8_t)(attempt[i] ^ PASSWORD[i]);
  }
  // PATCH (TIMING ATTACK): Reject shorter or longer inputs without a secret-dependent early exit.
  mismatch |= (uint8_t)(strlen(attempt) != pass_len);
  return mismatch == 0;
}

// PATCH (VOLTAGE GLITCHING): Independently repeat the check in reverse order.
// A single corrupted comparison result should not be enough to grant access.
bool reverse_compare(const char *attempt) {
  const size_t pass_len = sizeof(PASSWORD) - 1;
  uint8_t mismatch = 0;
  for (size_t i = pass_len; i > 0; --i) {
    mismatch |= (uint8_t)(attempt[i - 1] ^ PASSWORD[i - 1]);
  }
  mismatch |= (uint8_t)(strlen(attempt) != pass_len);
  return mismatch == 0;
}

void protected_section(Stream &out) {
  out.println(">> ACCESS GRANTED: running protected section");

  uint8_t SALT[4];
  for (int i = 0; i < salt_size; i++) {
    SALT[i] = (uint8_t) random(0x21, 0x7E);
  }

  hashPassword(secret, SALT, sizeof(SALT), digest);

  out.print("Here is your salt: ");
  for (int i = 0; i < 4; i++) {
    if (SALT[i] < 0x10) out.print("0");
    out.print(SALT[i], HEX);
  }
  out.println();
  out.print("Here is your hash: ");
  for (int i = 0; i < 32; i++) {
    if (digest[i] < 0x10) out.print("0");
    out.print(digest[i], HEX);
  }
  out.println();
}

void loop() {
  if (comms.available()) {
    String attempt = readLine(comms);
    attempt.trim();
    if (attempt.length() == 0) {
      comms.println("No input. Enter password:");
      return;
    }

    char attempt_c[64];
    attempt.toCharArray(attempt_c, sizeof(attempt_c));

    digitalWrite(13, HIGH);
    // PATCH (VOLTAGE GLITCHING): Require two independent checks before access.
    // This increases fault tolerance but does not guarantee resistance to glitching.
    bool first_check = early_exit_compare(attempt_c);
    bool second_check = reverse_compare(attempt_c);
    if (first_check && second_check) {
      protected_section(comms);
    } else {
      comms.println(">> ACCESS DENIED");
    }
    digitalWrite(13, LOW);

    comms.println();
    comms.println("Enter password:");
  }
}
