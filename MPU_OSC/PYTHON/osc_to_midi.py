import time
import math
from pythonosc.dispatcher import Dispatcher
from pythonosc.osc_server import BlockingOSCUDPServer
import rtmidi
import threading
import sys
import numpy as np
import matplotlib
matplotlib.use('Qt5Agg')
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from PyQt5.QtWidgets import QApplication

# MIDI setup
midiout = rtmidi.MidiOut()
available_ports = midiout.get_ports()

# Find LoopMIDI port 3
midi_port_index = None
for i, port in enumerate(available_ports):
    if "loopMIDI" in port and ("1" in port or "Port 1" in port):
        midi_port_index = i
        break
if midi_port_index is None:
    print("LoopMIDI port 3 not found. Available ports:", available_ports)
    exit(1)
midiout.open_port(midi_port_index)

# Major scale (C major as example)
MAJOR_SCALE = [0, 2, 4, 5, 7, 9, 11]  # C D E F G A B
OCTAVES = 5
NOTES_PER_OCTAVE = len(MAJOR_SCALE)
TOTAL_NOTES = NOTES_PER_OCTAVE * OCTAVES
BASE_MIDI_NOTE = 36  # C2, adjust as needed

# State for /gyr
last_angle = 0.0
# Store latest /acc y value for velocity (teapot output)
latest_acc_y = None
latest_opt_value = 1  # Start with opt=1

# Buffers for plotting
PLOT_LEN = 200
gyr_data = np.zeros((PLOT_LEN, 3))  # x, y, z
acc_data = np.zeros((PLOT_LEN, 3))
gyr_lock = threading.Lock()
acc_lock = threading.Lock()

# Map degrees to a circle of notes (now -36000 to +36000 -> 0 to TOTAL_NOTES)
def gyr_to_midi(x_deg, y_deg, z_deg):
    # x_deg: roll, y_deg: pitch, z_deg: yaw (teapot convention)
    global latest_acc_y
    # Map pitch (y_deg, -36000 to +36000) to note in scale
    pitch_norm = (y_deg + 36000) / 72000  # 0 to 1
    scale_degree = int(pitch_norm * NOTES_PER_OCTAVE) % NOTES_PER_OCTAVE
    # Map roll (x_deg, -36000 to +36000) to octave
    roll_norm = (x_deg + 36000) / 72000  # 0 to 1
    octave = int(roll_norm * OCTAVES) % OCTAVES
    midi_note = BASE_MIDI_NOTE + octave * 12 + MAJOR_SCALE[scale_degree]
    # Use latest_acc_y for velocity, scale to 0-127 (teapot output, -2000 to +2000 typical range)
    if latest_acc_y is not None:
        velocity = int(max(0, min(127, ((latest_acc_y + 2000) / 4000) * 127)))
    else:
        velocity = 112
    # Send MIDI note on channel 2 (0x91)
    note_on = [0x91, midi_note, velocity]
    note_off = [0x81, midi_note, 0]
    midiout.send_message(note_on)
    time.sleep(0.1)
    midiout.send_message(note_off)
    print(f"[MIDI] note={midi_note}, velocity={velocity}, roll={x_deg:.1f}, pitch={y_deg:.1f}, scale_degree={scale_degree}, octave={octave}")

def gyr_to_cc(x_deg, y_deg, z_deg, mode='all'):
    # Map -180 to +180 (or -1800 to +1800) to 0-127 for MIDI CC
    def map_cc(val):
        # Handle possible scaling
        if abs(val) > 360:
            val = val / 10
        return int(max(0, min(127, ((val + 18000) / 36000) * 127)))
    cc_x = map_cc(x_deg)
    cc_y = map_cc(y_deg)
    cc_z = map_cc(z_deg)
    if mode == 'all':
        # Send CC11, CC12, CC13 all on channel 1
        midiout.send_message([0xB0, 11, cc_x])  # Roll, channel 1
        midiout.send_message([0xB0, 12, cc_y])  # Pitch, channel 1
        midiout.send_message([0xB0, 13, cc_z])  # Yaw, channel 1
        print(f"[MIDI CC] CC11={cc_x}, CC12={cc_y}, CC13={cc_z} (all ch1), roll={x_deg:.1f}, pitch={y_deg:.1f}, yaw={z_deg:.1f}")
    elif mode == 'roll':
        # Send CC11 on channel 11 (0xB0)
        midiout.send_message([0xB0, 11, cc_x])
        print(f"[MIDI CC] CC11={cc_x} (roll), channel 11, roll={x_deg:.1f}")
    elif mode == 'pitch':
        # Send CC12 on channel 12 (0xB1)
        midiout.send_message([0xB1, 12, cc_y])
        print(f"[MIDI CC] CC12={cc_y} (pitch), channel 12, pitch={y_deg:.1f}")
    elif mode == 'yaw':
        # Send CC13 on channel 13 (0xB2)
        midiout.send_message([0xB2, 13, cc_z])
        print(f"[MIDI CC] CC13={cc_z} (yaw), channel 13, yaw={z_deg:.1f}")

