This is the codebase for the audio transmitter end. The 
transmitting ESP32-H2 (H2-T) will process I2S audio data,
compress it using the esp_audio_codec, and send it via 
Bluetooth LE to the receiver (H2-R).
The esp_audio_codec had to be added to the project as a
managed component. Here are the steps (done in terminal):

Step 1: Go into the project folder that contains your 
CMakeLists.txt file and your main code folder.

Step 2: run the following command: 
idf.py add-dependency "espressif/esp_audio_codec^2.0.3"
-You should see an idf_component.yml file in your directory-

Step 3: Reconfigure your build environment, or rebuild.
idf.py reconfigure OR idf.py build
