import serial as sr
import json
import time
import numpy as np

# Configuration du port série
SERIAL_PORT = 'COM3'  # Windows: COM3, COM4, etc. | Mac/Linux: /dev/ttyUSB0, /dev/ttyACM0
BAUD_RATE = 115200

def read_tof_data():
    """Lit les données du capteur ToF depuis Arduino"""
    try:
        ser = sr.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"✓ Connecté à {SERIAL_PORT}")
        time.sleep(2)  # Attendre l'initialisation Arduino
        FLOOR_DISTANCE = 2200  # distance plafond-sol (mm)
        PRESENCE_THRESHOLD = 300 #taille pour activation d'une zone
        MIN_ACTIVE_CELLS = 3 #sur les 8 cellules comprises dans la Zone A ou B
        number_of_people = 0
        state = "nothing"
        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                
                # Ignore les messages de debug
                if not line.startswith('{'):
                    continue
                
                try:
                    data = json.loads(line)
                    
                    # Si c'est un message de status
                    if 'status' in data:
                        print(f"Arduino status: {data['status']}")
                        continue
                    
                    # Si c'est une matrice de distances
                    if 'matrix' in data:
                        timestamp = data['timestamp']
                        distances = np.array(data['matrix']).reshape(4,4)
                        
                        print(f"\n=== Frame @ {timestamp} ms ===")
                        
                        
                        # Matrice distance → hauteur
                        height = FLOOR_DISTANCE - distances

                        # Zones
                        zone_a = height[:2, :]     # haut
                        zone_b = height[2:, :]     # bas
                        print("zone_a")
                        print(zone_a)
                        print("zone_b")
                        print(zone_b)
                        zone_a_active = np.sum(zone_a > PRESENCE_THRESHOLD) >= MIN_ACTIVE_CELLS
                        zone_b_active = np.sum(zone_b > PRESENCE_THRESHOLD) >= MIN_ACTIVE_CELLS
                        print("Zone A:", zone_a_active, "Zone B:", zone_b_active)
                        activation = (zone_a_active, zone_b_active)
                        # Détection Entrée / Sortie
                        event = None
                        match activation:
                            case (1,0):
                                match state:
                                    case "nothing":
                                        state = "A First"
                                    case "B First":
                                        event = "SORTIE"
                                        number_of_people -=1
                            case (0,0):
                                state = "nothing"
                            case (0,1):
                                match state:
                                    case "nothing":
                                        state = "B First"
                                    case "A First":
                                        event = "ENTREE"
                                        number_of_people +=1


                                        
                            
                            
                            
                            

                        # Résultat
                        if event:
                            print(f"🚶 {event}")
                            print(f"nombre de personnes : {number_of_people}")

                        # 🔥 ICI : Traite tes données
                        # Exemples :
                        # - Sauvegarder dans un fichier CSV
                        # - Envoyer vers un serveur web
                        # - Afficher une heatmap
                        # - Détecter des objets
                        
                except json.JSONDecodeError:
                    print(f"⚠️ JSON invalide: {line}")
                    
    except sr.SerialException as e:
        print(f"❌ Erreur série: {e}")
        print("\nVérifie:")
        print("1. Le port COM (Windows) ou /dev/ttyUSB0 (Linux)")
        print("2. Que l'Arduino est bien branché")
        print("3. Que le port n'est pas utilisé par l'IDE Arduino")
    except KeyboardInterrupt:
        print("\n\n⏹️ Arrêt du programme")
        ser.close()

if __name__ == "__main__":
    print("🚀 Lecture des données VL53L5CX...")
    read_tof_data()