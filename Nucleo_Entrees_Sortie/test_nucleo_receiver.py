import serial as sr
import time

SERIAL_PORT = 'COM3'
BAUD_RATE = 115200

START = 0xAA
END   = 0x55

def read_tof_data():
    try:
        ser = sr.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"✓ Connecté à {SERIAL_PORT}")
        time.sleep(2)

        number_of_people = 0
        state = "nothing"

        while True:
            byte = ser.read(1)
            if len(byte) == 0:
                continue

            # Attente du START BYTE
            if byte[0] == START:
                a = ser.read(1)
                b = ser.read(1)
                end = ser.read(1)

                # Vérifier que le frame est valide
                if len(a) and len(b) and len(end) and end[0] == END:

                    a_result = a[0]
                    b_result = b[0]

                    activation = (a_result, b_result)
                    event = None

                    match activation:
                        case (1,0):
                            if state == "nothing":
                                state = "A First"
                            elif state == "B First":
                                event = "SORTIE"
                                number_of_people -= 1
                                state = "nothing"

                        case (0,1):
                            if state == "nothing":
                                state = "B First"
                            elif state == "A First":
                                event = "ENTREE"
                                number_of_people += 1
                                state = "nothing"

                        case (0,0):
                            state = "nothing"

                    if event:
                        print(f"🚶 {event}")
                        print(f"👥 Nombre de personnes : {number_of_people}")

    except sr.SerialException as e:
        print(f"❌ Erreur série: {e}")
    except KeyboardInterrupt:
        print("\n⏹️ Arrêt du programme")
        ser.close()

if __name__ == "__main__":
    print("🚀 Lecture VL53L5CX binaire...")
    read_tof_data()
