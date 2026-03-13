import numpy as np
import wave
import matplotlib.pyplot as plt

head_width = 0.15 # m

def basic_stereo_separation(signal, trans_x, trans_y):
    """
    Basic Stereo Separation

    Parameters
    ----------
    signal : np.ndarray
        The input stereo signal (2D array with shape (num_samples, 2)).
    trans_x : float
        The the x-coordinate of the sound source in centimeters (relative to the center of the head).
    trans_y : float
        The y-coordinate of the sound source in centimeters (relative to the center of the head).


    -------

    """

    # Calculate the distance from the sound source to each ear
    left_ear_x = -head_width / 2
    right_ear_x = head_width / 2

    left_distance = np.sqrt((trans_x - left_ear_x) ** 2 + (trans_y) ** 2)
    right_distance = np.sqrt((trans_x - right_ear_x) ** 2 + (trans_y) ** 2)

    # Calculate the relative angles to each ear
    angle_relative_R_Ear = np.arccos((head_width ** 2 + right_distance ** 2 - left_distance ** 2) / (2 * head_width * right_distance))
    angle_relative_L_Ear = np.arccos((head_width ** 2 + left_distance ** 2 - right_distance ** 2) / (2 * head_width * left_distance))

    angle_difference = angle_relative_R_Ear - angle_relative_L_Ear
    # Apply the separation effect based on the angle difference
    separation_factor = np.sin(angle_difference)  # Simple separation factor based on angle difference

    # # Apply the separation factor to the input signal
    output_signal = np.zeros_like(signal)
   

    # Calculate the interaural time difference (ITD) and interaural level difference (ILD)
    speed_of_sound = 20  # Speed of sound in cm/s
    ITD = (right_distance - left_distance) / speed_of_sound  # Time difference in seconds
    ILD = 20 * np.log10(right_distance / left_distance)  # Level difference in decibels
    ITD_samples = int(ITD * 44100)  # Convert time difference to samples

    # 2 different methods to calculate the separated audio signal
   
    output_signal= apply_itd_ild(signal, ITD_samples, ILD) #Delay based seperation

    #output_signal = apply_separation(signal, separation_factor) # gain based seperation
   
    return output_signal


def apply_separation(signal, separation_factor):
    output_signal = np.zeros_like(signal)
    output_signal[:, 0] = signal[:, 0] * (1 + separation_factor) / 2  # Left channel
    output_signal[:, 1] = signal[:, 1] * (1 - separation_factor) / 2  # Right channel
    return output_signal


def apply_itd_ild(signal, ITD_samples, ILD):
    output_signal = np.zeros_like(signal)
    if (ITD_samples > 0):
        output_signal[:, 0] = np.pad(signal[:, 0], (0, ITD_samples), mode='constant')[:len(signal)]   # Left channel
        output_signal[:, 1] = np.pad(signal[:, 1], (ITD_samples, 0), mode='constant')[:len(signal)]   # Right channel
    else: 
        output_signal[:, 0] = np.pad(signal[:, 0], (-ITD_samples, 0), mode='constant')[:len(signal)]  # Left channel
        output_signal[:, 1] = np.pad(signal[:, 1], (0, -ITD_samples), mode='constant')[:len(signal)]  # Right channel
    output_signal[:, 0] *= 10 ** (ILD / 20)  # Apply ILD to left channel
    output_signal[:, 1] *= 10 ** (-ILD / 20) # Apply ILD to right channel
    output_signal = output_signal / (np.max(np.abs(output_signal)) *4)  # Normalize the output signal
    return output_signal

def Plot_channels(signal):
    plt.figure(figsize=(12, 6))
    plt.subplot(2, 1, 1)
    plt.plot(signal[:, 0])
    plt.title('Left Channel')
    plt.subplot(2, 1, 2)
    plt.plot(signal[:, 1])
    plt.title('Right Channel')
    plt.tight_layout()
    plt.show()

t = np.linspace(0, 1, 44100*2)  # 1 second of audio at 44.1 kHz
signal = np.column_stack((np.sin(2 * np.pi * 440 * t), np.sin(2 * np.pi * 440 * t)))  # Stereo signal (same in both channels)
trans_x = -40  # Sound source is X m to the right of the center
trans_y = 10   # Sound source is Y m in front of the user

output_signal = basic_stereo_separation(signal, trans_x, trans_y)

# Save the output signal to a WAV file
output_signal_int16 = np.int16(output_signal * 32767)  # Convert to 16-bit integer format

with wave.open('output_stereo_separation_TD.wav', 'w') as wav_file:
    wav_file.setnchannels(2)  # Stereo
    wav_file.setsampwidth(2)  # 16 bits per sample
    wav_file.setframerate(44100)  # Sample rate
    wav_file.writeframes(output_signal_int16.tobytes())


Plot_channels(output_signal_int16)