def update_gyr_plot(new_x, new_y, new_z):
    with gyr_lock:
        global gyr_data
        gyr_data = np.roll(gyr_data, -1, axis=0)
        gyr_data[-1] = [new_x, new_y, new_z]

def update_acc_plot(new_x, new_y, new_z):
    with acc_lock:
        global acc_data
        acc_data = np.roll(acc_data, -1, axis=0)
        acc_data[-1] = [new_x, new_y, new_z]

def plot_window():
    app = QApplication(sys.argv)
    fig, axs = plt.subplots(2, 1, figsize=(8, 6))
    axs[0].set_title('Gyroscope (gyr)')
    axs[1].set_title('Accelerometer (acc)')
    lines_gyr = [axs[0].plot([], [], label=lbl)[0] for lbl in ['x', 'y', 'z']]
    lines_acc = [axs[1].plot([], [], label=lbl)[0] for lbl in ['x', 'y', 'z']]
    axs[0].legend()
    axs[1].legend()
    axs[0].set_ylim(-36000, 36000)
    axs[1].set_ylim(-36000, 36000)
    axs[0].set_xlim(0, PLOT_LEN)
    axs[1].set_xlim(0, PLOT_LEN)

    mode_text = fig.text(0.1, 0.99, "", ha='left', va='top', fontsize=10, color='green')

    def get_mode_string(opt):
        if opt == 1:
            return 'Mode 1: MIDI Notes only'
        elif opt == 2:
            return 'Mode 2: MIDI Notes + MIDI CC (all axes)'
        elif opt == 3:
            return 'Mode 3: MIDI CC (roll/x only, channel 1)'
        elif opt == 4:
            return 'Mode 4: MIDI CC (pitch/y only, channel 2)'
        elif opt == 5:
            return 'Mode 5: MIDI CC (yaw/z only, channel 3)'
        else:
            return f'Mode {opt}: Unknown'

    def animate(frame):
        with gyr_lock:
            for i, line in enumerate(lines_gyr):
                line.set_data(np.arange(PLOT_LEN), gyr_data[:, i])
        with acc_lock:
            for i, line in enumerate(lines_acc):
                line.set_data(np.arange(PLOT_LEN), acc_data[:, i])
        # Update /opt value display
        if latest_opt_value is not None:
            mode_text.set_text(get_mode_string(latest_opt_value))
        else:
            mode_text.set_text('Mode: Unknown')
        return lines_gyr + lines_acc + [mode_text]

    for ax in axs:
        ax.set_xlabel('Samples')
        ax.set_ylabel('Value')
    ani = FuncAnimation(fig, animate, interval=50, blit=False)
    plt.tight_layout()
    plt.show()
    app.exec_()

# Patch OSC handlers to update plots
def handle_gyr(address, *args):
    print(f"[OSC]Gyr: {args}")
    if len(args) >= 3:
        update_gyr_plot(args[0], args[1], args[2])
        opt = latest_opt_value if latest_opt_value is not None else 1
        if opt == 1:
            gyr_to_midi(args[0], args[1], args[2])
        elif opt == 2:
            gyr_to_midi(args[0], args[1], args[2])
            gyr_to_cc(args[0], args[1], args[2], mode='all')
        elif opt == 3:
            gyr_to_cc(args[0], args[1], args[2], mode='roll')
        elif opt == 4:
            gyr_to_cc(args[0], args[1], args[2], mode='pitch')
        elif opt == 5:
            gyr_to_cc(args[0], args[1], args[2], mode='yaw')

def handle_acc(address, *args):
    global latest_acc_y
    print(f"[OSC] Acc: {args}")
    if len(args) >= 3:
        update_acc_plot(args[0], args[1], args[2])
        latest_acc_y = args[1]

def handle_opt(address, *args):
    global latest_opt_value
    if args:
        latest_opt_value = args[0]
        print(f"[OSC] /opt: {latest_opt_value}")

def osc_server_thread():
    dispatcher = Dispatcher()
    dispatcher.map("/gyr", handle_gyr)
    dispatcher.map("/acc", handle_acc)
    dispatcher.map("/opt", handle_opt)
    ip = "0.0.0.0"
    port = 8000  # Must match ESP32 sender
    server = BlockingOSCUDPServer((ip, port), dispatcher)
    print(f"Listening for OSC on {ip}:{port}")
    server.serve_forever()

def main():
    # Start OSC server in a separate thread
    osc_thread = threading.Thread(target=osc_server_thread, daemon=True)
    osc_thread.start()
    # Start plot window in main thread
    plot_window()

if __name__ == "__main__":
    main()
