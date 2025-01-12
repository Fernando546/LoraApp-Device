# ESP32 LoRa-BLE Communication Device  

This ESP32-based device bridges communication between an Android application and a LoRa network. It receives and sends encrypted messages via LoRa, while maintaining a BLE connection to exchange data with the Android app. The device uses a Caesar cipher for message encryption and decryption, ensuring secure communication.  

---

## Features  

- **BLE Communication:**  
  - Send and receive messages with an Android device using Bluetooth Low Energy (BLE).  

- **LoRa Communication:**  
  - Long-range, low-power wireless communication for message exchange.  

- **Encryption and Decryption:**  
  - Messages are encrypted with the Caesar cipher before LoRa transmission.  
  - Incoming LoRa messages are decrypted and verified for the intended recipient.  

- **Two-Way Communication:**  
  - **BLE → LoRa:** Messages received from BLE are encrypted and transmitted via LoRa.  
  - **LoRa → BLE:** LoRa messages are decrypted and forwarded to the Android app via BLE.  

---

## Message Handling Flow  

### 1. BLE → LoRa Transmission  

1. **Receive Message via BLE:**  
   - The device receives a text message from the connected Android app.  
2. **Encrypt the Message:**  
   - The message is encrypted using the Caesar cipher.  
3. **Construct LoRa Packet:**  
   - The message is structured into a LoRa packet with the following format:  
     ```plaintext  
     [Key]: Unique identifier to match the intended recipient.  
     [ID]: Name of the transmitting ESP32 device.  
     [Message]: Encrypted text message.  
     ```  
4. **Transmit via LoRa:**  
   - The encrypted message is sent to the LoRa network.  

---

### 2. LoRa → BLE Reception  

1. **Receive LoRa Message:**  
   - The device listens for incoming LoRa messages.  
2. **Decrypt the Message:**  
   - The encrypted message is decrypted using the Caesar cipher.  
3. **Check Recipient Key:**  
   - The "Key" field is verified to determine if the message is intended for the device.  
4. **Forward to Android App:**  
   - If the message is valid, the device sends the `ID` and decrypted `Message` via BLE to the connected Android app.  
