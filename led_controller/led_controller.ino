const int redPin = 3;    // PWM pin for the red component of the LED
const int greenPin = 5;  // PWM pin for the green component of the LED
const int bluePin = 6;   // PWM pin for the blue component of the LED

bool noConnection = true;

void setup() {
  Serial.begin(115200);  // Ensure this matches the baud rate in your C++ code
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
}

void setLed(int red, int blue, int green){
  analogWrite(redPin, 255 - red);
  analogWrite(greenPin, 255 - green);
  analogWrite(bluePin, 255 - blue);
}

void loop() {
  static char buffer[10];  // Buffer to store incoming data
  static int index = 0;    // Current index in the buffer

  while (Serial.available()) {
    noConnection = false;
    char c = Serial.read();
    if (c >= '0' && c <= '9') {  // Only read numeric characters
      buffer[index++] = c;
      if (index == 9) {        // If we have a full set of RGB values
        buffer[index] = '\0';  // Null-terminate the string

        // Extract RGB values
        int r = (buffer[0] - '0') * 100 + (buffer[1] - '0') * 10 + (buffer[2] - '0');
        int g = (buffer[3] - '0') * 100 + (buffer[4] - '0') * 10 + (buffer[5] - '0');
        int b = (buffer[6] - '0') * 100 + (buffer[7] - '0') * 10 + (buffer[8] - '0');

        // Set the LED color
        setLed(r, g, b);

        // Reset buffer index for the next set of RGB values
        index = 0;
      }
    } else {
      // If we receive a non-numeric character, reset the buffer
      index = 0;
    }
  }
}
